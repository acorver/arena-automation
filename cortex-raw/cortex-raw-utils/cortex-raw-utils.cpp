//
// This script parses a .VC* file and outputs its centroid data into a .msgpack 
// data file. This file will then be post-processed by a python script.
//
// Author: Abel Corver
//         abel.corver@gmail.com
//         Anthony Leonardo Lab, Janelia Research Campus, HHMI
// 
//         Essentially all code outside this main function file was written 
//         by Ned Phipps for the Motion Analysis Company.
//  
// Date  : March 2017
// 

#include "stdafx.h"
#include "Blobs.h"

// ============================================================================
// Main function
// ============================================================================

int main(int argc, char* argv[]) {

	char* filename = argv[1];

	// Local variables
	RawDataHeader header;
	sCameraCentData* pCentroidData;
	ScanList* pScanList;
	unsigned short frameIdx;
	FILE* pFile = 0;
	int t; 

	ostream* out = (argc < 3 ? (&cout) : (new std::ofstream(argv[2])));

	// Read header
	t = 0; 
	do {
		if (pFile) { CloseHandle(pFile); }

		memset(&header, 0, sizeof(RawDataHeader));
		pFile = VCX_ReadHeader(filename, &header);
		
		t++;
		if (t > 50) {
			exit(1);
		}
	} while (header.nWidth == 0 || header.nHeight == 0);

	// In-memory data object
	json data = json::array();

	// Read all frames
	while(true) {

		pScanList = VCX_ReadNextFrame(pFile, &header, &frameIdx, &pCentroidData);

		if (frameIdx >= header.nFrames) {
			break;
		}

		// TODO: Address errors in VCX_ReadNextFrame using retries... ?? Are there ever any errors?

		json frame;
		frame["width"] = header.nWidth;
		frame["height"] = header.nHeight;
		frame["fps"] = header.fCameraRate;
		frame["frame"] = frameIdx;
		frame["centroids"] = json::array();

		if (pCentroidData) {
			for (int j = 0; j < pCentroidData->nCentroids; j++) {

				json centroid;
				
				centroid["x"] = pCentroidData->Centroids[j].fX_raw;
				centroid["y"] = pCentroidData->Centroids[j].fY_raw;
				centroid["q"] = pCentroidData->Centroids[j].q_field;

				frame["centroids"].push_back(centroid);
			}
		}

		// Add to data
		data.push_back(frame);

		// Release frame
		if (pScanList) {
			ScanList_Free(pScanList);
		}
	}

	// Output
	(*out) << data.dump() << std::endl;

	// Clean up
	CloseHandle(pFile);
	if (argc == 3) {
		((ofstream*)out)->close();
		delete out;
	}
	
	// Done!
	exit(0);

	// TODO: DO NULL POINTER / DATA QUALITY CHECK~!!! THIS MIGHT BE WHAT IS CAUSING PYTHONG TO NOT BE ABLE TO LOAD MSGPACK!!

}
