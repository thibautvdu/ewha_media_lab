#ifndef FLEXIBLE_SURFACE_AUGMENTATION_SIMPLE_3D_BLOB_H_
#define FLEXIBLE_SURFACE_AUGMENTATION_SIMPLE_3D_BLOB_H_

#include "ofMain.h"

namespace garmentAugmentation {
	class simple3dBlob {
		public:
			int idx;
			vector<int> contourIndices2d;
			ofVec3f centroid;
			ofVec3f minX, minY, minZ; // points with minimum x / y / z
			ofVec3f maxX, maxY, maxZ; // points with maximum x / y / z
			ofVec3f boundingBoxMax, boundingBoxMin; // min bounding xyz
			ofVec3f dimensions; //dimensions
			ofVec3f massCenter;
			float volume; // volume
			int nbPoints;
	};
};

#endif // !FLEXIBLE_SURFACE_AUGMENTATION_SIMPLE_3D_BLOB_H_