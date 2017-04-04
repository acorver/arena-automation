
#ifndef BLOBS_H
#define BLOBS_H

#include "limits.h"
#include "data.h"

typedef struct RawDataHeader   /* <name.vc#> files */
{
    long  cpuFormat;
    short nWidth;
    short nHeight;
    long  nFrames;
    float fCameraRate;
    float fProcRate;
    long  lCameraInfo;
    short nTypes;
    char  types[16];

} RawDataHeader;

#if 1
typedef struct ScanList        /* List of raw scans */
{
    struct ScanList *next;
    int x1;
    int x2;
    int y;

} ScanList;
#endif

#if 0
class ScanList;
class ScanList        /* List of raw scans */
{
public:
    ScanList *next;
    int x1;
    int x2;
    int y;

};
#endif


typedef struct BlobList        /* Scans organized into blobs */
{
    struct BlobList *next;
    ScanList *scans;

} BlobList;



//extern BlobList  BlobListSpace_FIFO[256][MAX_VBOARDS][AMNM];
extern BlobList*** BlobListSpace_FIFO;  //[256][MAX_VBOARDS][AMNM];
extern BlobList* BlobListFIFO[256][MAX_VBOARDS];

int       BigScanListArray_Allocate();

void      ScanList_Reset();
BlobList* ScanList_ConvertToBlobs(ScanList* scans, BlobList* BlobListSpace);
int       ScanList_CalcCenter(ScanList *scans, float *xc, float *yc);
int       ScanList_Analysis(ScanList*, float*,float*, int*,int*,float*);
ScanList* ScanList_AllocateScan();
void      ScanList_FreeScan();
void      ScanList_Free(ScanList *scans);
ScanList* ScanList_Duplicate(ScanList* Src);

void      BlobList_Free(BlobList *blobs);

FILE*     VCX_ReadHeader(const char *filename, RawDataHeader *header);
ScanList* VCX_ReadNextFrame(FILE *handle, RawDataHeader *header, unsigned short *pFrame, sCameraCentData** centDataPtr);
void      VCX_PrintHeader(RawDataHeader *header);

int       VCX_TrimFile(const char* szFilesIn, int iFrame1, int iFrame2, int iSubSampleStep, const char* szFilesOut);
int EXPORTED VCX_TrimFiles(const char* szFilesIn, int iFrame1, int iFrame2, int iSubSampleStep, const char* szFilesOut);


ScanList* DecompressRawEdges(unsigned char *wBuffer, int nWords);
ScanList* Midas_DecompressRawData(int iFrame, int iBoard);

int       CalculateCentroids(int iFrame);

#endif
