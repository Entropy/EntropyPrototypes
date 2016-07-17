#include "LightingTest.h"

#include "entropy/Helpers.h"

namespace entropy
{
	namespace scene
	{
		//--------------------------------------------------------------
		LightingTest::LightingTest()
			: Base()
		{
			ENTROPY_SCENE_SETUP_LISTENER;
		}

		//--------------------------------------------------------------
		LightingTest::~LightingTest()
		{

		}

		//--------------------------------------------------------------
		// Set up your crap here!
		void LightingTest::setup()
		{
			ENTROPY_SCENE_EXIT_LISTENER;
			ENTROPY_SCENE_RESIZE_LISTENER;
			ENTROPY_SCENE_UPDATE_LISTENER;
			ENTROPY_SCENE_DRAW_BACK_LISTENER;
			ENTROPY_SCENE_DRAW_WORLD_LISTENER;
			ENTROPY_SCENE_DRAW_FRONT_LISTENER;
			ENTROPY_SCENE_GUI_LISTENER;
			ENTROPY_SCENE_SERIALIZATION_LISTENERS;

			// Load shaders.
			this->shader.load(this->getDataPath("shaders/main"));
			this->shader.printActiveUniforms();
			this->shader.printActiveUniformBlocks();
			CheckGLError();

			this->skyboxShader.load(this->getDataPath("shaders/skybox"));
			glGenVertexArrays(1, &this->defaultVao);
			CheckGLError();

			// Set up view UBO.
			const int viewUboBinding = 1;
			this->viewUbo.setup(viewUboBinding);
			this->viewUbo.configureShader(this->shader);
			this->viewUbo.configureShader(this->skyboxShader);
			CheckGLError();

			// Set up lighting.
			this->lightingSystem.setup(this->getCamera());
			this->lightingSystem.configureShader(this->shader);
			this->lightingSystem.setAmbientIntensity(0.5f);
			CheckGLError();

			// Set up PBR.
			this->material.setBaseColor(ofFloatColor(1.0f, 1.0f, 1.0f, 1.0f));
			this->material.setMetallic(0.0f);
			this->material.setRoughness(0.0f);
			this->material.setEmissiveColor(ofFloatColor(1.0f, 0.4f, 0.0f, 1.0f));
			this->material.setEmissiveIntensity(0.0f);
			CheckGLError();

			this->skyboxMap.loadDDSTexture(this->getDataPath("textures/output_skybox.dds"));
			this->irradianceMap.loadDDSTexture(this->getDataPath("textures/output_iem.dds"));
			this->radianceMap.loadDDSTexture(this->getDataPath("textures/output_pmrem.dds"));
			CheckGLError();

			glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

			this->sphere = ofSpherePrimitive(1.0f, 24);
		}

		//--------------------------------------------------------------
		// Clean up your crap here!
		void LightingTest::exit()
		{

		}

		//--------------------------------------------------------------
		// Resize your content here. 
		// Note that this is not the window size but the canvas size.
		void LightingTest::resize(ofResizeEventArgs & args)
		{

		}

		//--------------------------------------------------------------
		// Update your data here, once per frame.
		void LightingTest::update(double & dt)
		{
			this->animateLights();
		}

		//--------------------------------------------------------------
		// Draw 2D elements in the background here.
		void LightingTest::drawBack()
		{

		}

		//--------------------------------------------------------------
		// Draw 3D elements here.
		void LightingTest::drawWorld()
		{
			ofDisableAlphaBlending();
			
			auto cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
			GLint cullFaceMode[1];
			glGetIntegerv(GL_CULL_FACE_MODE, cullFaceMode);

			ofDisableAlphaBlending();

			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);

			this->viewUbo.bind();
			{
				this->skyboxMap.bind(14);
				this->irradianceMap.bind(2);
				this->radianceMap.bind(3);

				this->viewUbo.update(this->getCamera());
				this->lightingSystem.update(this->getCamera());

				ofSetColor(255, 255, 255, 255);

				if (this->parameters.debug)
				{
					this->lightingSystem.debugDrawFrustum(this->getCamera());

					this->lightingSystem.debugDrawCulledPointLights();
					this->lightingSystem.debugDrawClusteredPointLights();
					this->lightingSystem.debugDrawOccupiedClusters(this->getCamera());
				}
				else
				{
					this->drawSkybox();

					this->lightingSystem.begin();
					{
						this->shader.begin();
						{
							this->material.setUniforms(this->shader);
							this->shader.setUniform1f("uExposure", this->parameters.camera.exposure);
							this->shader.setUniform1f("uGamma", this->parameters.camera.gamma);
							this->shader.setUniform1i("uIrradianceMap", 2);
							this->shader.setUniform1i("uRadianceMap", 3);

							this->drawScene();
						}
						this->shader.end();
					}
					this->lightingSystem.end();
				}

				this->skyboxMap.unbind(14);
				this->irradianceMap.unbind(2);
				this->radianceMap.unbind(3);
			}
			this->viewUbo.unbind();

			// Restore state.
			if (GL_TRUE == cullFaceEnabled)
			{
				glCullFace(cullFaceMode[0]);
			}
			else
			{
				glDisable(GL_CULL_FACE);
			}
		}

		//--------------------------------------------------------------
		// Draw 2D elements in the foreground here.
		void LightingTest::drawFront()
		{

		}

		//--------------------------------------------------------------
		// Add Scene specific GUI windows here.
		void LightingTest::gui(ofxPreset::Gui::Settings & settings)
		{
			ofxPreset::Gui::SetNextWindow(settings);
			if (ofxPreset::Gui::BeginWindow(this->parameters.getName(), settings))
			{
				ofxPreset::Gui::AddParameter(this->parameters.debug);

				if (ImGui::CollapsingHeader("Material", nullptr, true, true))
				{
					ImGui::ColorEdit4("Base Color", (float *)&this->material.baseColor);
					ImGui::SliderFloat("Emissive Intensity", &this->material.emissiveIntensity, 0.0f, 1.0f);
					ImGui::ColorEdit4("Emissive Color", (float *)&this->material.emissiveColor);
#ifdef USE_INSTANCED
					ImGui::SliderFloat("Metallic", &this->material.metallic, 0.0f, 1.0f);
					ImGui::SliderFloat("Roughness", &this->material.roughness, 0.0f, 1.0f);
#endif
				}

				ofxPreset::Gui::AddGroup(this->parameters.camera, settings);

				if (ImGui::CollapsingHeader("Lighting", nullptr, true, true))
				{
					ImGui::SliderFloat("Ambient IBL Strength", &this->lightingSystem.ambientIntensity, 0.0f, 3.0f);
					if (ImGui::Button("Create Point Lights"))
					{
						createRandomLights();
					}
					ImGui::SameLine();
					if (ImGui::Button("Clear Point Lights"))
					{
						this->lightingSystem.clearPointLights();
					}

					ImGui::BeginGroup();
					ImGui::Text("Stats");
					ImGui::Text("Visible Lights: %u", this->lightingSystem.getNumVisibleLights());
					ImGui::Text("Culled Lights: %u", this->lightingSystem.getNumCulledPointLights());
					ImGui::Text("Num Affected Clusters: %u", this->lightingSystem.getNumAffectedClusters());
					ImGui::Text("Num Light Indices: %u", this->lightingSystem.getNumPointLightIndices());
					ImGui::EndGroup();
				}

			}
			ofxPreset::Gui::EndWindow(settings);
		}

		//--------------------------------------------------------------
		// Do something after the parameters are saved.
		// You can save other stuff to the same json object here too.
		void LightingTest::serialize(nlohmann::json & json)
		{

		}

		//--------------------------------------------------------------
		// Do something after the parameters are loaded.
		// You can load your other stuff here from that json object.
		// You can also set any refresh flags if necessary.
		void LightingTest::deserialize(const nlohmann::json & json)
		{

		}

		//--------------------------------------------------------------
		void LightingTest::createRandomLights()
		{
			this->lightingSystem.clearPointLights();

			const auto positionDist = 330.0f;
			const auto radius = 60.0f;

			const auto numPointLights = 60;
			for (int i = 0; i < numPointLights; ++i)
			{
				auto offset = glm::vec3(ofRandom(-positionDist, positionDist), 0.0f, ofRandom(-positionDist, positionDist));
				auto color = glm::normalize(glm::vec3(ofRandomuf(), ofRandomuf(), ofRandomuf()));
				auto light = ofxRTK::lighting::PointLight(offset, color, radius, 6000.0f);
				this->lightingSystem.addPointLight(light);
			}
		}

		//--------------------------------------------------------------
		void LightingTest::animateLights()
		{
			auto & pointLights = this->lightingSystem.getPointLights();
			for (int idx = 0; idx < pointLights.size(); ++idx)
			{
				auto & light = pointLights[idx];
				light.position.y = (sinf((ofGetElapsedTimeMillis() + idx * 40) / 1400.0f) * 0.5f + 0.5f) * 100.0f;
			}
		}

		//--------------------------------------------------------------
		void LightingTest::drawSkybox()
		{
			glDisable(GL_CULL_FACE);
			ofDisableDepthTest();

			this->skyboxShader.begin();
			this->skyboxShader.setUniform1f("uExposure", this->parameters.camera.exposure);
			this->skyboxShader.setUniform1f("uGamma", this->parameters.camera.gamma);
			this->skyboxShader.setUniform1i("uCubeMap", 3);
			{
				// Draw full-screen quad.
				glBindVertexArray(this->defaultVao);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}
			this->skyboxShader.end();

			ofEnableDepthTest();
			glEnable(GL_CULL_FACE);
		}

		//--------------------------------------------------------------
		void LightingTest::drawScene()
		{
			static const int kNumSpheres = 8;

			static const float kRadius = 30.0f;
			static const float kSpacing = kRadius * 2.0f + 15.0f;
			static const float kOffset = -kNumSpheres * kSpacing * 0.5f;

#ifdef USE_INSTANCED
			static const int kTotalSpheres = kNumSpheres * kNumSpheres;

			if (!this->bufferObject.isAllocated())
			{
				this->vboMesh = this->sphere.getMesh();

				glm::mat4 transforms[kTotalSpheres];
				for (int z = 0; z < kNumSpheres; ++z)
				{
					for (int x = 0; x < kNumSpheres; ++x)
					{
						int idx = z * kNumSpheres + x;
						transforms[idx] = glm::translate(glm::vec3(kOffset + x * kSpacing, kRadius * 2.0f, kOffset + z * kSpacing)) * glm::scale(glm::vec3(kRadius));
					}
				}

				this->bufferObject.allocate(sizeof(glm::mat4) * kTotalSpheres, transforms, GL_STATIC_DRAW);
				this->bufferTexture.allocateAsBufferTexture(this->bufferObject, GL_RGBA32F);
			}

			this->material.setUniforms(this->shader);
			this->shader.setUniformTexture("uOffsetTex", this->bufferTexture, 0);
			this->vboMesh.drawInstanced(OF_MESH_FILL, kTotalSpheres);
#else
			for (int z = 0; z < kNumSpheres; ++z)
			{
				float zPercent = z / (float)(kNumSpheres - 1);

				for (int x = 0; x < kNumSpheres; ++x)
				{
					float xPercent = x / (float)(kNumSpheres - 1);
					this->material.metallic = std::max(zPercent, 0.001f);
					this->material.roughness = std::max(xPercent * xPercent, 0.001f);
					this->material.setUniforms(this->shader);

					ofPushMatrix();
					{
						ofTranslate(kOffset + x * kSpacing, kRadius * 2.0, kOffset + z * kSpacing);
						ofScale(kRadius);
						this->shader.setUniformMatrix4f("uNormalMatrix", ofGetCurrentNormalMatrix());

						this->sphere.draw();
					}
					ofPopMatrix();
				}
			}
#endif
		}
	}
}