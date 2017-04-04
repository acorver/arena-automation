/*******************************************************************************
    Programmer     :  Ned Phipps, June 97
    Last Modified  :
    Functions      :

        1) VCX_ReadHeader
        2) VCX_ReadNextFrame
        3) VCX_WriteHeader
        4) VCX_CopyFrame
        
*******************************************************************************/

#include <windows.h>  // For CopyFile

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "standard.h"

//#include "nedstuff.h"
#include "blobs.h"
#include "orion.h"

#define NT_COMPILE

static int ReadCentroidsForFrame(FILE *handle, sCameraCentData* centDataStorage, FILE* fpCopy = NULL);

typedef struct Edge
{
    unsigned short x1;
    unsigned short x2;
    unsigned short y;
    /*struct Edge *next;*/

} Edge;


/* Enumeration containing ORION's data types */

typedef enum
{
    EDGE=1,       // OLD (used in very old .vc* files)
    CENTROF=2,    // ACTIVE (comes from Raptor cameras calculating HW centroids)  
    CENTROI=3,
    TWODPAF=4,
    TWODPAI=5,
    TDIMTRA=6,    // ACTIVE in both header and data
    TDIMANGLE=7,
    RAWEDGE=8,    // ACTIVE
    EVAASCII=9,
    EVTED=10,
    OTP3D=11,
    TDIMTRA_64=12,   // NEW (for 64 bit CamBitArray) in both header and data
    FILEHEADER=255

} DATATYPES;


/* Enumeration containing ORION's file header keywords */

typedef enum
{
    CPUFMTK=257,
    TYPEK,
    CAMRATEK,
    PROCRATEK,
    MAXXYK,
    SCALEK,
    CHANNK,
    ASPECK,
    COEFFK,
    GSSRK,
    SPCUNITSK,
    MARKERK,
    LINKK,
    SEGPERMASSK,
    SEGPERDISTK,
    EVENTNAMEK,
    EVENTLOCNK,
    CALINDEXK,
    COMMENTSK,
    CAMVPATTYPEK,
    REQUESTEDFRAMESK

} KEYWORDS;


typedef union DataUnion
{
    unsigned char  carray[4];
    unsigned short iarray[2];
    long  l;
    float f;

} DataUnion;


#define LITTLE_ENDIAN 1
#define BIG_ENDIAN    2

#ifdef SUN_COMPILE
#define NATIVE_CPU_FORMAT BIG_ENDIAN
#endif
#ifdef SGI_COMPILE
#define NATIVE_CPU_FORMAT BIG_ENDIAN
#endif
#ifdef NT_COMPILE
#define NATIVE_CPU_FORMAT LITTLE_ENDIAN
#endif

static int swapping = FALSE;

void Swap2Bytes(void *v)
{
    char *c = (char *)v;
    *c     ^= *(c+1);
    *(c+1) ^= *c;
    *c     ^= *(c+1);
}


int SetSwapping(int value)
{
    int old_value = swapping;
    swapping = value;
    return old_value;
}


void Reverse4Bytes(void *v)
{
    char *c1 = (char *)v;
    char *c2 = c1+1;
    char *c3 = c1+2;
    char *c4 = c1+3;

    *c1 ^= *c4;
    *c4 ^= *c1;
    *c1 ^= *c4;
    
    *c2 ^= *c3;
    *c3 ^= *c2;
    *c2 ^= *c3;
}


void Swap2Words(void *v)
{
    short *w1 = (short *)v;
    short *w2 = w1 + 1;

    *w1 ^= *w2;
    *w2 ^= *w1;
    *w1 ^= *w2;
}

void Swap4Bytes(void *v)
{
    char *c1 = (char *)v;
    char *c2 = c1+1;
    char *c3 = c1+2;
    char *c4 = c1+3;

/*printf("Swap4Bytes: %d %d %d %d\n", (int)*c1, (int)*c2,(int)*c3,(int)*c4);*/

    *c1 ^= *c2;
    *c2 ^= *c1;
    *c1 ^= *c2;

    *c3 ^= *c4;
    *c4 ^= *c3;
    *c3 ^= *c4;
}

void Print32Bits(long bits)
{
    int bit;
    
    for (bit=0 ; bit<32 ; bit++)
    {
        if (bits & (1<<bit))
        {
            printf("1");
        }
        else
        {
            printf("0");
        }
    }
}


int ExamineTracks(FILE *handle)
{
    struct
    {
    	long  ID;
        float x;
        float y;
        float z;
        float residual;
        long  bitmap;

    } track;
    int n_tracks;
    int i_track;
    int numread;
    DataUnion four_bytes;
    
    numread = fread(&four_bytes, 4, 1, handle);
    if (swapping)
        Reverse4Bytes(&four_bytes);
    n_tracks = (four_bytes.l - 3) / 6;
    
    for (i_track=0 ; i_track<n_tracks ; i_track++)
    {
        numread = fread(&track, sizeof(track), 1, handle);

        if (feof(handle))
        {
            return TRUE;
        }

        printf("%ld, (%f,%f,%f) %f, ",
            track.ID,
            track.x,
            track.y,
            track.z,
            track.residual);            
        Print32Bits(track.bitmap);
        printf("\n");
    }

    return TRUE;
}

#if 0
ScanList* ScanList_AllocateScan()
{
    return (ScanList*)malloc(sizeof(ScanList));
}

void ScanList_Free(ScanList *Scan)
{
    if (Scan != NULL)
    {
        free(Scan);
    }
}

void PopupNotice(char* str, char* title)
{
    printf(str);
    printf("\n");
}
#endif


GLOBAL ScanList *DecompressRawEdges(unsigned short *buffer, int nwords)
{
    unsigned short *two_bytes=buffer;
    unsigned int ioperand, iopcode;
    ScanList *scans=NULL;
    ScanList *scan;
    ScanList *new_scan;
    ScanList *start_of_row=NULL;

    nwords++;
    while (--nwords)
    {
        if (start_of_row != NULL)
        {
            printf("Error reading .vc* file:  Missing y-values at end.\n");
        }
        break;

        iopcode = (*two_bytes) >> 12;
        ioperand = (*two_bytes) & 0x0FFF;

        two_bytes++;

#if 0
if (ioperand < 0 || ioperand > 2000)
   printf("ioptcode, ioperand = %d, %d\n", iopcode, ioperand);
#endif

        switch (iopcode)
        {
        case 14: /* xleft */
            //new_scan = (ScanList *)malloc(sizeof(ScanList));
            new_scan = ScanList_AllocateScan();
            new_scan->x1 = new_scan->x2 = ioperand;
            new_scan->y = 0xFFFF;
            new_scan->next = scans;
            scans = new_scan;

            if (start_of_row == NULL)
            {
                start_of_row = new_scan;
            }
            break;

        case 10: /* xright */
            new_scan->x2 = ioperand;
            break;

        case 8: /* y-value */
        /*  arrives AFTER all the xleft,xright pairs for the scanline  */

            for (scan=scans ; scan!=start_of_row->next ; scan=scan->next)
            {
                scan->y = ioperand;
            }
            start_of_row = NULL;
            break;

        case 7:
            break;

        default:
            printf("Undefined opcode reading edges.\n");
            ScanList_Free(scans);
            return NULL;
        }
    }

/*  EOF is a valid exit criterion  */
    return scans;
}


ScanList *ExamineEdges(FILE *handle, int count)
{
    unsigned short two_bytes;
//    unsigned int xleft,xright,y;
    unsigned int ioperand, iopcode;
    int numread;
    ScanList *scans=NULL;
    ScanList *scan;
    ScanList *new_scan;
    ScanList *start_of_row=NULL;
    
/*printf("Examining edges\n");*/

    count = 2*count - 6;

    while (count--)
    {
        if (feof(handle))
        {
            printf("ERROR: Premature EOF\n");
            ScanList_Free(scans);
            return NULL;
        }
        
        numread = fread(&two_bytes, 2, 1, handle);
        if (swapping)
            Swap2Bytes(&two_bytes);

#if 0
    /*  Exit criterion is reaching a 4-byte sequence of zeros  */
        if (two_bytes == 0)
        {
        /*  Two more zero bytes to skip past  */
            numread = fread(&two_bytes, 2, 1, handle);
            return TRUE;
        }
#endif

        iopcode = two_bytes >> 12;
        ioperand = two_bytes & 0x0FFF;

        switch (iopcode)
        {
        case 14: /* xleft */
            //new_scan = (ScanList *)malloc(sizeof(ScanList));
            new_scan = ScanList_AllocateScan();

            new_scan->x1 = new_scan->x2 = ioperand;
            new_scan->y = 0xFFFF;
            new_scan->next = scans;
            scans = new_scan;

            if (start_of_row == NULL)
            {
                start_of_row = new_scan;
            }
            break;

        case 10: /* xright */
            new_scan->x2 = ioperand;
            break;

        case 8: /* y-value */

        /*  arrives AFTER all the xleft,xright pairs for the scanline  */
        
            for (scan=scans ; scan!=start_of_row->next ; scan=scan->next)
            {
                scan->y = ioperand;
            }
            start_of_row = NULL;
            break;

        case 7:
            break;

        default:
            printf("Undefined opcode reading edges.\n");
            ScanList_Free(scans);
            return NULL;
        }
    }

/*  EOF is a valid exit criterion  */

    return scans;
}


int ReadRawEdges(FILE *handle, Edge *scans, int max_scans)
{
    unsigned short two_bytes;
    unsigned int ioperand, iopcode;
    int i_start_of_row=0;
    int i_scan;
    int n_scans=0;

    while (n_scans < max_scans)
    {
        fread(&two_bytes, 2, 1, handle);
        if (feof(handle))
        {
        /*  EOF is a valid exit criterion  */
            break;
        }

        /*if (swapping)*/
#ifndef NT_COMPILE
            Swap2Bytes(&two_bytes);
#endif

    /*  Exit criterion is reaching a 4-byte sequence of zeros  */
        if (two_bytes == 0)
        {
        /*  Two more zero bytes to skip past  */
            fread(&two_bytes, 2, 1, handle);
            if (i_start_of_row != n_scans)
            {
                printf("Error reading .vc* file:  Missing y-values at end.\n");
                return 0;
            }
            break;
        }

        iopcode = two_bytes >> 12;
        ioperand = two_bytes & 0x0FFF;

        /*printf("ioptcode: %2d, ioperand: %4d\n", iopcode, ioperand);*/

        switch (iopcode)
        {
        case 14: /* xleft */
            scans[n_scans].x1 = scans[n_scans].x2 = ioperand;
            scans[n_scans].y = 0xFFFF;
            n_scans++;
            break;

        case 10: /* xright */
            scans[n_scans-1].x2 = ioperand;
            break;

        case 8: /* y-value */
        /*  arrives AFTER all the xleft,xright pairs for the scanline  */

            for (i_scan=i_start_of_row ; i_scan<n_scans ; i_scan++)
            {
                scans[i_scan].y = ioperand;
            }
            i_start_of_row = n_scans; /* next one will start another row */
            break;

        case 7:
            break;

        default:
            printf("Undefined opcode reading edges.\n");
            return 0;
        }
    }

#if 0
/* For compatibility sake */

    for (i_scan=0 ; i_scan<n_scans-1 ; i_scan++)
    {
        scans[i_scan].next = &scans[i_scan+1];
    }
    scans[n_scans-1].next = NULL;
#endif

    return n_scans;

}


ScanList *ExamineRawEdges(FILE *handle)
{
    unsigned short two_bytes;
    //unsigned int xleft,xright,y;
    unsigned int ioperand, iopcode;
    int numread;
    ScanList *scans=NULL;
    ScanList *scan;
    ScanList *new_scan;
    ScanList *start_of_row=NULL;

/*printf("Examining edges\n");*/
        
    while (1)
    {
        numread = fread(&two_bytes, 2, 1, handle);
        if (feof(handle))
            break;

        if (swapping)
            Swap2Bytes(&two_bytes);

    /*  Exit criterion is reaching a 4-byte sequence of zeros  */
        if (two_bytes == 0)
        {
        /*  Two more zero bytes to skip past  */
            numread = fread(&two_bytes, 2, 1, handle);
            if (start_of_row != NULL)
            {
                printf("Error reading .vc* file:  Missing y-values at end.\n");
            }
            break;
        }

        iopcode = two_bytes >> 12;
        ioperand = two_bytes & 0x0FFF;
/*printf("ioptcode: %2d, ioperand: %4d\n", iopcode, ioperand);*/

/*printf("%d ", ioperand);*/

#if 0
if (ioperand < 0 || ioperand > 2000)
   printf("ioptcode, ioperand = %d, %d\n", iopcode, ioperand);
#endif

        switch (iopcode)
        {
        case 14: /* xleft */
            //new_scan = (ScanList *)malloc(sizeof(ScanList));
            new_scan = ScanList_AllocateScan();
            new_scan->x1 = new_scan->x2 = ioperand;
            new_scan->y = 0xFFFF;
            new_scan->next = scans;
            scans = new_scan;

            if (start_of_row == NULL)
            {
                start_of_row = new_scan;
            }
            break;

        case 10: /* xright */
            new_scan->x2 = ioperand;
            break;

        case 8: /* y-value */
        /*  arrives AFTER all the xleft,xright pairs for the scanline  */
            if (start_of_row == NULL)
            {
//                ScanList_Free(scans);
//                PopupNotice("ERROR: Corrupt Data File");
//                return NULL;
                break;
            }

            for (scan=scans ; scan!=start_of_row->next ; scan=scan->next)
            {
                scan->y = ioperand;
            }
            start_of_row = NULL;
            break;

        case 7:
            break;

        default:
            printf("Undefined opcode reading edges.\n");
            ScanList_Free(scans);
            return NULL;
        }
    }

/*  EOF is a valid exit criterion  */
    return scans;
}


/*========================================================================
//
//  VCX_PrintHeader
//
//  Description:
//      This function prints the header info to stdout.
//
//  Arguments:
//      RawDataHeader *header : The header read.
//
//  Returns: (void)
//
//----------------------------------------------------------------------*/
void VCX_PrintHeader(RawDataHeader *header)
{
    printf("CPU Format     : %3d\n", header->cpuFormat);
    printf("Image Width    : %3d\n", header->nWidth);
    printf("Image Height   : %3d\n", header->nHeight);
    printf("Frames         : %3d\n", header->nFrames);
    printf("Camera Rate    : %.4f (Frames/Sec)\n", header->fCameraRate);
    printf("Processor Rate : %.4f(Frames/Sec)\n", header->fProcRate);
    printf("N Types        : %3d\n", header->nTypes);
    printf("Data Type      : %3d\n", (int)header->types[0]);
}


/*========================================================================
//
//  VCX_ReadHeader
//
//  Description:
//      This function reads the header of raw data (<name.vc#>) file.
//
//  Arguments:
//      char        *filename : The file to read.
//      RawDataHeader *header : The header read.
//
//  Returns:
//      FILE *: The handle to the file, ready for reading frames.
//
//----------------------------------------------------------------------*/
FILE* VCX_ReadHeader(const char *filename, RawDataHeader *header)
{
    DataUnion four_bytes;
    long  dummy_long;
    //short dummy_short;
    float dummy_float;
    FILE *handle;
    short key,count;
    short n_types=0;
/*    char  types[16];*/
    long  numread;
    int   i;
    //int   cpu_format;
    char  marker_name[80];
    int   marker_number;
    int   marker_color;
    int   marker_type;
    int   n_markers=0;
    char  link_name[80];
    int   link_number;
    int   link_color;
    int   link_type;
    int   link_obj1;
    int   link_obj2;
    int   n_links=0;
    float fCameraRate;
    float fProcessorRate;

    memset(header, 0, sizeof(RawDataHeader));

    if ((handle=fopen(filename, "rb")) == NULL)
    {
        /*printf("ERROR: Unable to open file \"%s\"\n", filename);*/
        return NULL;
    }

    swapping = FALSE;

    fread(&four_bytes, 4, 1, handle);
    if (four_bytes.l != 0)
    {
        printf("FILE FORMAT ERROR\n");
        fclose(handle);
        return NULL;
    }

    fread(&four_bytes, 4, 1, handle);
    if (four_bytes.l != 0xFFFFFFFF)
    {
        printf("FILE FORMAT ERROR 2\n");
        fclose(handle);
        return NULL;
    }

    fread(&four_bytes, 4, 1, handle);
    if (swapping)
        Swap4Bytes(&four_bytes);

#ifdef NP_DEBUG
    printf("Header size: %ld x 4 bytes\n", four_bytes.l);
    printf("\n");
#endif

    while (!feof(handle))
    {
        fread(&key,   2, 1, handle);
        if (swapping)
            Swap2Bytes(&key);

        fread(&count, 2, 1, handle);
        if (swapping)
            Swap2Bytes(&count);

        if (key==0 && count==0)
            break;

/*        printf("key,count = %3x,%x  ", key, count);*/

        switch (key)
        {
        /*
         *  Special case: key == 0x0101 so byte swapping has no effect.
         *  Also the next four bytes read in here are all the same.
         */
        case CPUFMTK: /* 0x0101 */
            numread = fread(&four_bytes, 4, 1, handle);
            header->cpuFormat = four_bytes.carray[0];
            //cpu_format = four_bytes.carray[0];
            if (header->cpuFormat != NATIVE_CPU_FORMAT)
                swapping = TRUE;             
            break;

        case TYPEK:
            for (i=0 ; i<count ; i++)
            {
                fread(&four_bytes, 4, 1, handle);
                /*if (swapping)*/
                /* the one real type is in the second byte */
                /* so let's just move it to the first */

                    Swap4Bytes(&four_bytes);

                header->types[header->nTypes++] = four_bytes.carray[0];
                header->types[header->nTypes++] = four_bytes.carray[1];
                header->types[header->nTypes++] = four_bytes.carray[2];
                header->types[header->nTypes++] = four_bytes.carray[3];
            }
            break;

        case CAMRATEK:
            numread = fread(&fCameraRate, 4, 1, handle);
            if (swapping)
                Reverse4Bytes(&fCameraRate);
            header->fCameraRate = fCameraRate;
            break;

        case PROCRATEK:
            numread = fread(&fProcessorRate, 4, 1, handle);
            if (swapping)
                Reverse4Bytes(&fProcessorRate);
            header->fProcRate = fProcessorRate;
            break;

        case MAXXYK:
            numread = fread(&four_bytes, 4, 1, handle);
            if (swapping)
                Swap4Bytes(&four_bytes);

            header->nWidth = four_bytes.iarray[0];
            header->nHeight = four_bytes.iarray[1];
            break;

        case SCALEK:
            numread = fread(&dummy_long, 4, 1, handle);
            break;

        case CHANNK:
            numread = fread(&dummy_long, 4, 1, handle);
            if (swapping)
                Swap4Bytes(&dummy_long);
            break;

        case ASPECK:
            numread = fread(&dummy_float, 4, 1, handle);
            if (swapping)
                Swap4Bytes(&dummy_float);
            break;

        case MARKERK:
            numread = fread(&four_bytes, 4, 1, handle);
            if (swapping)
                Swap4Bytes(&four_bytes);
            marker_number = four_bytes.iarray[0];
            marker_color = four_bytes.carray[2];
            marker_type = four_bytes.carray[3];

            for (i=0 ; i<count-1 ; i++)
            {
                numread = fread(&four_bytes, 4, 1, handle);
                memcpy(marker_name+4*i, &four_bytes, 4);
            }

            n_markers++;
            break;

        case LINKK:
            numread = fread(&four_bytes, 4, 1, handle);
            if (swapping)
                Swap4Bytes(&four_bytes);

            link_number = four_bytes.iarray[0];  /* 1..n */
            link_obj1 = four_bytes.iarray[1];

            numread = fread(&four_bytes, 4, 1, handle);
            if (swapping)
                Swap4Bytes(&four_bytes);

            link_obj2 = four_bytes.iarray[0];
            link_color = four_bytes.iarray[2];
            link_type = four_bytes.iarray[3];
            
            for (i=0 ; i<count-2 ; i++)
            {
                numread = fread(&four_bytes, 4, 1, handle);
                memcpy(link_name+4*i, &four_bytes, 4);
            }

            n_links++;
            break;

        case COEFFK:
            printf("Coefficients not handled yet\n");
            printf("This probably will cause further errors\n");
            break;

        case GSSRK:
            printf("Greyscale sample ratios keyword not handled yet\n");
            printf("This probably will cause further errors\n");
            break;

        case CAMVPATTYPEK:
            numread = fread(&four_bytes, 4, 1, handle);
            if (swapping)
                Swap4Bytes(&four_bytes);    
            /*
             *  VPAT = four_bytes.iarray[1]
             *  CamType = four_bytes.iarray[0]
             */
#if 0
            printf("Camera VPAT: %d.%d, Camera type: %d (sync rate: %d)\n",
                (int)four_bytes.carray[2],
                (int)four_bytes.carray[3],
                (int)four_bytes.carray[1],
                (int)four_bytes.carray[0]);
#endif
            header->lCameraInfo = four_bytes.l;
            break;

        case REQUESTEDFRAMESK:        
            numread = fread(&four_bytes, 4, 1, handle);
            if (swapping)
                Swap4Bytes(&four_bytes);
            else
            {
                unsigned short temp = four_bytes.iarray[0];
                four_bytes.iarray[0] = four_bytes.iarray[1];
                four_bytes.iarray[1] = temp;
            }

            /*header->nFrames = four_bytes.iarray[1];*/
            header->nFrames = four_bytes.l;
            break;

        case 0: /* unreachable */
            printf("This should be the end of the header\n");
            break;

        default:
            {
               char str[256];
                //printf("Unknown key: %d\n", key);
                //printf("count = %d\n", count);
                fclose(handle);
                handle = NULL;
                sprintf(str, "Error reading file header:\n%s", filename);
                PopupNotice(str);
                //SendThisMessage(MSG_STRING, 0, (long)str);
                return NULL;
            }
        }
    }

    return handle;
}

// Temporarily holds a frame of centroids from VCX_ReadNextFrame
static sCameraCentData tempCentroidStorage;

/*========================================================================
//
//  VCX_ReadNextFrame
//
//  Description:
//      This function reads the next unread frame of raw data from
//      a .vc* file.
//
//  Arguments:
//      FILE          *handle : Handle to file as returned by
//                              VCX_ReadHeader or previous call to this
//                              function.
//      RawDataHeader *header : The header as returned from VCX_ReadHeader.
//
//  Returns:
//      ScanList *: A complete list of raw scans for one frame.
//
//----------------------------------------------------------------------*/
ScanList* VCX_ReadNextFrame(FILE *handle, RawDataHeader *header, unsigned short *pFrame, sCameraCentData** centDataPtr)
{
    int numread;
    DataUnion four_bytes;
    int data_type;
    unsigned short frame;
    int block_size;  
    ScanList *scans=NULL;

/*  Get the first four bytes of the new frame  */
/*  but be sure to read past any left-over zero delimiters  */

    if (centDataPtr) *centDataPtr = NULL;

    do
    {
        numread = fread(&four_bytes, 4, 1, handle);
        if (feof(handle))
        {
            return NULL;
        }

    } while (four_bytes.l == 0);

/* The data_type is in the second byte */
/* No swizzling is needed to get this value */

    data_type = four_bytes.carray[1];

    if (swapping)
        Swap4Bytes(&four_bytes);

    frame = four_bytes.iarray[1];
    *pFrame = frame;

#if 0
    printf("\n");
    printf("Data type: %d\n", data_type);
    printf("Frame: %d\n", frame);
#endif

    if (data_type == EDGE)
    {
        if (header->types[0] == EDGE)
        {
        /*  Four more bytes that include a blocksize  */
            numread = fread(&four_bytes, 4, 1, handle);
            if (swapping)
                Swap4Bytes(&four_bytes);

            block_size = four_bytes.iarray[1];

            /*printf("Block size: %d\n", block_size);*/

            scans = ExamineEdges(handle, block_size);
        }
        else
        if (header->types[0] == RAWEDGE)
        {
            scans = ExamineRawEdges(handle);
        }
    }
    else if (data_type == CENTROF)
    {
        if (ReadCentroidsForFrame(handle, &tempCentroidStorage) == OK)
        {
            if (centDataPtr) *centDataPtr = &tempCentroidStorage;
        }
    }

    return scans;
}

/*========================================================================
//
//  VCX_ReadNextFrame2
//
//  Description:
//      This function reads the next unread frame of raw data from
//      a .vc* file.
//
//  Arguments:
//      FILE          *handle : Handle to file as returned by
//                              VCX_ReadHeader or previous call to this
//                              function.
//      RawDataHeader *header : The header as returned from VCX_ReadHeader.
//
//  Returns:
//      ScanList *: A complete list of raw scans for one frame.
//
//----------------------------------------------------------------------*/
int VCX_ReadNextFrame2(
  FILE *handle,
  int file_data_type,
  Edge *scans,
  int max_scans)
{
    int numread;
    DataUnion four_bytes;
    int frame_data_type;
    int frame;
    int block_size;
    int n_scans;

/*  Get the first four bytes of the new frame  */
/*  but be sure to read past any left-over zero delimiters  */

    do
    {
        numread = fread(&four_bytes, 4, 1, handle);
        if (feof(handle))
        {
            return 0;
        }

    } while (four_bytes.l == 0);

/* The data_type is in the second byte */
/* No swizzling is needed to get this value */

    frame_data_type = four_bytes.carray[1];

    /*if (swapping)*/
#ifndef NT_COMPILE
        Swap4Bytes(&four_bytes);
#endif

    frame = four_bytes.iarray[1];
    
#if 0
    printf("\n");
    printf("Data type: %d\n", frame_data_type);
    printf("Frame: %d\n", frame);
#endif

    if (frame_data_type != EDGE  &&  frame_data_type != RAWEDGE)
    {
        printf("Error reading frame header\n");
        return 0;
    }

    if (file_data_type == EDGE)
    {
    /*  Four more bytes that include a blocksize  */
        numread = fread(&four_bytes, 4, 1, handle);
        /*if (swapping)*/
#ifndef NT_COMPILE
            Swap4Bytes(&four_bytes);
#endif

        block_size = four_bytes.iarray[1];
        /*printf("Block size: %d\n", block_size);*/
    }

    n_scans = ReadRawEdges(handle, scans, max_scans);

    return n_scans;
}


LOCAL void write_header(
  FILE *handle,
  short type,
  short count,
  void *_data,
  int swapping)
{
    long *data = (long *)_data;
    short two_bytes;
    long four_bytes;
    int i;

    if (swapping)
    {
        two_bytes = type;
        Swap2Bytes(&two_bytes);
        fwrite(&two_bytes,  2, 1, handle);

        two_bytes = count;
        Swap2Bytes(&two_bytes);
        fwrite(&two_bytes,  2, 1, handle);

        for (i=0 ; i<count ; i++)
        {
            four_bytes = *data;
            /*Swap4Bytes(&four_bytes);*/
            fwrite(&four_bytes, 4, 1, handle);
        }
    }
    else
    {
        fwrite(&type, 2, 1, handle);
        fwrite(&count, 2, 1, handle);
        while (count--)
        {
            fwrite(data, 4, 1, handle);
            data++;
        }
    }
}


GLOBAL void VCX_WriteHeader(FILE *handle, RawDataHeader *header)
{
    long four_bytes;
    short two_bytes;
    int swapping = FALSE;
    //int i;
    
    if (header->cpuFormat != NATIVE_CPU_FORMAT)
    {
        swapping = TRUE;
    }    

    four_bytes = 0x00000000;
    fwrite(&four_bytes, 4, 1, handle);
    four_bytes = 0xFFFFFFFF;
    fwrite(&four_bytes, 4, 1, handle);
    
    /*  HEADER SIZE  */
    
    four_bytes = 16 + header->nTypes/4; /* 17 */
    if (swapping) Swap4Bytes(&four_bytes);
    fwrite(&four_bytes, 4, 1, handle);

    /*  CPU FORMAT  */
    
    two_bytes = CPUFMTK;
    fwrite(&two_bytes, 2, 1, handle);
    two_bytes = 0;
    fwrite(&two_bytes, 2, 1, handle);
    four_bytes = 0x01010101;
    fwrite(&four_bytes, 4, 1, handle);

    four_bytes = header->types[0];
    /*if (swapping)*/ Swap4Bytes(&four_bytes);
    write_header(handle, TYPEK,            1, &four_bytes, swapping);

    four_bytes = *(long*)&header->fCameraRate;
    if (swapping) Reverse4Bytes(&four_bytes);
    write_header(handle, CAMRATEK,         1, &four_bytes, swapping);

    four_bytes = *(long*)&header->fProcRate;
    if (swapping) Reverse4Bytes(&four_bytes);
    write_header(handle, PROCRATEK,        1, &four_bytes, swapping);

    four_bytes = *(long*)&header->nWidth;
    if (swapping) Swap4Bytes(&four_bytes);
    write_header(handle, MAXXYK,           1, &four_bytes, swapping);

    four_bytes = *(long*)&header->lCameraInfo;
    if (swapping) Swap4Bytes(&four_bytes);
    write_header(handle, CAMVPATTYPEK,     1, &four_bytes, swapping);

    four_bytes = *(long*)&header->nFrames;
    if (swapping) Swap4Bytes(&four_bytes);
    else
    {
        Swap2Words(&four_bytes);
    }
    write_header(handle, REQUESTEDFRAMESK, 1, &four_bytes, swapping);

/*  Close with four zeros  */
    four_bytes = 0;
    fwrite(&four_bytes, 4, 1, handle);
}


GLOBAL void VCX_CopyFrame(RawDataHeader *header, FILE *infile, FILE *outfile, int iFrameOutput)
{
    DataUnion four_bytes;
    short two_bytes;
    int count;
    int block_size;
    int swapping=FALSE;
    short data_type;
    short frame;

    if (header->cpuFormat != NATIVE_CPU_FORMAT)
        swapping = TRUE;             
#if 0
    if (header->cpuFormat == 1)
    {
        swapping = TRUE;
    }
#endif

/*  Get the first four bytes of the new frame  */
/*  but be sure to read past any left-over zero delimiters  */

    do
    {
        fread(&four_bytes, 4, 1, infile);
        fwrite(&four_bytes, 4, 1, outfile);

    } while (four_bytes.l == 0);

    if (swapping)
        Swap4Bytes(&four_bytes);

    data_type = four_bytes.carray[1];
    frame = four_bytes.iarray[1];

    //frame -= nFramesOffset;
	frame = iFrameOutput;
    four_bytes.iarray[1] = frame;
    fseek(outfile, -4, SEEK_CUR);
    fwrite(&four_bytes, 4, 1, outfile);

    if (data_type == EDGE)
    {
        if (header->types[0] == EDGE)
        {
            /*  THIS VARIATION HAS A BLOCKSIZE AT THE START  */

            fread(&four_bytes, 4, 1, infile);
            fwrite(&four_bytes, 4, 1, outfile);
            if (swapping)
                Swap4Bytes(&four_bytes);

            block_size = four_bytes.iarray[1];

            count = 2*block_size - 6;

            while (count--)
            {        
                fread(&two_bytes, 2, 1, infile);
                fwrite(&two_bytes, 2, 1, outfile);
            }
        }
        else if (header->types[0] == RAWEDGE)
        {
            /*  THIS VARIATION IS "NULL TERMINATED"  */

            while (!feof(infile))
            {
                fread(&two_bytes, 2, 1, infile);
                fwrite(&two_bytes, 2, 1, outfile);
                if (swapping)
                    Swap2Bytes(&two_bytes);
                /*
                Exit criterion is reaching a 4-byte sequence of zeros
                Note: 2 zeroes never occurs without the next 2 following immediately
                */
                if (two_bytes == 0)
                {
                    /*  Two more zero bytes to skip past  */
                    fread(&two_bytes, 2, 1, infile);
                    fwrite(&two_bytes, 2, 1, outfile);
                    break;
                }
            }
        }
    }
    else if (data_type == CENTROF)
    {
        ReadCentroidsForFrame(infile, &tempCentroidStorage, outfile);
    }
}


GLOBAL void VCX_DummyReadFrame(RawDataHeader *header, FILE *infile)
{
    DataUnion four_bytes;
    short two_bytes;
    int count;
    int block_size;
    int swapping=FALSE;
    short data_type;
    short frame;

    if (header->cpuFormat != NATIVE_CPU_FORMAT)
        swapping = TRUE;             
#if 0
    if (header->cpuFormat == 1)
    {
        swapping = TRUE;
    }
#endif

/*  Get the first four bytes of the new frame  */
/*  but be sure to read past any left-over zero delimiters  */

    do
    {
        fread(&four_bytes, 4, 1, infile);

    } while (four_bytes.l == 0);

    if (swapping)
        Swap4Bytes(&four_bytes);

    data_type = four_bytes.carray[1];
    frame = four_bytes.iarray[1];

    if (data_type == EDGE)
    {
        if (header->types[0] == EDGE)
        {
            /*  THIS VARIATION HAS A BLOCKSIZE AT THE START  */

            fread(&four_bytes, 4, 1, infile);
            if (swapping)
                Swap4Bytes(&four_bytes);

            block_size = four_bytes.iarray[1];

            count = 2*block_size - 6;

            while (count--)
            {        
                fread(&two_bytes, 2, 1, infile);
            }
        }
        else if (header->types[0] == RAWEDGE)
        {
            /*  THIS VARIATION IS "NULL TERMINATED"  */

            while (!feof(infile))
            {
                fread(&two_bytes, 2, 1, infile);
                if (swapping)
                    Swap2Bytes(&two_bytes);
                /*
                Exit criterion is reaching a 4-byte sequence of zeros
                Note: 2 zeroes never occurs without the next 2 following immediately
                */
                if (two_bytes == 0)
                {
                    /*  Two more zero bytes to skip past  */
                    fread(&two_bytes, 2, 1, infile);
                    break;
                }
            }
        }
    }
    else if (data_type == CENTROF)
    {
        ReadCentroidsForFrame(infile, &tempCentroidStorage);
    }
}



int VCX_TrimFile(const char* szFileIn, int iFrame1, int iFrame2, int iSubSampleStep, const char* szFileOut)
{
    int retval = OK;
    FILE *FileIn = NULL;
    FILE *FileOut = NULL;
    RawDataHeader Header;
    int iFrame;

    FileIn = VCX_ReadHeader(szFileIn, &Header);

    if (FileIn == NULL)
    {
//        printf("Unable to open file %s\n", szFileIn);
        retval = ERRFLAG;
        goto DONE;
    }

    FileOut = fopen(szFileOut, "w+b");
    if (FileOut == NULL)
    {
        printf("Unable to open file %s\n", szFileOut);
        retval = ERRFLAG;
        goto DONE;
    }

	// Read to the starting frame
    for (iFrame=0 ; iFrame<iFrame1 ; iFrame++)
    {
        VCX_DummyReadFrame(&Header, FileIn);
    }

    Header.nFrames = iFrame2 - iFrame1 + 1;
    VCX_WriteHeader(FileOut, &Header);

	int nFramesWritten = 0;
	for (iFrame=iFrame1 ; iFrame<=iFrame2 ; iFrame+=iSubSampleStep)
    {
        VCX_CopyFrame(&Header, FileIn, FileOut, nFramesWritten+1);
		nFramesWritten++;

		int nSkip = iSubSampleStep - 1;
		for (int iSkip=0 ; iSkip<nSkip ; iSkip++)
		{
		    VCX_DummyReadFrame(&Header, FileIn);
		}
    }

	// Rewrite the header
	Header.nFrames = nFramesWritten;
	Header.fCameraRate /= iSubSampleStep;
	Header.fProcRate /= iSubSampleStep;
	fseek(FileOut, 0, SEEK_SET);
    VCX_WriteHeader(FileOut, &Header);

DONE:

    if (FileIn  != NULL) fclose(FileIn);
    if (FileOut != NULL) fclose(FileOut);

    return retval;
}

int EXPORTED VCX_TrimFiles(const char* szFilesIn, int iFrame1, int iFrame2, int iSubSampleStep, const char* szFilesOut)
{
    char szNameIn[256];
    char szNameOut[256];
    char szBaseNameIn[256];
    char szBaseNameOut[256];
    int length;
    int iCamera;
    int nFilesTrimmed = 0;
    int bOverwriting;
    char szTempBaseNameOut[256] = "__TEMP_VCX_OUT.vc";
    char szTempNameOut[256] = "__TEMP_VCX_OUT.vc";

    if (stricmp(szFilesIn, szFilesOut) == 0)
    {
        bOverwriting = TRUE;
    }
    else
    {
        bOverwriting = FALSE;
    }


    strcpy(szBaseNameIn, szFilesIn);
    length = strlen(szBaseNameIn);
    szBaseNameIn[length-1] = 0; // Trim off the '1' at the end of .vc1

 // Trim to the new name or a temp name if overwriting
    if (bOverwriting)
    {
        strcpy(szBaseNameOut, szTempBaseNameOut);
    }
    else
    {
        strcpy(szBaseNameOut, szFilesOut);
        length = strlen(szBaseNameOut);
        szBaseNameOut[length-1] = 0; // Trim off the '1' at the end of .vc1
    }

    for (iCamera=0 ; iCamera<g_nMaxCameras ; iCamera++)
    {
        sprintf(szNameIn,  "%s%d", szBaseNameIn,  iCamera+1);
        sprintf(szNameOut, "%s%d", szBaseNameOut, iCamera+1);

        if (VCX_TrimFile(szNameIn, iFrame1, iFrame2, iSubSampleStep, szNameOut) == OK)
        {
            nFilesTrimmed++;
        }
    }

 // if overwriting, then cleanup the tempname files.
    if (bOverwriting)
    {
        for (iCamera=0 ; iCamera<g_nMaxCameras ; iCamera++)
        {
            sprintf(szNameIn,  "%s%d", szBaseNameIn,  iCamera+1);
            sprintf(szNameOut, "%s%d", szBaseNameOut, iCamera+1);
            sprintf(szTempNameOut, "%s%d", szTempBaseNameOut, iCamera+1);
            remove(szNameIn);
            CopyFile(szTempNameOut, szNameIn, TRUE);
            remove(szTempNameOut);
        }
    }

    return nFilesTrimmed;
}

#if 0
int EXPORTED VCX_TrimFiles(const char* szFilesIn, int iFrame1, int iFrame2, const char* szFilesOut)
{
    char szNameIn[256];
    char szNameOut[256];
    char szBaseNameIn[256];
    char szBaseNameOut[256];
    int length;
    int iCamera;
    int nFilesTrimmed = 0;

    strcpy(szBaseNameIn, szFilesIn);
    length = strlen(szBaseNameIn);
    szBaseNameIn[length-1] = 0; // Trim off the '1' at the end of .vc1

    strcpy(szBaseNameOut, szFilesOut);
    length = strlen(szBaseNameOut);
    szBaseNameOut[length-1] = 0; // Trim off the '1' at the end of .vc1

    for (iCamera=0 ; iCamera<64 ; iCamera++)
    {
        sprintf(szNameIn,  "%s%d", szBaseNameIn,  iCamera+1);
        sprintf(szNameOut, "%s%d", szBaseNameOut, iCamera+1);

        if (VCX_TrimFile(szNameIn, iFrame1, iFrame2, szNameOut) == OK)
        {
            nFilesTrimmed++;
        }
    }

    return nFilesTrimmed;
}
#endif

// This doesn't read a complete frame of centroids, it reads
// the centroids within a frame, after the frame header has already been read
static int ReadCentroidsForFrame(FILE *handle, sCameraCentData* centDataStorage, FILE* fpCopy)
{
    int rc = ERRFLAG;
    int nCentroids = 0;
    int readCount = 0;
    sCameraCentroid tmpCentroid;

    // Read the count
    if (fread(&nCentroids, sizeof(int), 1, handle) == 1 && nCentroids >= 0)
    {
        centDataStorage->nCentroids = MIN(nCentroids, AMNM);

        // Read the centroids
        readCount = (int)fread(&centDataStorage->Centroids[0], sizeof(sCameraCentroid), 
                               centDataStorage->nCentroids, handle);
        
        if (readCount == centDataStorage->nCentroids)
        {
            rc = OK;
            nCentroids -= centDataStorage->nCentroids;

            // Should we copy these centroids to an output file
            // Probably should have some error checking here, but if the copy
            // fails, should the whole function be considered to have failed?
            if (fpCopy)
            {
                fwrite(&readCount, sizeof(int), 1, fpCopy);
                fwrite(&centDataStorage->Centroids[0], sizeof(sCameraCentroid), readCount, fpCopy);
            }
        }
    }

    // We may need to read in any centroids above AMNM
    // Centroids above AMNM don't get copied to fpCopy
    while (rc == OK && nCentroids > 0)
    {
        if (fread(&tmpCentroid, sizeof(sCameraCentroid), 1, handle) != 1)
        {
            rc = ERRFLAG;
        }
        else
        {
            nCentroids--;
        }
    }

    return rc;
}
