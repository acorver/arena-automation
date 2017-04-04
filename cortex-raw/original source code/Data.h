
#ifndef DATA_H
#define DATA_H

#include "limits.h"

struct ScanList;
struct BlobList;

// Live data types and globals variables

// This structure now contains two variations of corrected
// centroid coordinates.  This is for the transition period
// between using old EVa lens correction and DLT techniques
// and my new lens correction.  They are mathematically
// equivalent, but the EVa way is very confusing.


typedef struct sCentroid
{
    float fX_raw;
    float fY_raw;
    float u;  // Slope values: tan(theta)
    float v;
    float fX; // Pixel values
    float fY;
    short iMarker;
    short nPixels;
    float quality;  // Raptor cameras provide a quality measure, 0.5 <= quality <= 1

} sCentroid;


typedef struct sCentroidData
{
    int       nCentroids;
    sCentroid *Centroids; // Must use the Allocate functions below.

} sCentData;


/**
The structure as used and calculated on the cameras
*/

typedef struct sCameraCentroid
{
    float fX_raw;
    float fY_raw;
    unsigned int q_field;   // pixel count and quality are packed in here

} sCameraCentroid;


typedef struct sCameraCentroidData
{
    int             nCentroids;
    sCameraCentroid Centroids[AMNM];

} sCameraCentData;

int TransferCamCentroidValues(int iCamera, sCameraCentData* CameraData, sCentroid EVaCentroids[], int applyMasks);

typedef int (*CentroidCompareFunc)(const void* a, const void* b);
int CentroidCompareIncreasingY(const sCentroid* a, const sCentroid* b);
void SortCentroids(sCentroid Centroids[], int nCentroids, CentroidCompareFunc cmpFunc);

typedef struct sCameraDataHeader
{
    char        szFilename[256];
    int         nFrames;
    int         nCameras;
    sCentData** CentData;
    ScanList*** RawScanPtrs;  // nFrames X nCameras
    BlobList*** Blobs;        // nFrames X nCameras
    int** bHWCentroids;

} sCameraDataHeader;

//extern sCentData  CentroidFIFO[256][MAX_VBOARDS];
extern sCentData  **CentroidFIFO;
extern int** g_bHWCentroids;
extern sCameraDataHeader LoadedCameraData;


int  CentroidData_AllocateFIFO();
void CentroidData_FreeFIFO();

sCentData *CentroidData_AllocateFrame(int nCamera=MAX_VBOARDS, int nMaxCentroids=AMNM);

int RawFiles_LoadAllData(char *szFilename, sCameraDataHeader *CameraDataHeader);
int RawFiles_FreeAllData(sCameraDataHeader *CameraDataHeader);


const int RawData_FIFOSIZE = 1 * 0x240000; //0x120000; // about 2.35 meg
extern __int64 nRawDataWords[MAX_VBOARDS];
//extern unsigned short wRawData[MAX_VBOARDS][RawData_FIFOSIZE]; // 2 bytes * 32 cams * 2.35 meg
extern unsigned short** wRawData;  // Allocated as [g_nMaxCameras][RawData_FIFOSIZE];
extern __int64 iFrameStarts[256][MAX_VBOARDS];
extern int iBoardStatus[MAX_VBOARDS];


#if 0
class cRawDataFIFO
{
public:
    cRawDataFIFO(int TotalNumberOfScans);
   ~cRawDataFIFO();

private:
    int m_RawData_FIFOSIZE;
    int nRawDataWords;
    unsigned short *wRawData;  // allocated as m_RawData_FIFOSIZE
    iFrameStarts[256];
};
#endif

#endif
