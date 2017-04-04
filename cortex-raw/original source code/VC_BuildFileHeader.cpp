int cRawData::BuildFileHeader()
{
    float fdata;
    int nWords=0;

 /* Write the standard block header which begins the Video File Environment
    Block.  Check the bMultiple input parameter to determine whether we
    need to write to a single file or to multiple files. */

    m_FileHeader[nWords++] = 0; // FieldDelimiter
    m_FileHeader[nWords++] = 0; // FieldDelimiter

    m_FileHeader[nWords++] = 0xFFFF; // Event = FF, DataType = FF
    m_FileHeader[nWords++] = 0xFFFF; // Field Counter

    m_FileHeader[nWords++] = 0; // BlockLength HighWord
    m_FileHeader[nWords++] = 0; // BlockLength LowWord (Gets filled in at end of function)

    /* Following the Video Environment Block Header is the Video
       Environment Block Data. */

    m_FileHeader[nWords++] = 0x0101; // CPUFMT_KYWRD
    m_FileHeader[nWords++] = 1;      // count
    m_FileHeader[nWords++] = 0x0101; // Little Endian
    m_FileHeader[nWords++] = 0x0101; // Little Endian


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

    m_FileHeader[nWords++] = 0x0102; // Data types keyword
    m_FileHeader[nWords++] = 1;      // count
    m_FileHeader[nWords++] = 0x0800; // Video datatype
    m_FileHeader[nWords++] = 0;      // filler

    m_FileHeader[nWords++] = 0x0103; // Video rate keyword
    m_FileHeader[nWords++] = 1;      // count
    fdata = ProjectParameters.VideoSystem.fCameraCaptureRate;
    memcpy(&m_FileHeader[nWords], &fdata, 4);
    nWords += 2;

    m_FileHeader[nWords++] = 0x0104; // Subsample rate keyword
    m_FileHeader[nWords++] = 1;      // count
    //fdata = (float)Midasboard[0].dtruefieldrate/nframeratio;
    fdata = ProjectParameters.VideoSystem.fCameraCaptureRate; // Same as for 0x0103
    memcpy(&m_FileHeader[nWords], &fdata, 4);
    nWords += 2;

    /////
    // Store this location (word offset) so that we can poke the actual value later
    /////
    m_SensorResolution_iWord = nWords+2;
    m_FileHeader[nWords++] = 0x0105; // MAXXY keyword
    m_FileHeader[nWords++] = 1;      // count
    m_FileHeader[nWords++] = 1280;
    m_FileHeader[nWords++] = 1024;

	/* Write camera type (8 bits), camera sync (8 bits) and
       VPAT revision level (16 bits) information.
       We define the 16 bits of ndata for camera type and syncrate as consisting
       of bits 15 - 8 (= camera type) and bits 7 - 0 (= syncrate)
       So for example camera type 45 and syncrate 240 would be
       stored as 2DF0h (=11,760 decimal) */

    int nCameraType = 0;
    int nSyncRate = int (0.5f + ProjectParameters.VideoSystem.fCameraCaptureRate);
    int wEagleVersionNumber = 1;

    m_FileHeader[nWords++] = 0x0114; // CameraType and SyncRate
    m_FileHeader[nWords++] = 1;      // count
    m_FileHeader[nWords++] = (unsigned short)((nCameraType << 8) + nSyncRate);
    m_FileHeader[nWords++] = wEagleVersionNumber;


    /////
    // Store this location (byte offset) so that we can poke the frame count value
    // at the end of recording
    /////
    m_FrameCount_FilePos = 2*(nWords+2);

    m_FileHeader[nWords++] = 0x0115; // Number of frames keyword
    m_FileHeader[nWords++] = 1;      // count
    m_FileHeader[nWords++] = 0;      // HighWord of frame count
    m_FileHeader[nWords++] = 0;      // LowWord of frame count

    // Fill in the header size

    m_FileHeader[5] = nWords/2;

   // The actual number of frames gets filled in when collection is completed

    m_FileHeader_nWords = nWords;

    return nWords;
}
