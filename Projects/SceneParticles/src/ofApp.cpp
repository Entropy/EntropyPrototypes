#include "ofApp.h"
#include "ofxSerialize.h"
#include "Helpers.h"

//--------------------------------------------------------------
const string ofApp::kSceneName = "Particles";
const float ofApp::kHalfDim = 400.0f;
const unsigned int ofApp::kMaxLights = 16u;

//--------------------------------------------------------------
void ofApp::setup()
{
	ofDisableArbTex();
	ofSetDataPathRoot(entropy::GetSceneDataPath(kSceneName).string());
	//ofSetTimeModeFixedRate(ofGetFixedStepForFps(60));
	ofBackground(ofColor::black);	
	
	// Initialize particle system.
	environment = nm::Environment::Ptr(new nm::Environment(glm::vec3(-kHalfDim), glm::vec3(kHalfDim)));
	particleSystem.init(environment);
	photons.init(environment);
	pointLights.resize(kMaxLights);

	// Initialize transform feedback.
	feedbackBuffer.allocate(1024 * 1024 * 300, GL_STATIC_DRAW);
	auto stride = sizeof(glm::vec4) * 3;// + sizeof(glm::vec3);
	feedbackVbo.setVertexBuffer(feedbackBuffer, 4, stride, 0);
	feedbackVbo.setColorBuffer(feedbackBuffer, stride, sizeof(glm::vec4));
	feedbackVbo.setNormalBuffer(feedbackBuffer, stride, sizeof(glm::vec4) * 2);
	glGenQueries(1, &numPrimitivesQuery);

	// Init fbos
	ofFbo::Settings fboSettings;
	fboSettings.width = 1920;
	fboSettings.height = entropy::GetSceneHeight() * fboSettings.width / entropy::GetSceneWidth();
	fboSettings.internalformat = GL_RGBA32F;
	fboSettings.useDepth = true;
	fboSettings.useStencil = false;
	fboSettings.numSamples = 4;

	fboScene.allocate(fboSettings);
	fboPost.allocate(fboSettings);

	// Init post effects
	this->postEffects.resize(fboSettings.width, fboSettings.height);
	this->renderer.resize(fboSettings.width, fboSettings.height);

	// Setup renderers.
	this->renderer.setup(kHalfDim);
	this->renderer.resize(fboSettings.width, fboSettings.height);
	this->renderer.parameters.fogMaxDistance.setMax(kHalfDim * 100);
	this->renderer.parameters.fogMinDistance.setMax(kHalfDim);

	textRenderer.setup(kHalfDim);

	// Load shaders.
	shaderSettings.bindDefaults = false;
	shaderSettings.shaderFiles[GL_VERTEX_SHADER] = std::filesystem::path("shaders") / "particle.vert";
	shaderSettings.varyingsToCapture = { "out_position", "out_color", "out_normal" };
	shaderSettings.sourceDirectoryPath = std::filesystem::path("shaders");
	shaderSettings.intDefines["COLOR_PER_TYPE"] = this->parameters.rendering.colorsPerType;
	this->shader.setup(shaderSettings);

	// Setup lights.
	for (auto & light : pointLights)
	{
		light.setup();
		light.setAmbientColor(ofFloatColor::black);
		light.setSpecularColor(ofFloatColor::white);
	}

	// Add parameter listeners.
	this->eventListeners.push_back(parameters.rendering.ambientLight.newListener([&](float & ambient)
	{
		ofSetGlobalAmbientColor(ofFloatColor(ambient));
	}));
	this->eventListeners.push_back(this->parameters.rendering.colorsPerType.newListener([&](bool & colorsPerType)
	{
		shaderSettings.intDefines["COLOR_PER_TYPE"] = colorsPerType;
		this->shader.setup(shaderSettings);
	}));
	this->eventListeners.push_back(this->parameters.rendering.colorsPerType.newListener([&](bool & colorsPerType)
	{
		shaderSettings.intDefines["COLOR_PER_TYPE"] = colorsPerType;
		this->shader.setup(shaderSettings);
	}));

	// Setup the gui and timeline.
	ofxGuiSetDefaultWidth(250);
	ofxGuiSetFont("FiraCode-Light", 11, true, true, 72);
	this->gui.setup(kSceneName, "parameters.json");
	this->gui.add(this->parameters);
	this->gui.add(this->environment->parameters);
	this->gui.add(nm::Particle::parameters);
	this->gui.add(this->renderer.parameters);
	this->gui.add(this->postParams);
	this->gui.add(this->textRenderer.parameters);
	this->gui.minimizeAll();
	this->eventListeners.push_back(this->gui.savePressedE.newListener([this](void)
	{
//		if (ofGetKeyPressed(OF_KEY_SHIFT))
//		{
			auto name = ofSystemTextBoxDialog("Enter a name for the preset", "");
			if (!name.empty())
			{
				return this->savePreset(name);
			}
//		}
		return true;
	}));
	this->eventListeners.push_back(this->gui.loadPressedE.newListener([this](void)
	{
//		if (ofGetKeyPressed(OF_KEY_SHIFT))
//		{
			auto result = ofSystemLoadDialog("Select a preset folder.", true, ofToDataPath("presets", true));
			if (result.bSuccess)
			{
				this->loadPreset(result.fileName);
				ofSetWindowTitle(ofFilePath::getBaseName(result.fileName));
			}
//		}
		return true;
	}));

	this->timeline.setName("timeline");
	this->timeline.setup();
	this->timeline.setDefaultFontPath("FiraCode-Light");
	this->timeline.setOffset(glm::vec2(0, ofGetHeight() - this->timeline.getHeight()));
	//this->timeline.setSpacebarTogglePlay(false);
	this->timeline.setLoopType(OF_LOOP_NONE);
	this->timeline.setFrameRate(30.0f);
	this->timeline.setDurationInSeconds(60 * 5);
	this->timeline.setAutosave(false);
	this->timeline.setPageName(this->parameters.getName());
	this->timeline.addFlags("Cues");
	this->eventListeners.push_back(this->timeline.events().viewWasResized.newListener([this](ofEventArgs &)
	{
		this->timeline.setOffset(glm::vec2(0, ofGetHeight() - this->timeline.getHeight()));
	}));

//	const auto cameraTrackName = "Camera";
//	this->cameraTrack.setDampening(1.0f);
//	this->cameraTrack.setCamera(this->camera);
//	this->cameraTrack.setXMLFileName(this->timeline.nameToXMLName(cameraTrackName));
//	this->timeline.addTrack(cameraTrackName, &this->cameraTrack);
//	this->cameraTrack.setDisplayName(cameraTrackName);

	this->gui.setTimeline(&this->timeline);

	// Setup texture recorder.
	this->eventListeners.push_back(this->parameters.recording.recordSequence.newListener([this](bool & record)
	{
		if (record)
		{
			auto path = ofSystemLoadDialog("Record to folder:", true);
			if (path.bSuccess)
			{
				ofxTextureRecorder::Settings recorderSettings(this->fboPost.getTexture());
				recorderSettings.imageFormat = OF_IMAGE_FORMAT_JPEG;
				recorderSettings.folderPath = path.getPath();
				this->textureRecorder.setup(recorderSettings);

				this->reset();
//				this->cameraTrack.lockCameraToTrack = true;
				this->timeline.play();
			}
			else
			{
				this->parameters.recording.recordSequence = false;
			}
		}
	}));

	this->eventListeners.push_back(parameters.recording.recordVideo.newListener([this](bool & record)
	{
#if OFX_VIDEO_RECORDER
		if (record)
		{
			auto path = ofSystemSaveDialog("video.mp4", "Record to video:");
			if (path.bSuccess)
			{
				ofxTextureRecorder::VideoSettings recorderSettings(fboScene.getTexture(), 60);
				recorderSettings.videoPath = path.getPath();
				//				recorderSettings.videoCodec = "libx264";
				//				recorderSettings.extrasettings = "-preset ultrafast -crf 0";
				recorderSettings.videoCodec = "prores";
				recorderSettings.extrasettings = "-profile:v 0";
				textureRecorder.setup(recorderSettings);
			}
			else {
				this->parameters.recording.recordVideo = false;
			}
		}
		else {
			textureRecorder.stop();
		}
#endif
	}));

//	eventListeners.push_back(gui.savePressedE.newListener([this]{
//		auto saveTo = ofSystemSaveDialog(ofGetTimestampString() + ".json", "save settings");
//		if(saveTo.bSuccess){
//			auto path = std::filesystem::path(saveTo.getPath());
//			auto folder = path.parent_path();
//			auto basename = path.stem().filename().string();
//			auto extension = ofToLower(path.extension().string());
//			auto timelineDir = (folder / (basename + "_timeline")).string();
//			if(extension == ".xml"){
//				ofXml xml;
//				if(std::filesystem::exists(path)){
//					xml.load(path);
//				}
//				ofSerialize(xml, gui.getParameter());
//				timeline.saveTracksToFolder(timelineDir);
//				timeline.saveStructure(timelineDir);
//				xml.save(path);
//			}else if(extension == ".json"){
//				ofJson json = ofLoadJson(path);
//				ofSerialize(json, gui.getParameter());
//				timeline.saveTracksToFolder(timelineDir);
//				timeline.saveStructure(timelineDir);
//				ofSavePrettyJson(path, json);
//			}else{
//				ofLogError("ofxGui") << extension << " not recognized, only .xml and .json supported by now";
//			}
//		}
//		return true;
//	}));


//	eventListeners.push_back(gui.loadPressedE.newListener([this]{
//		auto loadFrom = ofSystemLoadDialog("load settings", false, ofToDataPath("presets"));
//		if(loadFrom.bSuccess){
//			auto path = std::filesystem::path(loadFrom.getPath());
//			auto folder = path.parent_path();
//			auto basename = path.stem().filename().string();
//			auto extension = ofToLower(path.extension().string());
//			auto timelineDir = (folder / (basename + "_timeline")).string();
//			if(!ofDirectory(timelineDir).exists()){
//				timelineDir = folder;
//			}
//			if(extension == ".xml"){
//				ofXml xml;
//				xml.load(path);
//				ofDeserialize(xml, gui.getParameter());
//				timeline.loadStructure(timelineDir);
//				timeline.loadTracksFromFolder(timelineDir);
//				timeline.setOffset(glm::vec2(0, ofGetHeight() - timeline.getHeight()));
//				gui.refreshTimelined(&timeline);
//			}else
//			if(extension == ".json"){
//				ofJson json;
//				ofFile jsonFile(path);
//				jsonFile >> json;
//				ofDeserialize(json, gui.getParameter());
//				timeline.loadStructure(timelineDir);
//				timeline.loadTracksFromFolder(timelineDir);
//				timeline.setOffset(glm::vec2(0, ofGetHeight() - timeline.getHeight()));
//				gui.refreshTimelined(&timeline);
//			}else{
//				ofLogError("ofxGui") << extension << " not recognized, only .xml and .json supported by now";
//			}
//			reset();
//		}
//		return true;
//	}));

	eventListeners.push_back(parameters.recording.fps.newListener([this](int & fps){
		if(parameters.recording.systemClock){
			ofSetTimeModeSystem();
		}else{
			ofSetTimeModeFixedRate(ofGetFixedStepForFps(fps));
		}
	}));

	eventListeners.push_back(parameters.recording.systemClock.newListener([this](bool & systemClock){
		if(systemClock){
			ofSetTimeModeSystem();
		}else{
			ofSetTimeModeFixedRate(ofGetFixedStepForFps(parameters.recording.fps));
		}
	}));

	parameters.rendering.fov.ownListener([this](float & fov){
		camera.setFov(fov);
	});

	// Setup renderer and post effects using resize callback.
	this->windowResized(ofGetWidth(), ofGetHeight());

	//this->loadPreset("_autosave");
	//this->reset();

	ofSetMutuallyExclusive(parameters.rendering.additiveBlending, parameters.rendering.glOneBlending);

}

//--------------------------------------------------------------
void ofApp::exit()
{
	particleSystem.clearParticles();
	
	// Clear transform feedback.
	glDeleteQueries(1, &numPrimitivesQuery);
}

//--------------------------------------------------------------
void ofApp::update()
{
	dt = timeline.getCurrentTime() - now;
	now = timeline.getCurrentTime();

	photons.update(dt);
	particleSystem.update(dt);

	auto & photons = this->photons.getPosnsRef();

	for (auto & light : pointLights)
	{
		light.disable();
		light.setPosition(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	}

	for (size_t i = 0, j = 0; i < photons.size() && j < kMaxLights; ++i)
	{
		if (photons[i].x > glm::vec3(-kHalfDim * 2).x  &&
			photons[i].y > glm::vec3(-kHalfDim * 2).y  &&
			photons[i].z > glm::vec3(-kHalfDim * 2).z  &&
			photons[i].x < glm::vec3(kHalfDim * 2).x &&
			photons[i].y < glm::vec3(kHalfDim * 2).y &&
			photons[i].z < glm::vec3(kHalfDim * 2).z) {
			auto & light = pointLights[j];
			light.enable();
			light.setDiffuseColor(ofFloatColor::white * parameters.rendering.lightStrength);
			light.setPointLight();
			light.setPosition(photons[i]);
			light.setAttenuation(0, 0, parameters.rendering.attenuation);
			j++;
		}
	}

	glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, numPrimitivesQuery);
	this->shader.beginTransformFeedback(GL_TRIANGLES, feedbackBuffer);
	{
		std::array<ofFloatColor, nm::Particle::NUM_TYPES> colors;
		std::transform(nm::Particle::DATA, nm::Particle::DATA + nm::Particle::NUM_TYPES, colors.begin(), [](const nm::Particle::Data & data)
		{
			return data.color.get();
		});
		this->shader.setUniform4fv("colors", reinterpret_cast<float*>(colors.data()), nm::Particle::NUM_TYPES);
		particleSystem.draw(this->shader);
	}
	this->shader.endTransformFeedback(feedbackBuffer);
	glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
	glGetQueryObjectuiv(numPrimitivesQuery, GL_QUERY_RESULT, &numPrimitives);


	if(parameters.rendering.drawText){
		textRenderer.update(particleSystem, BARYOGENESIS);
	}


	auto scale = environment->getExpansionScalar();
	bool renewLookAt = true;
	if(lookAt.first != lookAt.second){
		auto * lookAt1 = particleSystem.getById(lookAt.first);
		auto * lookAt2 = particleSystem.getById(lookAt.second);
		if(lookAt1 && lookAt2){
			renewLookAt = glm::distance(lookAt1->pos * scale, lookAt2->pos * scale) > nm::Octree<nm::Particle>::INTERACTION_DISTANCE();
		}
	}
	if(renewLookAt && timeConnectionLost==0){
		timeConnectionLost = now;
		lookAt.first = lookAt.second = 0;
	}

	auto timesinceConnectionLost = now - timeConnectionLost;
	if(renewLookAt && timesinceConnectionLost > 1){
		auto particles = particleSystem.getParticles();
		std::vector<nm::Particle*> sortedParticles(particles.size());
		std::transform(particles.begin(),
					   particles.end(),
					   sortedParticles.begin(),
					   [](nm::Particle & p){ return &p; });

		std::sort(sortedParticles.begin(), sortedParticles.end(), [&](nm::Particle * p1, nm::Particle * p2){
			return glm::distance2(p1->pos, camera.getGlobalPosition()) < glm::distance2(p2->pos, camera.getGlobalPosition());
		});

		for(auto * p1: sortedParticles){
			for(auto * p2: p1->potentialInteractionPartners){
				auto distance = glm::distance(p1->pos * scale, p2->pos * scale);
				travelDistance = glm::distance(p1->pos, camera.getGlobalPosition()) / kHalfDim;
				auto minTravelTime = travelDistance / parameters.rendering.travelMaxSpeed;
				auto annihilationPartners = std::count_if(p1->potentialInteractionPartners.begin(), p1->potentialInteractionPartners.end(), [&](nm::Particle * partner){
					return (p1->getAnnihilationFlag() ^ partner->getAnnihilationFlag()) == 0xFF;
				});
				auto aproxAnnihilationTime = annihilationPartners * 1.f / environment->systemSpeed;

				bool foundNew = distance < nm::Octree<nm::Particle>::INTERACTION_DISTANCE();
				foundNew &= distance > nm::Octree<nm::Particle>::INTERACTION_DISTANCE() * 1. / 2.;
				foundNew &= ((p1->isMatterQuark() && p2->isAntiMatterQuark()) || (p1->isAntiMatterQuark() && p2->isMatterQuark()));
				foundNew &= minTravelTime < aproxAnnihilationTime * 0.8;

				if(foundNew){
					if(p1->id<p2->id){
						lookAt.first = p1->id;
						lookAt.second = p2->id;
					}else{
						lookAt.first = p2->id;
						lookAt.second = p1->id;
					}
					timeRenewLookAt = now;
					prevLookAt = lerpedLookAt;
					prevCameraPosition = camera.getGlobalPosition();
					arrived = false;
					timeConnectionLost = 0;
					// cout << "renewed to " << p1->id << "  " << p2->id << " " << &p1 << " " << p2 << " at distance " << distance << endl;
					break;
				}
			}
		}
	}

	if(lookAt.first != lookAt.second){
		auto * lookAt1 = particleSystem.getById(lookAt.first);
		auto * lookAt2 = particleSystem.getById(lookAt.second);
		if(lookAt1 && lookAt2){
			lookAtPos = (lookAt1->pos + lookAt2->pos) / 2.;
		}
		currentLookAtParticles.first = lookAt1;
		currentLookAtParticles.second = lookAt2;

		auto timePassed = now - timeRenewLookAt;
		auto animationTime = std::min(2.f, travelDistance / parameters.rendering.travelMaxSpeed);
		auto pct = float(timePassed) / animationTime;
		if(currentLookAtParticles.first){
			annihilationPct = currentLookAtParticles.first->anihilationRatio / environment->getAnnihilationThresh();
		}
		if(pct>1){
			pct = 1;
			if(!arrived){
				orbitAngle = 0;
				arrived = true;
			}
		}
		pct = ofxeasing::map(pct, 0, 1, 0, 1, ofxeasing::quad::easeInOut);
		lerpedLookAt = glm::lerp(prevLookAt, lookAtPos, pct);


		auto newCameraPosition = glm::lerp(prevCameraPosition, lookAtPos + glm::vec3(0,0,50), pct);

		if(!arrived){
			camera.setPosition(newCameraPosition);
			camera.lookAt(lerpedLookAt, glm::vec3(0,1,0));
		}else{
			camera.orbitDeg(orbitAngle, 0, 50, lookAtPos);
			//	camera.orbitDeg(orbitAngle, 0, parameters.rendering.rotationRadius * kHalfDim);
			orbitAngle += dt * parameters.rendering.rotationSpeed;
			orbitAngle = ofWrap(orbitAngle, 0, 360);
		}
	}else{
		currentLookAtParticles.first = nullptr;
		currentLookAtParticles.second = nullptr;
	}



//	camera.setPosition(lookAtPos + glm::vec3(0,0,50));
//	camera.lookAt(lookAtPos, glm::vec3(0,1,0));



//	auto findNewAnnihilation = [this]{
//		std::pair<size_t, size_t> lookAt{0,0};
//		auto scale = environment->getExpansionScalar();
//		auto particles = particleSystem.getParticles();
//		std::vector<nm::Particle*> particlesPtr(particles.size());
//		std::transform(particles.begin(),
//					   particles.end(),
//					   particlesPtr.begin(),
//					   [](nm::Particle & p){ return &p; });

//		std::vector<nm::Particle*> sortedParticles;
//		std::copy_if(particlesPtr.begin(), particlesPtr.end(), std::back_inserter(sortedParticles),
//					 [](nm::Particle * p){ return p->isQuark(); });
//		std::sort(sortedParticles.begin(), sortedParticles.end(), [&](nm::Particle * p1, nm::Particle * p2){
//			return glm::distance2(p1->pos, camera.getGlobalPosition()) < glm::distance2(p2->pos, camera.getGlobalPosition());
//		});

//		std::vector<nm::Particle*> annihilationPartners;
//		for(auto * p1: sortedParticles){
//			annihilationPartners.clear();
//			std::copy_if(p1->potentialInteractionPartners.begin(), p1->potentialInteractionPartners.end(), std::back_inserter(annihilationPartners), [&](nm::Particle * partner){
//				return (p1->id < partner->id) && (p1->getAnnihilationFlag() ^ partner->getAnnihilationFlag()) == 0xFF;
//			});
//			for(auto * p2: annihilationPartners){
//				auto distance = glm::distance(p1->pos * scale, p2->pos * scale);
//				travelDistance = glm::distance(p1->pos, camera.getPosition());
//				auto minTravelTime = travelDistance / kHalfDim / parameters.rendering.travelMaxSpeed;
//				auto aproxAnnihilationTime = annihilationPartners.size() * 1.f / environment->systemSpeed;
//				travelSpeed = std::max(travelDistance / (aproxAnnihilationTime * 0.8f), parameters.rendering.travelMaxSpeed * kHalfDim);

//				bool foundNew = distance < nm::Octree<nm::Particle>::INTERACTION_DISTANCE();
//				foundNew &= distance > nm::Octree<nm::Particle>::INTERACTION_DISTANCE() * 1. / 2.;
//				foundNew &= minTravelTime < aproxAnnihilationTime * 0.5;

//				if(foundNew){
//					if(p1->id<p2->id){
//						lookAt.first = p1->id;
//						lookAt.second = p2->id;
//					}else{
//						lookAt.first = p2->id;
//						lookAt.second = p1->id;
//					}
//					break;
//				}
//			}
//		}
//		return lookAt;
//	};

//	auto distanceToTarget = glm::distance(camera.getPosition(), lookAtPos);
//	auto pctSpeed = travelDistance>0 ? ofMap(distanceToTarget / travelDistance, 0, 1, 2, 0.1, true) : 1;
//	if(lookAt.first != lookAt.second){
//		auto * lookAt1 = particleSystem.getById(lookAt.first);
//		if(lookAt1){
//			auto pctAnni = lookAt1->anihilationRatio / environment->getAnnihilationThresh();
//			cout << pctAnni << endl;
//		}
//	}
////	travelSpeed = /*travelSpeed * 0.9 + */parameters.rendering.travelMaxSpeed * kHalfDim * dt * pctSpeed/* * 0.1*/;
//	traveledLength += travelSpeed * dt;// * pctSpeed;

//	if(cameraPath.getVertices().empty() || traveledLength >= cameraPath.getPerimeter()){
//		lookAt = findNewAnnihilation();
//		if(lookAt.first != lookAt.second){
//			auto * lookAt1 = particleSystem.getById(lookAt.first);
//			auto * lookAt2 = particleSystem.getById(lookAt.second);
//			if(lookAt1 && lookAt2){
//				lookAtPos = (lookAt1->pos + lookAt2->pos) / 2.;
//				cameraViewport.setPosition(lookAtPos + glm::vec3(0,0,50));
//				cameraViewport.lookAt(lookAtPos, glm::vec3(0,1,0));
//				float nextDistance = glm::distance(camera.getPosition(), lookAtPos);
//				auto resolution = nextDistance * 10.;
//				cameraPath.curveTo(lookAtPos, resolution);
//				arrived = false;
//				pct = 0;
//				timeRenewLookAt = now;
//				cout << "renewed to " << lookAtPos << " at distance " << nextDistance << " with reolution " << resolution << endl;
//			}
//		}
//	}


//	if(traveledLength < cameraPath.getPerimeter()){
//		auto left = camera.getXAxis();
//		auto next = cameraPath.getPointAtLength(traveledLength);
//		auto nextLength = traveledLength + travelSpeed;
//		auto lookAt = cameraPath.getPointAtLength(nextLength);
//		auto up = glm::cross(left, glm::normalize(lookAt - next));
//		camera.setPosition(next);
//		camera.lookAt(lookAt, up);
//	}

//	cout << traveledLength << " / " << cameraPath.getPerimeter() <<  " @ " << pctSpeed << endl;
}

//--------------------------------------------------------------
void ofApp::draw()
{
	// Draw the scene.
	this->fboScene.begin();
	{
		ofClear(0, 255);

		if(parameters.rendering.useEasyCam){
			easyCam.begin();
		}else{
			camera.begin();
		}
		ofEnableDepthTest();
		{
			if (this->parameters.debugLights)
			{
				for (auto& light : pointLights)
				{
					if (light.getPosition().x > glm::vec3(-kHalfDim).x  &&
						light.getPosition().y > glm::vec3(-kHalfDim).y  &&
						light.getPosition().z > glm::vec3(-kHalfDim).z  &&
						light.getPosition().x < glm::vec3(kHalfDim).x &&
						light.getPosition().y < glm::vec3(kHalfDim).y &&
						light.getPosition().z < glm::vec3(kHalfDim).z)
					{
						light.draw();
					}
				}
			}



			if(parameters.rendering.drawText){
				ofEnableBlendMode(OF_BLENDMODE_ADD);
				if(parameters.rendering.useEasyCam){
					textRenderer.draw(particleSystem, *environment, BARYOGENESIS, currentLookAtParticles, renderer, easyCam);
				}else{
					textRenderer.draw(particleSystem, *environment, BARYOGENESIS, currentLookAtParticles, renderer, camera);
				}
			}



			if (this->parameters.rendering.additiveBlending){
				ofEnableBlendMode(OF_BLENDMODE_ADD);
			}else if(this->parameters.rendering.glOneBlending){
				glBlendFunc(GL_ONE, GL_ONE);
			}else{
				ofEnableAlphaBlending();
			}
			if(parameters.rendering.drawModels){
				if(parameters.rendering.useEasyCam){
					this->renderer.draw(feedbackVbo, 0, numPrimitives * 3, GL_TRIANGLES, easyCam);
				}else{
					this->renderer.draw(feedbackVbo, 0, numPrimitives * 3, GL_TRIANGLES, camera);
				}
			}
			if (parameters.rendering.drawPhotons)
			{
				photons.draw();
			}
		}
		ofDisableDepthTest();

		cameraPath.draw();

		if(parameters.rendering.useEasyCam){
			camera.draw();
			easyCam.end();
		}else{
			camera.end();
		}

		ofFill();
		ofSetColor(0);
		ofDrawRectangle({fboScene.getWidth() - 320, fboScene.getHeight(), 320, -240});
		ofSetColor(255);
		cameraViewport.begin({fboScene.getWidth() - 320, 0, 320, 240});
		ofEnableBlendMode(OF_BLENDMODE_ADD);
		textRenderer.draw(particleSystem, *environment, BARYOGENESIS, currentLookAtParticles, renderer, cameraViewport);
		if (this->parameters.rendering.additiveBlending){
			ofEnableBlendMode(OF_BLENDMODE_ADD);
		}else if(this->parameters.rendering.glOneBlending){
			glBlendFunc(GL_ONE, GL_ONE);
		}else{
			ofEnableAlphaBlending();
		}
		this->renderer.draw(feedbackVbo, 0, numPrimitives * 3, GL_TRIANGLES, cameraViewport);
		cameraViewport.end();
	}
	this->fboScene.end();

	this->postEffects.process(this->fboScene.getTexture(), this->fboPost, this->postParams);

	ofDisableBlendMode();
	ofSetColor(ofColor::white);
	this->fboPost.draw(0, 0);

	if (this->parameters.recording.recordSequence || this->parameters.recording.recordVideo)
	{
		this->textureRecorder.save(this->fboPost.getTexture());

		if (this->timeline.getCurrentFrame() == this->timeline.getOutFrame())
		{
			this->parameters.recording.recordSequence = false;
			this->parameters.recording.recordVideo = false;
		}
	}

	ofEnableBlendMode(OF_BLENDMODE_ALPHA);

	// Draw the controls.
	this->timeline.draw();
	this->gui.draw();

	ofDrawBitmapString(ofGetFrameRate(), ofGetWidth()-100, 20);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
//	if (key == 'L')
//	{
//		this->cameraTrack.lockCameraToTrack ^= 1;
//	}
//	else if (key == 'T')
//	{
//		this->cameraTrack.addKeyframe();
//	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
	this->timeline.setOffset(glm::vec2(0, ofGetHeight() - this->timeline.getHeight()));
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

//--------------------------------------------------------------
void ofApp::reset()
{
	particleSystem.clearParticles();
	
	ofSetGlobalAmbientColor(ofFloatColor(parameters.rendering.ambientLight));

	// Use the same random seed for each run.
	ofSeedRandom(3030);

	int counts[6];
	for (int i = 0; i < 6; ++i)
	{
		counts[i] = 0;
	}

	for (unsigned i = 0; i < 4000; ++i)
	{
		glm::vec3 position = glm::vec3(
			ofRandom(-kHalfDim, kHalfDim),
			ofRandom(-kHalfDim, kHalfDim),
			ofRandom(-kHalfDim, kHalfDim)
		);

		float speed = glm::gaussRand(60.f, 20.f);
		glm::vec3 velocity = glm::sphericalRand(speed);

		float coin = ofRandomuf();
		nm::Particle::Type type;
//		// 2:1 ratio
//		//if (coin < .11f) type = nm::Particle::Type::POSITRON;
//		//else if (coin < .33f) type = nm::Particle::Type::ELECTRON;
//		//else if (coin < .44f) type = nm::Particle::Type::ANTI_UP_QUARK;
//		//else if (coin < .67f) type = nm::Particle::Type::UP_QUARK;
//		//else if (coin < .78f) type = nm::Particle::Type::ANTI_DOWN_QUARK;
//		//else type = nm::Particle::Type::DOWN_QUARK;
//		// 3:1 ratio
//		if (coin < .0825f) type = nm::Particle::Type::POSITRON;
//		else if (coin < .33f) type = nm::Particle::Type::ELECTRON;
//		else if (coin < .4125f) type = nm::Particle::Type::ANTI_UP_QUARK;
//		else if (coin < .67f) type = nm::Particle::Type::UP_QUARK;
//		else if (coin < .7425f) type = nm::Particle::Type::ANTI_DOWN_QUARK;
//		else type = nm::Particle::Type::DOWN_QUARK;
//		counts[type]++;
//		particleSystem.addParticle(type, position, velocity);
		particleSystem.addParticle((nm::Particle::Type)(i % 6), position, velocity);
	}

	int numParticles = particleSystem.getParticles().size();
	ofLog() << "Reset system with " << numParticles << " particles: " << endl
		<< "  " << counts[nm::Particle::Type::ELECTRON]        << " (" << ofToString(counts[nm::Particle::Type::ELECTRON] / (float)numParticles, 2)        << ") electrons" << endl
		<< "  " << counts[nm::Particle::Type::POSITRON]        << " (" << ofToString(counts[nm::Particle::Type::POSITRON] / (float)numParticles, 2)        << ") positrons" << endl
		<< "  " << counts[nm::Particle::Type::UP_QUARK]        << " (" << ofToString(counts[nm::Particle::Type::UP_QUARK] / (float)numParticles, 2)        << ") up quarks" << endl
		<< "  " << counts[nm::Particle::Type::ANTI_UP_QUARK]   << " (" << ofToString(counts[nm::Particle::Type::ANTI_UP_QUARK] / (float)numParticles, 2)   << ") anti up quarks" << endl
		<< "  " << counts[nm::Particle::Type::DOWN_QUARK]      << " (" << ofToString(counts[nm::Particle::Type::DOWN_QUARK] / (float)numParticles, 2)      << ") down quarks" << endl
		<< "  " << counts[nm::Particle::Type::ANTI_DOWN_QUARK] << " (" << ofToString(counts[nm::Particle::Type::ANTI_DOWN_QUARK] / (float)numParticles, 2) << ") anti down quarks" << endl;
}

//--------------------------------------------------------------
bool ofApp::loadPreset(const string & presetName)
{
	// Make sure file exists.
	const auto presetPath = std::filesystem::path("presets") / presetName;
	const auto presetFile = ofFile(presetPath);
	if (presetFile.exists())
	{
		// Load parameters from the preset.
		const auto paramsPath = presetPath / "parameters.json";
		const auto paramsFile = ofFile(paramsPath);
		if (paramsFile.exists())
		{
			const auto json = ofLoadJson(paramsFile);
			ofDeserialize(json, this->gui.getParameter());
			//ofDeserialize(json, this->camera, "ofEasyCam");
		}

		this->timeline.loadStructure(presetPath.string());
		this->timeline.loadTracksFromFolder(presetPath.string());

		this->gui.refreshTimelined(&this->timeline);

		this->currPreset = presetName;
	}
	else
	{
		ofLogWarning(__FUNCTION__) << "Directory not found at path " << presetPath;
		this->currPreset.clear();
	}

	// Setup scene with the new parameters.
	this->reset();

	if (this->currPreset.empty())
	{
		return false;
	}

	return true;
}

//--------------------------------------------------------------
bool ofApp::savePreset(const string & presetName)
{
	const auto presetPath = std::filesystem::path("presets") / presetName;

	const auto paramsPath = presetPath / "parameters.json";
	nlohmann::json json;
	ofSerialize(json, this->gui.getParameter());
	ofSerialize(json, this->camera, "ofEasyCam");
	ofSavePrettyJson(paramsPath, json);

	this->timeline.saveTracksToFolder(presetPath.string());
	this->timeline.saveStructure(presetPath.string());

	return true;
}
