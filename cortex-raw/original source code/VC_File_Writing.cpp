
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>

#include <stdio.h>

#include "standard.h"

#include "Data.h"
#include "Messenger.h"
#include "output.h"
#include "project.h"
#include "orion.h"
#include "socket.h"
#include "MidasControl.h"
#include "Eagle.h"

extern int UMAX2D,VMAX2D;

int EXPORTED Eagle_SendSimpleCommand(int iCommand);

extern int DataCollection_Count;

LOCAL FILE* vcx_handles[MAX_VBOARDS]={NULL};
LOCAL int FrameCount_FilePos=-1;
LOCAL int SensorResolution_iWord=-1;
LOCAL unsigned short Header_wBuffer[0x10000];
LOCAL int Header_nWords=0;

//GLOBAL int bRecordingVideo = FALSE;
GLOBAL eRecordingStatus VCX_RecordingStatus = NOT_RECORDING;
GLOBAL eRecordingStatus AVI_RecordingStatus = NOT_RECORDING;

LOCAL int nFramesWritten = 0;

GLOBAL int nFramesWritten2[MAX_VBOARDS] = {0};
LOCAL cCamBitArray OpenFilesBitmap;

int VCX_Write1Frame(int iCamera, int iFrame);
int VCX_Write1Frame(int iFrame);


// TEST

LOCAL int bAlwaysFlushRawData = TRUE;

#define MAX_EXTRAGROUPS 10
LOCAL int nExtraRawFileGroups=0;
LOCAL FILE* ExtraFileHandles[MAX_VBOARDS][MAX_EXTRAGROUPS]={NULL};

LOCAL int WriteExtraRawFiles(int iCamera, unsigned short* Words, int nWords);
LOCAL int OpenExtraRawFileGroups(char *szOriginalFilename, int iCamera, unsigned short* Header, int nWords);
LOCAL int PokeFrameCountExtraFileFiles(int iCamera, int nFrames);

// END TEST


/*{====================================================================
//
//  Note: We are using this function to write EagleCamera files.
//
//------------------------------------------------------------------}*/
int VCX_WriteFrame(int iCamera, int iFrame, unsigned short *Words, int nWords)
{
    char str[256];
    unsigned short word;
    int iFixupFrame;

    if (VCX_RecordingStatus == NOT_RECORDING)
    {
        return 0;
    }

    if (vcx_handles[iCamera] == NULL)
    {
        return 0;
    }

    if (iFrame < Recording_iFirstFrame)
    {
        return 0;
    }

    if (nFramesWritten2[iCamera] == 0
     && iFrame > Recording_iFirstFrame)
    {
        if (DebugBits & DEBUG_RAWDATA)
        {
            sprintf(str, "Camera %d: Digging into FIFO for the first %d frames",
                iCamera+1,
                iFrame - Recording_iFirstFrame);
            SendComment(str);
        }

        for (iFixupFrame=Recording_iFirstFrame ; iFixupFrame<iFrame ; iFixupFrame++)
        {
            VCX_Write1Frame(iCamera, iFixupFrame);
        }
//        sprintf(str, "Starting Frame(s) problem for camera %d handled", iCamera+1);

//        SendThisMessage(MSG_STRING, 0, (long)str);
//        PopupNotice(str);
    }

 // One more check because the VCX_WriteFrame(iCamera, iFrame) may have closed the file
    if (vcx_handles[iCamera] == NULL)
    {
        return 0;
    }

    Words[3] -= Recording_iFirstFrame;

    fwrite(Words, nWords, 2, vcx_handles[iCamera]);

// FINDING OUT IF THIS FLUSH HELPS OR HURTS.
// THIS FLUSH HAS BEEN HERE TO MAKE THE CAPTURE CONSISTENT!
    if (bAlwaysFlushRawData)
        fflush(vcx_handles[iCamera]);

//TEST
    if (nExtraRawFileGroups > 0)
        WriteExtraRawFiles(iCamera, Words, nWords);
//END TEST

    nFramesWritten2[iCamera]++;


 // That could be finishing the recording for this camera and also for all cameras

    if (iFrame >= Recording_iLastFrame)
    {
        int nFrames = Recording_iLastFrame - Recording_iFirstFrame + 1;

        fseek(vcx_handles[iCamera], FrameCount_FilePos, SEEK_SET);

        word = nFrames >> 16;
        fwrite(&word, 2, 1, vcx_handles[iCamera]);

        word = nFrames & 0xFFFF;
        fwrite(&word, 2, 1, vcx_handles[iCamera]);

        fclose(vcx_handles[iCamera]);
        vcx_handles[iCamera] = NULL;

        PokeFrameCountExtraFileFiles(iCamera, nFrames);


        OpenFilesBitmap.UnsetCamBit(iCamera);

        if (OpenFilesBitmap.IsEmpty())
        {
         // If the status is FLUSHING, then the AVI finish is done at the time
         // the FLUSHING status was set.
            if (AVI_RecordingStatus == RECORDING)
            {
                AVI_WriteData_Finish2();
            }

            VCX_RecordingStatus = NOT_RECORDING;

            if (DebugBits & DEBUG_RAWDATA)
            {
                SendComment("Finished writing .vc* files.");
            }

            Recording_iBitMask &= ~REC_VCX;
            if (Recording_iBitMask == 0)
            {
                SendThisMessage(MSG_RECORDING_COMPLETE);
            }
        }
    }

    return 0;
}


/*================================================================
// for eagle data
// Write to a file from data in the FIFO
//--------------------------------------------------------------*/
int VCX_Write1Frame(int iCamera, int iFrame)
{
    int iStart1;
    int iStart2;
    int nWords1;
    int nWords2;
    int iWord_FrameNumber;
    unsigned short word;

    if (vcx_handles[iCamera] == NULL)
    {
        return 0;
    }

    if (iFrame < Recording_iFirstFrame)
     //|| iFrame > Recording_iLastFrame)
    {
        return 0;
    }

    iStart1 = iFrameStarts[iFrame&0xFF][iCamera] % RawData_FIFOSIZE;
    iStart2 = iFrameStarts[(iFrame+1)&0xFF][iCamera] % RawData_FIFOSIZE;

    if (iStart2 > iStart1)
    {
        nWords1 = iStart2 - iStart1;
        nWords2 = 0;
    }
    else
    {
        nWords2 = iStart2;
        iStart2 = 0;
        nWords1 = RawData_FIFOSIZE - iStart1;
    }

#if 1
    if (nWords1 < 4)
        iWord_FrameNumber = 3 - nWords1;
    else
        iWord_FrameNumber = iStart1 + 3;

//    wRawData[iCamera][iWord_FrameNumber] = (nFramesWritten + 1) & 0xFFFF;
    wRawData[iCamera][iWord_FrameNumber] -= Recording_iFirstFrame;
#endif

    if (nWords1 > 0x10000
     || nWords2 > 0x10000)
    {
        char str[256];
        sprintf(str, "nWords1 = %d, nWords2 = %d", nWords1, nWords2);
        SendComment(str);
    }

    fwrite(
        &wRawData[iCamera][iStart1],
        2,
        nWords1,
        vcx_handles[iCamera]);
        //vcx_handles[0]);

    if (nWords2 > 0)
    {
        fwrite(
            &wRawData[iCamera][iStart2],
            2,
            nWords2,
            vcx_handles[iCamera]);
            //vcx_handles[0]);
    }

    fflush(vcx_handles[iCamera]);
    //fflush(vcx_handles[0]);

//TEST
    WriteExtraRawFiles(iCamera, &wRawData[iCamera][iStart1], nWords1);
    if (nWords2 > 0)
    {
        WriteExtraRawFiles(iCamera, &wRawData[iCamera][iStart2], nWords2);
    }
//END TEST

    nFramesWritten2[iCamera]++;


 // That might have completed the entire .vc* capture

//if (iCamera != 0) return 0;

    if (iFrame >= Recording_iLastFrame)
    {
        int nFrames = Recording_iLastFrame - Recording_iFirstFrame + 1;

        fseek(vcx_handles[iCamera], FrameCount_FilePos, SEEK_SET);

        word = nFrames >> 16;
        fwrite(&word, 2, 1, vcx_handles[iCamera]);

        word = nFrames & 0xFFFF;
        fwrite(&word, 2, 1, vcx_handles[iCamera]);

        fclose(vcx_handles[iCamera]);
        vcx_handles[iCamera] = NULL;

        OpenFilesBitmap.UnsetCamBit(iCamera);

        if (OpenFilesBitmap.IsEmpty())
        {
            VCX_RecordingStatus = NOT_RECORDING;

            Recording_iBitMask &= ~REC_VCX;
            if (Recording_iBitMask == 0)
            {
                SendThisMessage(MSG_RECORDING_COMPLETE);
            }

            SendThisMessage(MSG_STRING, 0, (LPARAM)"Finished writing .vc* files.");
        }
    }

    return 0;
}


//=============================================================
//
// Note: This function actually will sometimes write
//       more than one frame.  This is expected at the
//       start of recording when the start was by trigger
//       and the first frame number is explicit.  This function
//       will "catch up" to the current frame
//
//-------------------------------------------------------------
int VCX_WriteFrame(int iCurrentFrame)
{
    int iFrame = Recording_iFirstFrame + nFramesWritten;

 // Possibly nothing to do
//    if (!bRecordingVideo)
    if (VCX_RecordingStatus == NOT_RECORDING)
    {
        return 0;
    }

    if (iCurrentFrame < Recording_iFirstFrame)
    {
        return 0;
    }
    
    while (iFrame <= iCurrentFrame)
    {
        VCX_Write1Frame(iFrame);
        iFrame++;
    }

    return 0;
}


int VCX_Write1Frame(int iFrame)
{
    int iStart1;
    int iStart2;
    int nWords1;
    int nWords2;
    int iBoard;
    int iWord_FrameNumber;

//    if (!bRecordingVideo)
    if (VCX_RecordingStatus == NOT_RECORDING)
    {
        return 0;
    }

    if (iFrame < Recording_iFirstFrame)
    {
        return 0;
    }

    if (iFrame > Recording_iLastFrame)
    {
        VCX_WriteData_Finish();
        return 0;
    }

    //iFrame &= 0xFF;

    for (iBoard=0 ; iBoard<g_nMaxCameras ; iBoard++)
    {
        if (vcx_handles[iBoard] == NULL)
        {
            continue;
        }
        if (iBoardStatus[iBoard] != OK)
        {
            continue;
        }

        iStart1 = iFrameStarts[iFrame&0xFF][iBoard] % RawData_FIFOSIZE;
        iStart2 = iFrameStarts[(iFrame+1)&0xFF][iBoard] % RawData_FIFOSIZE;

        if (iStart2 > iStart1)
        {
             nWords1 = iStart2 - iStart1;
             nWords2 = 0;
        }
        else
        {
            nWords2 = iStart2;
            iStart2 = 0;
            nWords1 = RawData_FIFOSIZE - iStart1;
        }

        if (nWords1 < 4)
            iWord_FrameNumber = 3 - nWords1;
        else
            iWord_FrameNumber = iStart1 + 3;

//        wRawData[iBoard][iWord_FrameNumber] = (nFramesWritten + 1) & 0xFFFF;
        wRawData[iBoard][iWord_FrameNumber] -= Recording_iFirstFrame;

        fwrite(
            &wRawData[iBoard][iStart1],
            2,
            nWords1,
            vcx_handles[iBoard]);

        if (nWords2 > 0)
        {
            fwrite(
                &wRawData[iBoard][iStart2],
                2,
                nWords2,
                vcx_handles[iBoard]);
        }

        fflush(vcx_handles[iBoard]);
    }

    nFramesWritten++;

    if (iFrame >= Recording_iLastFrame)
    {
        VCX_WriteData_Finish();
        return 0;
    }

    return 0;
}

#if 0
int VCX_ReadAndStoreFileHeader(int nWords)
{
    //sVideoChannel *channels = ProjectParameters.VideoSystem.Channels;
    int iCamera;
    int iWord=6;
    int nBytes;
    int nPixelsWidth;
    int nPixelsHeight;

    Header_nWords = nWords;

    nBytes = Socket_ReadExactly(
        MidasConnection,
        (char *)Header_wBuffer,
        2*nWords);

 // Scan the buffer for the location of the frame count.
 // That value will get filled in at the finish

    FrameCount_FilePos = -1;

    iWord = 6;
    while (iWord < nWords)
    {
        if (Header_wBuffer[iWord] == 0x0115)
        {
            FrameCount_FilePos = 2*(iWord+2);
        }

        if (Header_wBuffer[iWord] == 0x105) //MAXXYK
        {
            SensorResolution_iWord = iWord+2;

            nPixelsWidth = Header_wBuffer[iWord+2];
            nPixelsHeight = Header_wBuffer[iWord+3];
        }

        iWord += 2 + 2*Header_wBuffer[iWord+1];
    }


 // Midas cameras must all have the same resolution.

    for (iCamera=0 ; iCamera<g_nMaxCameras ; iCamera++)
    {
        CameraSites[iCamera].nPixelsWidth = nPixelsWidth;
        CameraSites[iCamera].nPixelsHeight = nPixelsHeight;
    }

    return 0;
}
#endif


int VCX_WriteHeader()
{
    int iBoard;

    for (iBoard=0 ; iBoard<g_nMaxCameras ; iBoard++)
    {
        if (vcx_handles[iBoard] != NULL)
        {
            Header_wBuffer[SensorResolution_iWord] = CameraSites[iBoard].nPixelsWidth;
            Header_wBuffer[SensorResolution_iWord+1] = CameraSites[iBoard].nPixelsHeight;

            rewind(vcx_handles[iBoard]);
            fwrite(Header_wBuffer, 2, Header_nWords, vcx_handles[iBoard]);
            fflush(vcx_handles[iBoard]);
        }
    }
    return 0;
}



int EXPORTED Test_AlwaysFlushRawData(int bValue)
{
    bAlwaysFlushRawData = bValue;

    return OK;
}


int EXPORTED Test_ExtraRawFileGroups(int nGroups)
{
    if (nGroups > MAX_EXTRAGROUPS)
    {
        nGroups = MAX_EXTRAGROUPS;
    }

    nExtraRawFileGroups = nGroups;

    return OK;
}


LOCAL int OpenExtraRawFileGroups(char *szOriginalFilename, int iCamera, unsigned short* Header, int nWords)
{
    char szFilename[256];
    char szPrevFilename[256];
    int iGroup;
    FILE* Handle;

    strcpy(szPrevFilename, szOriginalFilename);

    for (iGroup=0 ; iGroup<nExtraRawFileGroups ; iGroup++)
    {
        sprintf(szFilename, "_%s", szPrevFilename);
        Handle = fopen(szFilename, "w+b");

        fwrite(Header, 2, nWords, Handle);
        fflush(Handle);

        ExtraFileHandles[iCamera][iGroup] = Handle;

        strcpy(szPrevFilename, szFilename);
    }

    return OK;
}


LOCAL int WriteExtraRawFiles(int iCamera, unsigned short* Words, int nWords)
{
    int iGroup;

    for (iGroup=0 ; iGroup<nExtraRawFileGroups ; iGroup++)
    {
        if (ExtraFileHandles[iCamera][iGroup] != NULL)
        {
            fwrite(Words, 2,nWords, ExtraFileHandles[iCamera][iGroup]);
            if (bAlwaysFlushRawData)
            {
               fflush(ExtraFileHandles[iCamera][iGroup]);
            }
        }
    }
    return OK;
}


LOCAL int PokeFrameCountExtraFileFiles(int iCamera, int nFrames)
{
    int iGroup;
    unsigned short word;

    for (iGroup=0 ; iGroup<nExtraRawFileGroups ; iGroup++)
    {
        FILE* Handle = ExtraFileHandles[iCamera][iGroup];
        if (Handle != NULL)
        {
            fseek(Handle, FrameCount_FilePos, SEEK_SET);

            word = nFrames >> 16;
            fwrite(&word, 2, 1, Handle);

            word = nFrames & 0xFFFF;
            fwrite(&word, 2, 1, Handle);

            fclose(Handle);
            ExtraFileHandles[iCamera][iGroup] = NULL;
        }
    }

    return OK;
}


LOCAL int CloseExtraRawFileGroups()
{
    int iCamera;
    int iGroup;

    for (iCamera=0 ; iCamera<g_nMaxCameras ;iCamera++)
    {
        for (iGroup=0 ; iGroup<MAX_EXTRAGROUPS ; iGroup++)
        {
            if (ExtraFileHandles[iCamera][iGroup] != NULL)
            {
                fclose(ExtraFileHandles[iCamera][iGroup]);
                ExtraFileHandles[iCamera][iGroup] = NULL;
            }
        }
    }
    return OK;
}


int VCX_WriteData_Init(char *name)
{
    sVideoChannel *channels = ProjectParameters.VideoSystem.Channels;
    char filename[256];
    int iCamera;

    OpenFilesBitmap.Clear();

    for (iCamera=0 ; iCamera<g_nMaxCameras ; iCamera++)
    {
        if (vcx_handles[iCamera] != NULL)
        {
            fclose(vcx_handles[iCamera]);
            vcx_handles[iCamera] = NULL;
        }

        sprintf(filename, "%s.vc%d", name, iCamera+1);

        if (channels[iCamera].bEnabled)
        {
            vcx_handles[iCamera] = fopen(filename, "w+b");

            Header_wBuffer[SensorResolution_iWord] = CameraSites[iCamera].nPixelsWidth;
            Header_wBuffer[SensorResolution_iWord+1] = CameraSites[iCamera].nPixelsHeight;

            fwrite(Header_wBuffer, 2, Header_nWords, vcx_handles[iCamera]);
            fflush(vcx_handles[iCamera]);
            OpenFilesBitmap.SetCamBit(iCamera);

            OpenExtraRawFileGroups(filename, iCamera, Header_wBuffer, Header_nWords);
        }
        else
        {
            remove(filename);
        }

        nFramesWritten2[iCamera] = 0;
    }

    nFramesWritten = 0;
//    bRecordingVideo = TRUE;
    VCX_RecordingStatus = RECORDING;

    return 0;
}


#if 0
int VCX_WriteData(int iBoard, unsigned short *wBuffer, int nWords)
{
    if (vcx_handles[iBoard] != NULL)
    {
        fwrite(wBuffer, 2, nWords, vcx_handles[iBoard]);
    }
    return 0;
}
#endif


int VCX_WriteData_Finish()
{
    int iBoard;
    unsigned short word;
    int nFrames;

    if (VCX_RecordingStatus == NOT_RECORDING)
    {
        return 0;
    }

 // Eagle camera completion is handled elsewhere.
    if (iCollectionSource == SOURCE_E1)
    {
//        Eagle_SendSimpleCommand(PKT_FLUSHDATA);
        return 0;
    }


    AVI_WriteData_Finish2();


    for (iBoard=0 ; iBoard<g_nMaxCameras ; iBoard++)
    {
        if (vcx_handles[iBoard] == NULL)
        {
            continue;
        }

     // Poke the number of frames into the file header

        if (FrameCount_FilePos != -1)
        {
            fseek(vcx_handles[iBoard], FrameCount_FilePos, SEEK_SET);

            word = nFramesWritten >> 16;
            fwrite(&word, 2, 1, vcx_handles[iBoard]);

            word = nFramesWritten & 0xFFFF;
            fwrite(&word, 2, 1, vcx_handles[iBoard]);
        }

        fclose(vcx_handles[iBoard]);
        vcx_handles[iBoard] = NULL;
    }

    nFrames = Recording_iLastFrame - Recording_iFirstFrame + 1;
    if (nFrames != nFramesWritten)
    {
        char str[256];
        sprintf(str, "nFrames=%d, nFramesWritten=%d", nFrames, nFramesWritten);
        PopupNotice(str);
    }

    VCX_RecordingStatus = NOT_RECORDING;

    Recording_iBitMask &= ~REC_VCX;
    if (Recording_iBitMask == 0)
    {
        SendThisMessage(MSG_RECORDING_COMPLETE);
    }

    return 0;
}


int VCX_WriteData_Finish_Cal()
{
    int iBoard;
    unsigned short word;

    if (VCX_RecordingStatus == NOT_RECORDING)
    {
        return 0;
    }

    for (iBoard=0 ; iBoard<g_nMaxCameras ; iBoard++)
    {
        if (vcx_handles[iBoard] == NULL)
        {
            continue;
        }

     // Poke the number of frames into the file header

        if (FrameCount_FilePos != -1)
        {
            fseek(vcx_handles[iBoard], FrameCount_FilePos, SEEK_SET);

            word = nFramesWritten >> 16;
            fwrite(&word, 2, 1, vcx_handles[iBoard]);

            word = nFramesWritten & 0xFFFF;
            fwrite(&word, 2, 1, vcx_handles[iBoard]);
        }

        fclose(vcx_handles[iBoard]);
        vcx_handles[iBoard] = NULL;
    }

	return 0;
}



/*/////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------*/

int BuildEagleRawVideoFileHeader(unsigned short wBuffer[])
{
    float fdata;
    int nWords=0;

#if 0
    int index;
    unsigned int nframeratio;
 /* Information needed for storing the SAMPLERATE_KYWD keyword information.*/
     /* for example, for a Falcon
	    camera, index will have a value of 1 when the sync rate is
	    set to 60 and a value of 4 when the sync rate is set to 240 */
 /* for example, for a Falcon camera:
    sync rate   video rate  nframeratio
        60           60          1
        60           30          2
       240          240          1
       240           60          4
 */
    index = Video.icurrentsyncrateindex;
    nframeratio = (unsigned int) (Videorate[index].
        wskipframes[Videorate[index].icurrentsamplerateindex]) + 1;
#endif


 /* Write the standard block header which begins the Video File Environment
    Block.  Check the bMultiple input parameter to determine whether we
    need to write to a single file or to multiple files. */

    wBuffer[nWords++] = 0; // FieldDelimiter
    wBuffer[nWords++] = 0; // FieldDelimiter

    wBuffer[nWords++] = 0xFFFF; // Event = FF, DataType = FF
    wBuffer[nWords++] = 0xFFFF; // Field Counter

    wBuffer[nWords++] = 0; // BlockLength HighWord
    wBuffer[nWords++] = 0; // BlockLength LowWord (Gets filled in at end of function)

    /* Following the Video Environment Block Header is the Video
       Environment Block Data. */

    wBuffer[nWords++] = 0x0101; // CPUFMT_KYWRD
    wBuffer[nWords++] = 1;      // count
    wBuffer[nWords++] = 0x0101; // Little Endian
    wBuffer[nWords++] = 0x0101; // Little Endian


    /* Now specify a data type of 08, i.e. edge data without the block
       length in the block header.
       IMPORTANT NOTE: the data coming out of the FIFO will have a
       data type of 01 encoded in it. Since the rtmidas software is
       just transferring the data without modifying it, the data type
       will remain encoded as 01 even though it it really 08 (because
       it will not contian the block length). Therefore it is incumbent
       upon any software reading in the edge (.vc) file to read the
       video file environment block and recognize that the data type is
       08 and ignore the subsequent 01 data type contained in the
       edge block header for each frame of data.  For the Update Screen
       data collection, we can supply the block length information so
       the file created for Update Screen will be a type 01. */

    wBuffer[nWords++] = 0x0102; // Data types keyword
    wBuffer[nWords++] = 1;      // count
    wBuffer[nWords++] = 0x0800; // Video datatype
    wBuffer[nWords++] = 0;      // filler

    wBuffer[nWords++] = 0x0103; // Video rate keyword
    wBuffer[nWords++] = 1;      // count
    fdata = ProjectParameters.VideoSystem.fCameraCaptureRate;
    memcpy(&wBuffer[nWords], &fdata, 4);
    nWords += 2;

    wBuffer[nWords++] = 0x0104; // Subsample rate keyword
    wBuffer[nWords++] = 1;      // count
    //fdata = (float)Midasboard[0].dtruefieldrate/nframeratio;
    fdata = ProjectParameters.VideoSystem.fCameraCaptureRate; // Same as for 0x0103
    memcpy(&wBuffer[nWords], &fdata, 4);
    nWords += 2;

    wBuffer[nWords++] = 0x0105; // MAXXY keyword
    wBuffer[nWords++] = 1;      // count
    wBuffer[nWords++] = 1280;
    wBuffer[nWords++] = 1024;

	/* Write camera type (8 bits), camera sync (8 bits) and
       VPAT revision level (16 bits) information.
       We define the 16 bits of ndata for camera type and syncrate as consisting
       of bits 15 - 8 (= camera type) and bits 7 - 0 (= syncrate)
       So for example camera type 45 and syncrate 240 would be
       stored as 2DF0h (=11,760 decimal) */

int nCameraType = 0;
int nSyncRate = int (0.5f + ProjectParameters.VideoSystem.fCameraCaptureRate);
int wEagleVersionNumber = 1;

    wBuffer[nWords++] = 0x0114; // CameraType and SyncRate
    wBuffer[nWords++] = 1;      // count
    wBuffer[nWords++] = (unsigned short)((nCameraType << 8) + nSyncRate);
    wBuffer[nWords++] = wEagleVersionNumber;

    wBuffer[nWords++] = 0x0115; // Number of frames keyword
    wBuffer[nWords++] = 1;      // count
    wBuffer[nWords++] = 0;      // HighWord of frame count
    wBuffer[nWords++] = 0;      // LowWord of frame count

 // Fill in the header size

    wBuffer[5] = nWords/2;

 // The number of frames gets filled in when collection is completed

    return nWords;
}


int VCX_SetFileHeader(unsigned short *Header, int nWords)
{
    int iWord;

    memcpy(Header_wBuffer, Header, 2*nWords);
    Header_nWords = nWords;

    FrameCount_FilePos = -1;

    iWord = 6;
    while (iWord < nWords)
    {
        if (Header_wBuffer[iWord] == 0x0115)
        {
            FrameCount_FilePos = 2*(iWord+2);
            //break;
        }            

        if (Header_wBuffer[iWord] == 0x105) //MAXXYK
        {
            extern int UMAX2D,VMAX2D;
            SensorResolution_iWord = iWord+2;

            UMAX2D = Header_wBuffer[iWord+2];
            VMAX2D = Header_wBuffer[iWord+3];
        }

        iWord += 2 + 2*Header_wBuffer[iWord+1];
    }
    return 0;
}
