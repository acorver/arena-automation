/*=========================================================
//
// File: ClientTest.cpp
//
// Created by Ned Phipps, Oct-2004
//
=============================================================================*/

#include <windows.h> // for the Sleep function
#include <stdio.h>   // for the printf function
#include <conio.h>

#include "Cortex.h"


int PrintBodyDefs(FILE *handle, sBodyDefs* pBodyDefs)
{
    int iBody;
    int i;

    if (handle == NULL)
    {
        return -1;
    }
    if (pBodyDefs == NULL)
    {
        return -1;
    }

    fprintf(handle, "nBodies=%d\n", pBodyDefs->nBodyDefs);

    for (iBody=0 ; iBody<pBodyDefs->nBodyDefs ; iBody++)
    {
        sBodyDef *pBody = &pBodyDefs->BodyDefs[iBody];
        sHierarchy *pHierarchy = &pBody->Hierarchy;

        fprintf(handle, "Body %d: %s\n", iBody+1, pBody->szName);
        fprintf(handle, "{\n");

     // Markers

        fprintf(handle, "\tnMarkers=%d\n", pBody->nMarkers);
        fprintf(handle, "\t{\n");
        for (i=0 ; i<pBody->nMarkers ; i++)
        {
            fprintf(handle, "\t\t%s\n",
                 pBody->szMarkerNames[i]);
        }
        fprintf(handle, "\t}\n");


     // Segments

        fprintf(handle, "\tnSegments=%d\n", pHierarchy->nSegments);
        fprintf(handle, "\t{\n");
        for (i=0 ; i<pHierarchy->nSegments ; i++)
        {
            int iParent = pHierarchy->iParents[i];

            if (iParent == -1)
            {
                fprintf(handle, "\t\t%s\t%s\n",
                    pHierarchy->szSegmentNames[i],
                    "GLOBAL");
            }
            else
            {
                fprintf(handle, "\t\t%s\t%s\n",
                    pHierarchy->szSegmentNames[i],
                    pHierarchy->szSegmentNames[pHierarchy->iParents[i]]);
            }
        }
        fprintf(handle, "\t}\n");


     // Dofs

        fprintf(handle, "\tnDofs=%d\n", pBody->nDofs);
        fprintf(handle, "\t{\n");
        for (i=0 ; i<pBody->nDofs ; i++)
        {
            fprintf(handle, "\t\t%s\n", pBody->szDofNames[i]);
        }
        fprintf(handle, "\t}\n");

        fprintf(handle, "}\n");
    }


    fprintf(handle, "\n");
	if(pBodyDefs->AnalogBitDepth == 0)
	{
		fprintf(handle, "AnalogBitDepth=<not provided by Cortex>\n");
	}
	else
	{
		fprintf(handle, "AnalogBitDepth=%d\n", pBodyDefs->AnalogBitDepth);
	}
    fprintf(handle, "nAnalogChannels=%d\n", pBodyDefs->nAnalogChannels);
    fprintf(handle, "{\n");
    for (i=0 ; i<pBodyDefs->nAnalogChannels ; i++)
    {
        fprintf(handle, "\t%s\n", pBodyDefs->szAnalogChannelNames[i]);
		if(pBodyDefs->AnalogLoVoltage != NULL && pBodyDefs->AnalogHiVoltage != NULL)
		{
			fprintf(handle, "\tRange=[%f,%f]\n", pBodyDefs->AnalogLoVoltage[i], pBodyDefs->AnalogHiVoltage[i]);
		}
		else
		{
			fprintf(handle, "\tRange=<not provided by Cortex>\n");
		}
    }
    fprintf(handle, "}\n");

    fprintf(handle, "\n");
    fprintf(handle, "nForcePlates=%d\n", pBodyDefs->nForcePlates);


//    fclose(handle);

    return 0;
}


void MyErrorMsgHandler(int iLevel, char *szMsg)
{
    char *szLevel;

    if (iLevel == VL_Debug)
    {
        szLevel = "Debug";
    }
    else
    if (iLevel == VL_Info)
    {
        szLevel = "Info";
    }
    else
    if (iLevel == VL_Warning)
    {
        szLevel = "Warning";
    }
    else
    if (iLevel == VL_Error)
    {
        szLevel = "Error";
    }

    printf("    %s: %s\n", szLevel, szMsg);
}


void PrintAnalogData(FILE *handle, sAnalogData* pAnalogData)
{
    int iSample;
    int nSamples = pAnalogData->nAnalogSamples;
    int iChannel;
    int nChannels = pAnalogData->nAnalogChannels;
    short *pSample = pAnalogData->AnalogSamples;
    int nForcePlates = pAnalogData->nForcePlates;
    int nForceSamples = pAnalogData->nForceSamples;
    tForceData *Force = pAnalogData->Forces;
    int iPlate;
    int nAngleEncoders = pAnalogData->nAngleEncoders;
	int nAngleEncoderSamples = pAnalogData->nAngleEncoderSamples;
    double* AngleEncoderSamples = pAnalogData->AngleEncoderSamples;

    fprintf(handle, "nAnalogChannels=%d\n", nChannels);
    fprintf(handle, "nAnalogSamples=%d\n", nSamples);

    fprintf(handle, "{\n");
    for (iSample=0 ; iSample<nSamples ; iSample++)
    {
        fprintf(handle, "\t%d", iSample+1);
        for (iChannel=0 ; iChannel<nChannels ; iChannel++)
        {
            fprintf(handle, "\t%d", (long)*pSample);
            pSample++;
        }
        fprintf(handle, "\n");
    }
    fprintf(handle, "}\n");


    fprintf(handle, "nForceSamples=%d\n", nForceSamples);
    fprintf(handle, "nForcePlates=%d\n", nForcePlates);

    fprintf(handle, "{\n");
    for (iSample=0 ; iSample<nForceSamples ; iSample++)
    {
        for (iPlate=0 ; iPlate<nForcePlates ; iPlate++)
        {
            fprintf(handle, "\t%d\t%d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\n",
                iSample+1,
                iPlate+1,
                (*Force)[0],
                (*Force)[1],
                (*Force)[2],
                (*Force)[3],
                (*Force)[4],
                (*Force)[5],
                (*Force)[6]);

            Force++;
        }
    }
    fprintf(handle, "}\n");

	fprintf(handle, "nAngleEncoders=%d\n", nAngleEncoders);
	fprintf(handle, "nAngleEncoderSamples=%d\n", nAngleEncoderSamples);

	fprintf(handle, "{\n");
	double *ptr = AngleEncoderSamples;
    for (iSample=0 ; iSample<nAngleEncoderSamples ; iSample++)
	{
        fprintf(handle, "\t%d", iSample+1);

		for (int i=0 ; i<nAngleEncoders ; i++)
	    {
		    fprintf(handle, "\t%lf", *ptr);
			ptr++;
		}

		fprintf(handle, "\n");
	}
	fprintf(handle, "}\n");
}


void PrintFrameOfData(FILE *handle, sFrameOfData *FrameOfData)
{
    int iBody;
    int i;

    fprintf(handle, "Frame=%d\n", FrameOfData->iFrame);
    fprintf(handle, "nBodies=%d\n", FrameOfData->nBodies);

    for (iBody=0 ; iBody<FrameOfData->nBodies ; iBody++)
    {
        sBodyData *Body = &FrameOfData->BodyData[iBody];

        fprintf(handle, "Body %d: %s\n", iBody+1, Body->szName);
        fprintf(handle, "{\n");


     // Markers

        fprintf(handle, "\tnMarkers=%d\n", Body->nMarkers);
        fprintf(handle, "\t{\n");
        for (i=0 ; i<Body->nMarkers ; i++)
        {
            fprintf(handle, "\t\t%d\t%f\t%f\t%f\n",
                i+1,
                Body->Markers[i][0],
                Body->Markers[i][1],
                Body->Markers[i][2]);
        }
        fprintf(handle, "\t}\n");


     // Segments

        fprintf(handle, "\tnSegments=%d\n", Body->nSegments);
        fprintf(handle, "\t{\n");
        for (i=0 ; i<Body->nSegments ; i++)
        {
            fprintf(handle, "\t\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf\n",
                i+1,
                Body->Segments[i][0],
                Body->Segments[i][1],
                Body->Segments[i][2],
                Body->Segments[i][3],
                Body->Segments[i][4],
                Body->Segments[i][5],
                Body->Segments[i][6]);
        }
        fprintf(handle, "\t}\n");


     // Dofs

        fprintf(handle, "\tnDofs=%d\n", Body->nDofs);
        fprintf(handle, "\t{\n");
        for (i=0 ; i<Body->nDofs ; i++)
        {
            fprintf(handle, "\t\t%d\t%lf\n", i+1, Body->Dofs[i]);
        }
        fprintf(handle, "\t}\n");

		fprintf(handle, "\tEncoder Zoom=%d\n", Body->ZoomEncoderValue);
		fprintf(handle, "\tEncoder Focus=%d\n", Body->FocusEncoderValue);

	 // Events
		fprintf(handle, "\tnEvents=%d\n", Body->nEvents);
        fprintf(handle, "\t{\n");
        for (i=0 ; i<Body->nEvents; i++)
        {
            fprintf(handle, "\t\t%s\n", Body->Events[i]);
        }
        fprintf(handle, "\t}\n");

     // End of one BodyData
        fprintf(handle, "}\n");
    }


 // Unidentified Markers

    fprintf(handle, "nUnidentifiedMarkers=%d\n", FrameOfData->nUnidentifiedMarkers);
    fprintf(handle, "{\n");
    for (i=0 ; i<FrameOfData->nUnidentifiedMarkers ; i++)
    {
        fprintf(handle, "\t%d\t%f\t%f\t%f\n",
            i+1,
            FrameOfData->UnidentifiedMarkers[i][0],
            FrameOfData->UnidentifiedMarkers[i][1],
            FrameOfData->UnidentifiedMarkers[i][2]);
    }
    fprintf(handle, "}\n");

 // Raw Analog Data

    PrintAnalogData(handle, &FrameOfData->AnalogData);

 // TimeCode Data

    sTimeCode* TC = &FrameOfData->TimeCode;
    if (TC->iStandard != 0)
    {
        fprintf(handle, "TimeCode=");
        fprintf(handle, "%02d:%02d:%02d:%02d",
            TC->iHours,
            TC->iMinutes,
            TC->iSeconds,
            TC->iFrames);

        switch (TC->iStandard)
        {
            case 1: fprintf(handle, "(SMPTE)\n"); break;
            case 2: fprintf(handle, "(FILM)\n"); break;
            case 3: fprintf(handle, "(EBU)\n"); break;
            case 4: fprintf(handle, "(SYSTEMCLOCK)\n"); break;
        }
    }

 // Recording Status
	sRecordingStatus *RC = &FrameOfData->RecordingStatus;
	fprintf(handle, "Recording=%d\n", RC->bRecording);
	fprintf(handle, "Record First Frame=%d\n", RC->iFirstFrame);
	fprintf(handle, "Record Last Frame=%d\n", RC->iLastFrame);
	fprintf(handle, "Record File Name=%s\n", RC->szFilename);
}


void MyDataHandler(sFrameOfData* FrameOfData)
{
    static FILE *handle=NULL;
    static int Count=0;

    if (Count >= 10)
    {
        if (handle != NULL)
        {
            fclose(handle);
            handle = NULL;
        }
        return;
    }

    if (handle == NULL)
    {
        handle = fopen("TestDataHandling.txt", "w+");
    }
    if (handle == NULL)
    {
        return;
    }

    PrintFrameOfData(handle, FrameOfData);
    fflush(handle);

    Count++;
}


void Request()
{
    int retval;
    void *pResponse;
    int nBytes;
    char key;

    while (1)
    {
        printf("\n");
        printf("Options(<esc>=Back):\n");
        printf("----------------\n");
        printf("1) LiveMode\n");
        printf("2) Pause\n");
        printf("3) SetOutputName=<Filename>\n");
        printf("4) StartRecording\n");
        printf("5) StopRecording\n");
        printf("6) ResetIDs\n");
        printf("7) GetContextFrameRate\n");
        printf("8) GetUpAxis\n");
        printf("9) GetConversionToMillimeters\n");
        printf("----------------\n");
        printf("--> ");

        key = getch();

        printf("%c\n", key);
        printf("\n");

        if (key == 27)
        {
            return;
        }


        if (key == '1')
        {
            retval = Cortex_Request("LiveMode", &pResponse, &nBytes);
            if (retval != RC_Okay) goto NOT_OKAY;
        }
        else
        if (key == '2')
        {
            retval = Cortex_Request("Pause", &pResponse, &nBytes);
            if (retval != RC_Okay) goto NOT_OKAY;
        }
        else
        if (key == '3')
        {
            char szRequest[256];
            char szFilename[256];

            printf("Filename --> ");
            scanf("%s", szFilename);
            sprintf(szRequest, "SetOutputName=%s", szFilename);
            retval = Cortex_Request(szRequest, &pResponse, &nBytes);
            if (retval != RC_Okay) goto NOT_OKAY;
        }
        else
        if (key == '4')
        {
            retval = Cortex_Request("StartRecording", &pResponse, &nBytes);
            if (retval != RC_Okay) goto NOT_OKAY;
        }
        else
        if (key == '5')
        {
            retval = Cortex_Request("StopRecording", &pResponse, &nBytes);
            if (retval != RC_Okay) goto NOT_OKAY;
        }
        else
        if (key == '6')
        {
            retval = Cortex_Request("ResetIDs", &pResponse, &nBytes);
            if (retval != RC_Okay) goto NOT_OKAY;
        }
        else
        if (key == '7')
        {
            retval = Cortex_Request("GetContextFrameRate", &pResponse, &nBytes);
            if (retval != RC_Okay) goto NOT_OKAY;

            printf("ContextFrameRate = %f\n", *(float*)pResponse);
        }
        else
        if (key == '8')
        {
            retval = Cortex_Request("GetUpAxis", &pResponse, &nBytes);
            if (retval != RC_Okay) goto NOT_OKAY;

            printf("UpAxis = %d\n", *(int*)pResponse);
        }
        else
        if (key == '9')
        {
            retval = Cortex_Request("GetConversionToMillimeters", &pResponse, &nBytes);
            if (retval != RC_Okay) goto NOT_OKAY;

            printf("ConversionToMillimeters = %f\n", *(float*)pResponse);
        }




NOT_OKAY:
        if (retval == RC_TimeOut)
        {
            printf("TimeOut\n");
        }
        else
        if (retval == RC_Unrecognized)
        {
            printf("Unrecognized\n");
        }
    }

 // Unreachable
    return;
}


int main(int argc, char* argv[])
{
    sHostInfo Cortex_HostInfo;
    int retval;
    unsigned char SDK_Version[4];
    char key;
    int i;
    sBodyDefs*    pBodyDefs=NULL;
    sFrameOfData* pFrameOfData=NULL;
    sFrameOfData  MyCopyOfFrame;

    memset(&MyCopyOfFrame, 0, sizeof(sFrameOfData));

    printf("Usage: ClientTest <Me> <Cortex>\n");
    printf("       Me = My machine name or its IP address\n");
    printf("       Cortex = Cortex's machine name or its IP Address\n");

    for (i=0 ; i<argc ; i++)
        printf(" %s", argv[i]);
    printf("\n");

    printf("----------\n");

    Cortex_SetVerbosityLevel(VL_Info); //(VL_Debug);

    Cortex_GetSdkVersion(SDK_Version);
    printf("Using SDK Version %d.%d.%d\n",
        SDK_Version[1],
        SDK_Version[2],
        SDK_Version[3]);


    Cortex_SetErrorMsgHandlerFunc(MyErrorMsgHandler);
    Cortex_SetDataHandlerFunc(MyDataHandler);

    if (argc == 1)
    {
        retval = Cortex_Initialize("", NULL);
    }
    else
    if (argc == 2)
    {
        retval = Cortex_Initialize(argv[1], NULL);
    }
    else
    if (argc == 3)
    {
        retval = Cortex_Initialize(argv[1], argv[2]);
    }

    if (retval != RC_Okay)
    {
        printf("Error: Unable to initialize ethernet communication\n");
        goto DONE;
    }

    retval = Cortex_GetHostInfo(&Cortex_HostInfo);

	if (retval != RC_Okay
     || !Cortex_HostInfo.bFoundHost)
    {
        printf("\n");
        printf("Cortex not found.\n");
    }
    else
    {
        printf("\n");
        printf("Found %s Version %d.%d.%d at %d.%d.%d.%d (%s)\n",
            Cortex_HostInfo.szHostProgramName,
            Cortex_HostInfo.HostProgramVersion[1],
            Cortex_HostInfo.HostProgramVersion[2],
            Cortex_HostInfo.HostProgramVersion[3],

            Cortex_HostInfo.HostMachineAddress[0],
            Cortex_HostInfo.HostMachineAddress[1],
            Cortex_HostInfo.HostMachineAddress[2],
            Cortex_HostInfo.HostMachineAddress[3],
            Cortex_HostInfo.szHostMachineName);
    }

    while (1)
    {
        printf("\n");
        printf("Options(Q=Exit):\n");
        printf("----------------\n");
        printf("1) GetBodyDefs\n");
        printf("2) GetFrameOfData\n");
        printf("3) Request Function\n");
        printf("----------------\n");
        printf("--> ");

        key = getch();

        printf("%c\n", key);
        printf("\n");

        if (key == '1')
        {
            pBodyDefs = Cortex_GetBodyDefs();

            if (pBodyDefs == NULL)
            {
                printf("????\n");
            }
            else
            {
                FILE *handle = fopen("BodyDefs.txt", "w+");
                if (handle == NULL)
                {
                    printf("Unable to open file: BodyDefs.txt\n");
                }
                else
                {
                    PrintBodyDefs(handle, pBodyDefs);
                    fclose(handle);
                    handle = NULL;

                    printf("File \"BodyDefs.txt\" was written defining %d body(s).\n", pBodyDefs->nBodyDefs);

                    Cortex_FreeBodyDefs(pBodyDefs);
                    pBodyDefs = NULL;
                }
            }
        }
        else
        if (key == '2')
        {
            pFrameOfData = Cortex_GetCurrentFrame();
            if (pFrameOfData == NULL)
            {
                printf("????\n");
            }
            else
            {
                FILE *handle = fopen("FrameOfData.txt", "w+");
                if (handle == NULL)
                {
                    printf("Unable to open file: FrameOfData.txt\n");
                }
                else
                {
                    retval = Cortex_CopyFrame(pFrameOfData, &MyCopyOfFrame);
                    if (retval != RC_Okay)
                    {
                        continue;
                    }

                    PrintFrameOfData(handle, pFrameOfData);
                    //PrintFrameOfData(handle, &MyCopyOfFrame);
                    Cortex_FreeFrame(&MyCopyOfFrame);

                    fclose(handle);
                    handle = NULL;

                    printf("File \"FrameOfData.txt\" was written for %d body(s).\n", pFrameOfData->nBodies);

                    //Cortex_FreeBodyDefs(pBodyDefs);
                    //pBodyDefs = NULL;
                }
            }
        }
        else
        if (key == '3')
        {
            Request();
        }
        else
        if (key == 'Q'
         || key == 'q')
        {
            break;
        }
    }

DONE:

    retval = Cortex_Exit();
    printf("Press any key to continue...");
    key = getch();

    return 0;
}
