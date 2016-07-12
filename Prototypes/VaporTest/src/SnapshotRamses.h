#pragma once

#include "ofMain.h"
#include "ofxHDF5.h"
#include "ofxRange.h"
#include "Vapor3DTexture.h"
#include "VaporOctree.h"
#include "ofxVolumetrics3D.h"
#include "ofxTexture3d.h"

namespace ent
{
	enum SnapshotAttributes
	{
		POSITION_ATTRIBUTE = 0,
		SIZE_ATTRIBUTE = 1,
		DENSITY_ATTRIBUTE = 2,
	};
	
	class SnapshotRamses
	{
	public:
		SnapshotRamses();
		~SnapshotRamses();

		void setup(const std::string& folder, int frameIndex, float minDensity, float maxDensity, ofxTexture & tex, size_t worldsize);
		void clear();

		void update(ofShader& shader);
		void draw();
		void drawOctree(float minDensity, float maxDensity);

		ofxRange3f& getCoordRange();
		ofxRange1f& getSizeRange();
		ofxRange1f& getDensityRange();

		std::size_t getNumCells() const;
		bool isLoaded() const;
		BoundingBox m_boxRange;

	protected:
		void loadhdf5(const std::string& file, std::vector<float>& elements);
		void precalculate(const std::string folder, int frameIndex, float minDensity, float maxDensity, size_t worldsize);

		ofVbo m_vboMesh;

		ofxRange3f m_coordRange;
		ofxRange1f m_sizeRange;
		ofxRange1f m_densityRange;

		std::size_t m_numCells;
		bool m_bLoaded;
		ofTexture m_particlesTex;
		Vapor3DTexture vaporPixels;
		VaporOctree vaporOctree;
		ofBuffer vaporPixelsBuffer;
		ofShader voxels2texture;
		ofShader particles2texture;
	};
}
