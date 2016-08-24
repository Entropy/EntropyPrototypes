#include "Box.h"

namespace entropy
{
	namespace geom
	{
		//--------------------------------------------------------------
		Box::Box()
			: meshDirty(true)
			, colorDirty(true)
		{			
			this->paramListeners.push_back(this->color.newListener([this](ofFloatColor &)
			{
				this->colorDirty = true;
			}));
			this->paramListeners.push_back(this->size.newListener([this](float &)
			{
				this->meshDirty = true;
			}));
			this->paramListeners.push_back(this->edgeWidth.newListener([this](float &)
			{
				this->meshDirty = true;
			}));
			this->paramListeners.push_back(this->subdivisions.newListener([this](int &)
			{
				this->meshDirty = true;
			}));
		}

		//--------------------------------------------------------------
		Box::~Box()
		{
			this->clear();
		}

		//--------------------------------------------------------------
		void Box::clear()
		{
			this->mesh.clear();
		}

		//--------------------------------------------------------------
		bool Box::update()
		{
			if (this->enabled)
			{
				bool didUpdate = false;

				if (this->meshDirty)
				{
					this->rebuildMesh();
					this->light.setPosition(glm::vec3(this->size) * 2.0f);

					didUpdate = true;
				}

				if (this->colorDirty)
				{
					this->mesh.getColors().resize(this->mesh.getVertices().size());
					for (auto & c : this->mesh.getColors()) 
					{
						c = this->color;
					}

					this->colorDirty = false;
					didUpdate = true;
				}

				return didUpdate;
			}
			return false;
		}

		//--------------------------------------------------------------
		void Box::draw() const
		{
			if (!this->enabled) return;

			ofPushStyle();
			{
				//this->material.setDiffuseColor(color);
				//this->light.enable();
				{
					ofSetColor(color.get());
					//this->material.begin();
					{
						const auto cullMode = static_cast<CullMode>(this->cullFace.get());
						if (cullMode != CullMode::None)
						{
							glEnable(GL_CULL_FACE);
							if (cullMode == CullMode::Back)
							{
								glCullFace(GL_BACK);
							}
							else
							{
								glCullFace(GL_FRONT);
							}
						}
						else
						{
							glDisable(GL_CULL_FACE);
						}
						this->mesh.draw();
						if (cullMode != CullMode::None)
						{
							glDisable(GL_CULL_FACE);
						}
					}
					//this->material.end();
				}
				//this->light.disable();
			}
			ofPopStyle();
		}

		//--------------------------------------------------------------
		void Box::draw(render::WireframeFillRenderer & renderer)
		{
			if (!this->enabled) return;
			
			ofPushStyle();
			{
				//this->material.setDiffuseColor(color);
				//this->light.enable();
				{
					ofSetColor(color.get());
					//this->material.begin();
					{
						const auto cullMode = static_cast<CullMode>(this->cullFace.get());
						if (cullMode != CullMode::None)
						{
							glEnable(GL_CULL_FACE);
							if (cullMode == CullMode::Back)
							{
								glCullFace(GL_BACK);
							}
							else
							{
								glCullFace(GL_FRONT);
							}
						}
						else
						{
							glDisable(GL_CULL_FACE);
						}
						renderer.drawElements(this->mesh.getVbo(), 0, this->mesh.getNumIndices());
						if (cullMode != CullMode::None)
						{
							glDisable(GL_CULL_FACE);
						}
					}
					//this->material.end();
				}
				//this->light.disable();
			}
			ofPopStyle();
		}

		//--------------------------------------------------------------
		void Box::rebuildMesh()
		{
			this->clear();

			float edgeOffset = size / subdivisions;
			glm::vec3 center;
			glm::vec3 dimensions;

			// Front.
			{
				// Horizontal.
				{
					center = glm::vec3(0.0f, 0.0f - size * 0.5f, 0.0f + size * 0.5f);
					dimensions = glm::vec3(size + edgeWidth, edgeWidth, edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Front);
						center.y += edgeOffset;
					}
				}
				// Vertical.
				{
					center = glm::vec3(0.0f - size * 0.5f, 0.0f, 0.0f + size * 0.5f);
					dimensions = glm::vec3(edgeWidth, size + edgeWidth, edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Front);
						center.x += edgeOffset;
					}
				}
			}

			// Back.
			{
				// Horizontal.
				{
					center = glm::vec3(0.0f, 0.0f - size * 0.5f, 0.0f - size * 0.5f);
					dimensions = glm::vec3(size + edgeWidth, edgeWidth, edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Back);
						center.y += edgeOffset;
					}
				}
				// Vertical.
				{
					center = glm::vec3(0.0f - size * 0.5f, 0.0f, 0.0f - size * 0.5f);
					dimensions = glm::vec3(edgeWidth, size + edgeWidth, edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Back);
						center.x += edgeOffset;
					}
				}
			}

			// Left.
			{
				// Horizontal.
				{
					center = glm::vec3(0.0f - size * 0.5f, 0.0f - size * 0.5f, 0.0f);
					dimensions = glm::vec3(edgeWidth, edgeWidth, size + edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Left);
						center.y += edgeOffset;
					}
				}
				// Vertical.
				{
					center = glm::vec3(0.0f - size * 0.5f, 0.0f, 0.0f - size * 0.5f);
					dimensions = glm::vec3(edgeWidth, size + edgeWidth, edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Left);
						center.z += edgeOffset;
					}
				}
			}

			// Right.
			{
				// Horizontal.
				{
					center = glm::vec3(0.0f + size * 0.5f, 0.0f - size * 0.5f, 0.0f);
					dimensions = glm::vec3(edgeWidth, edgeWidth, size + edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Right);
						center.y += edgeOffset;
					}
				}
				// Vertical.
				{
					center = glm::vec3(0.0f + size * 0.5f, 0.0f, 0.0f - size * 0.5f);
					dimensions = glm::vec3(edgeWidth, size + edgeWidth, edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Right);
						center.z += edgeOffset;
					}
				}
			}

			// Top.
			{
				// Horizontal.
				{
					center = glm::vec3(0.0f, 0.0f + size * 0.5f, 0.0f - size * 0.5f);
					dimensions = glm::vec3(size + edgeWidth, edgeWidth, edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Top);
						center.z += edgeOffset;
					}
				}
				// Vertical.
				{
					center = glm::vec3(0.0f - size * 0.5f, 0.0f + size * 0.5f, 0.0f);
					dimensions = glm::vec3(edgeWidth, edgeWidth, size + edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Top);
						center.x += edgeOffset;
					}
				}
			}

			// Bottom.
			{
				// Horizontal.
				{
					center = glm::vec3(0.0f, 0.0f - size * 0.5f, 0.0f - size * 0.5f);
					dimensions = glm::vec3(size + edgeWidth, edgeWidth, edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Bottom);
						center.z += edgeOffset;
					}
				}
				// Vertical.
				{
					center = glm::vec3(0.0f - size * 0.5f, 0.0f - size * 0.5f, 0.0f);
					dimensions = glm::vec3(edgeWidth, edgeWidth, size + edgeWidth);
					for (int i = 0; i <= subdivisions; ++i)
					{
						this->addEdge(center, dimensions, Face::Bottom);
						center.x += edgeOffset;
					}
				}
			}

			this->meshDirty = false;
		}

		//--------------------------------------------------------------
		void Box::addEdge(const glm::vec3 & center, const glm::vec3 & dimensions, int faces)
		{
			const auto resX = 2;
			const auto resY = 2;
			const auto resZ = 2;

			const auto halfW = dimensions.x * .5f;
			const auto halfH = dimensions.y * .5f;
			const auto halfD = dimensions.z * .5f;

			glm::vec3 vert;
			glm::vec2 texcoord;
			glm::vec3 normal;
			std::size_t vertOffset = this->mesh.getNumVertices();

			if ((faces & Face::Front) == Face::Front)
			{
				// Front Face.
				normal = { 0.f, 0.f, 1.f };

				for (int iy = 0; iy < resY; iy++)
				{
					for (int ix = 0; ix < resX; ix++)
					{
						texcoord.x = ((float)ix / ((float)resX - 1.f));
						texcoord.y = ((float)iy / ((float)resY - 1.f));

						vert.x = center.x + texcoord.x * dimensions.x - halfW;
						vert.y = center.y + texcoord.y * dimensions.y - halfH;
						vert.z = center.z + halfD;

						this->mesh.addVertex(vert);
						this->mesh.addTexCoord(texcoord);
						this->mesh.addNormal(normal);
					}
				}

				for (int y = 0; y < resY - 1; y++) {
					for (int x = 0; x < resX - 1; x++) {
						// first triangle //
						this->mesh.addIndex((y)*resX + x + vertOffset);
						this->mesh.addIndex((y)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + vertOffset);

						// second triangle //
						this->mesh.addIndex((y)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + vertOffset);
					}
				}

				vertOffset = this->mesh.getNumVertices();
			}

			if ((faces & Face::Right) == Face::Right)
			{
				// Right Side Face.
				normal = { 1.f, 0.f, 0.f };
				// add the vertexes //
				for (int iy = 0; iy < resY; iy++) {
					for (int ix = 0; ix < resZ; ix++) {

						// normalized tex coords //
						texcoord.x = ((float)ix / ((float)resZ - 1.f));
						texcoord.y = ((float)iy / ((float)resY - 1.f));

						//vert.x = texcoord.x * width - halfW;
						vert.x = center.x + halfW;
						vert.y = center.y + texcoord.y * dimensions.y - halfH;
						vert.z = center.z + texcoord.x * -dimensions.z + halfD;

						this->mesh.addVertex(vert);
						this->mesh.addTexCoord(texcoord);
						this->mesh.addNormal(normal);
					}
				}

				for (int y = 0; y < resY - 1; y++) {
					for (int x = 0; x < resZ - 1; x++) {
						// first triangle //
						this->mesh.addIndex((y)*resZ + x + vertOffset);
						this->mesh.addIndex((y)*resZ + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resZ + x + vertOffset);

						// second triangle //
						this->mesh.addIndex((y)*resZ + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resZ + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resZ + x + vertOffset);
					}
				}

				vertOffset = this->mesh.getNumVertices();
			}

			if ((faces & Face::Left) == Face::Left)
			{
				// Left Side Face.
				normal = { -1.f, 0.f, 0.f };
				// add the vertexes //
				for (int iy = 0; iy < resY; iy++) {
					for (int ix = 0; ix < resZ; ix++) {

						// normalized tex coords //
						texcoord.x = ((float)ix / ((float)resZ - 1.f));
						texcoord.y = ((float)iy / ((float)resY - 1.f));

						//vert.x = texcoord.x * width - halfW;
						vert.x = center.x - halfW;
						vert.y = center.y + texcoord.y * dimensions.y - halfH;
						vert.z = center.z + texcoord.x * dimensions.z - halfD;

						this->mesh.addVertex(vert);
						this->mesh.addTexCoord(texcoord);
						this->mesh.addNormal(normal);
					}
				}

				for (int y = 0; y < resY - 1; y++) {
					for (int x = 0; x < resZ - 1; x++) {
						// first triangle //
						this->mesh.addIndex((y)*resZ + x + vertOffset);
						this->mesh.addIndex((y)*resZ + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resZ + x + vertOffset);

						// second triangle //
						this->mesh.addIndex((y)*resZ + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resZ + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resZ + x + vertOffset);
					}
				}

				vertOffset = this->mesh.getNumVertices();
			}

			if ((faces & Face::Back) == Face::Back)
			{
				// Back Face.
				normal = { 0.f, 0.f, -1.f };
				// add the vertexes //
				for (int iy = 0; iy < resY; iy++) {
					for (int ix = 0; ix < resX; ix++) {

						// normalized tex coords //
						texcoord.x = ((float)ix / ((float)resX - 1.f));
						texcoord.y = ((float)iy / ((float)resY - 1.f));

						vert.x = center.x + texcoord.x * -dimensions.x + halfW;
						vert.y = center.y + texcoord.y * dimensions.y - halfH;
						vert.z = center.z - halfD;

						this->mesh.addVertex(vert);
						this->mesh.addTexCoord(texcoord);
						this->mesh.addNormal(normal);
					}
				}

				for (int y = 0; y < resY - 1; y++) {
					for (int x = 0; x < resX - 1; x++) {
						// first triangle //
						this->mesh.addIndex((y)*resX + x + vertOffset);
						this->mesh.addIndex((y)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + vertOffset);

						// second triangle //
						this->mesh.addIndex((y)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + vertOffset);
					}
				}

				vertOffset = this->mesh.getNumVertices();
			}

			if ((faces & Face::Bottom) == Face::Bottom)
			{
				// Top Face.
				normal = { 0.f, -1.f, 0.f };
				// add the vertexes //
				for (int iy = 0; iy < resZ; iy++) {
					for (int ix = 0; ix < resX; ix++) {

						// normalized tex coords //
						texcoord.x = ((float)ix / ((float)resX - 1.f));
						texcoord.y = ((float)iy / ((float)resZ - 1.f));

						vert.x = center.x + texcoord.x * dimensions.x - halfW;
						//vert.y = center.y + texcoord.y * height - halfH;
						vert.y = center.y - halfH;
						vert.z = center.z + texcoord.y * dimensions.z - halfD;

						this->mesh.addVertex(vert);
						this->mesh.addTexCoord(texcoord);
						this->mesh.addNormal(normal);
					}
				}

				for (int y = 0; y < resZ - 1; y++) {
					for (int x = 0; x < resX - 1; x++) {
						// first triangle //
						this->mesh.addIndex((y)*resX + x + vertOffset);
						this->mesh.addIndex((y)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + vertOffset);

						// second triangle //
						this->mesh.addIndex((y)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + vertOffset);
					}
				}

				vertOffset = this->mesh.getNumVertices();
			}

			if ((faces & Face::Top) == Face::Top)
			{
				// Bottom Face.
				normal = { 0.f, 1.f, 0.f };
				// add the vertexes //
				for (int iy = 0; iy < resZ; iy++) {
					for (int ix = 0; ix < resX; ix++) {

						// normalized tex coords //
						texcoord.x = ((float)ix / ((float)resX - 1.f));
						texcoord.y = ((float)iy / ((float)resZ - 1.f));

						vert.x = center.x + texcoord.x * dimensions.x - halfW;
						//vert.y = center.y + texcoord.y * dimensions.y - halfH;
						vert.y = center.y + halfH;
						vert.z = center.z + texcoord.y * -dimensions.z + halfD;

						this->mesh.addVertex(vert);
						this->mesh.addTexCoord(texcoord);
						this->mesh.addNormal(normal);
					}
				}

				for (int y = 0; y < resZ - 1; y++) {
					for (int x = 0; x < resX - 1; x++) {
						// first triangle //
						this->mesh.addIndex((y)*resX + x + vertOffset);
						this->mesh.addIndex((y)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + vertOffset);

						// second triangle //
						this->mesh.addIndex((y)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + 1 + vertOffset);
						this->mesh.addIndex((y + 1)*resX + x + vertOffset);
					}
				}
			}
		}

		//--------------------------------------------------------------
		const ofVboMesh & Box::getMesh()
		{
			return this->mesh;
		}
	}
}