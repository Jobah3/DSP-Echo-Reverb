/**
 * wavefile.cpp
 *
 *  Created on: Apr 10, 2024
 *      Author: pekez
 */
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <cstring>
#include <cstdint>
#include <errno.h>
#include <assert.h>

#include "wavefile.h"

 //================================ local helper types ================================
struct RIFFHDR
{
    char chunkID[4];                    // Must be "RIFF"
    unsigned int fileSize;              // Size of the file
    char riffType[4];                   // Must be "WAVE"
};

struct FORMATHDR
{
    char           fmtID[4];           /* Must be "fmt " */
    unsigned int   fmtSize;            /* Format hdr size */
    unsigned short wFormatTag;         /* format type */
    unsigned short nChannels;          /* number of channels (i.e. mono, stereo...) */
    unsigned int   nSamplesPerSec;     /* sample rate */
    unsigned int   nAvgBytesPerSec;    /* for buffer estimation */
    unsigned short nBlockAlign;        /* block size of data */
    unsigned short wBitsPerSample;     /* number of bits per sample of mono data */
};

struct DATAHDR
{
    char            dataID[4];
    unsigned int    dataSize;
};

struct COMBINEDHDR
{
    RIFFHDR     riffHdr;
    FORMATHDR   formatHdr;
    DATAHDR     dataHdr;
};

#define     WAVE_FORMAT_PCM         1




typedef struct
{
    FILE* fileHandle;

    unsigned int    nChannels;
    unsigned int    nSamplesPerSecond;
    unsigned int    bitsPerSample;
    int             nCurrentSample;
    FORMATHDR       formatHdr;
    int             nChannelSamples;


}wavefile_info_t, * pWavefile_info_t;

static wavefile_info_t inputWaveFileInfo = { 0 };
static wavefile_info_t outputWaveFileInfo = { 0 };


static int read_wave_hdr(FILE* handle, FORMATHDR* waveFormat, RIFFHDR* riffHdr, DATAHDR* dataHdr);
static void mod_wave_header(const pWavefile_info_t info);

//============================ published functions from dsplib/wavefile.h ===============

int cl_wavread_open(char* filename, WAVREAD_HANDLE** info)
{
    int retValue = 1;
    std::string fileName = filename;
    *info = static_cast<WAVREAD_HANDLE*>(&inputWaveFileInfo);

    FILE* fileHandle = fopen(fileName.c_str(), "rb");

    if (fileHandle == NULL)
    {
        return -1;
    }
    inputWaveFileInfo.fileHandle = fileHandle;


    // try to readwave header
    FORMATHDR formatHdr;
    RIFFHDR riffHdr;
    DATAHDR dataHdr;

    if (!read_wave_hdr(fileHandle, &formatHdr, &riffHdr, &dataHdr))
    {
        int bytesPerFrame = formatHdr.nChannels * (formatHdr.wBitsPerSample / 8);

        inputWaveFileInfo.nChannels = formatHdr.nChannels;
        inputWaveFileInfo.bitsPerSample = formatHdr.wBitsPerSample;
        inputWaveFileInfo.nCurrentSample = 0;
        inputWaveFileInfo.nChannelSamples = dataHdr.dataSize / bytesPerFrame;
        inputWaveFileInfo.formatHdr = formatHdr;
        inputWaveFileInfo.nSamplesPerSecond = formatHdr.nSamplesPerSec;
    }
    else
    {
        // Not a PCM wave file
        inputWaveFileInfo.bitsPerSample = 32;
        inputWaveFileInfo.nCurrentSample = 0;
        inputWaveFileInfo.nChannelSamples = -1;
        inputWaveFileInfo.nSamplesPerSecond = 0;

        // move fileReadPtr to the beginning of file
        fclose(fileHandle);
        fileHandle = fopen(fileName.c_str(), "rb");

        // input file is not WAVE
        retValue = 0;
    }

    return retValue;
}

int cl_wavread_close(WAVREAD_HANDLE* handle)
{
    if (handle != NULL)
    {
        wavefile_info_t* info = static_cast<wavefile_info_t*>(handle);
        int result = fclose(inputWaveFileInfo.fileHandle);
        handle = NULL;
        return result;
    }
    else
    {
        return -1;
    }
}

int cl_wavread_getnchannels(WAVREAD_HANDLE* handle)
{
    if (handle != NULL) {
        wavefile_info_t* info = static_cast<wavefile_info_t*>(handle);
        return info->nChannels;
    }
    else {
        return -1;
    }
}

int cl_wavread_bits_per_sample(WAVREAD_HANDLE* handle)
{
    if (handle != NULL) {
        wavefile_info_t* info = static_cast<wavefile_info_t*>(handle);
        return info->bitsPerSample;
    }
    else {
        return -1;
    }
}

int cl_wavread_frame_rate(WAVREAD_HANDLE* handle)
{
    if (handle != NULL) {
        wavefile_info_t* info = static_cast<wavefile_info_t*>(handle);
        return info->nSamplesPerSecond;
    }
    else {
        return -1;
    }
}

int cl_wavread_number_of_channel_samples(WAVREAD_HANDLE* handle)
{
    if (handle != NULL) {
        wavefile_info_t* info = static_cast<wavefile_info_t*>(handle);
        return info->nChannelSamples;
    }
    else {
        return -1;
    }
}

int cl_wavread_sample_number(WAVREAD_HANDLE* handle)
{
    if (handle != NULL) {
        wavefile_info_t* info = static_cast<wavefile_info_t*>(handle);
        return info->nCurrentSample;
    }
    else {
        return -1;
    }
}

int cl_wavread_eof(WAVREAD_HANDLE* handle)
{
    if (handle != NULL) {
        wavefile_info_t* info = static_cast<wavefile_info_t*>(handle);
        return feof(info->fileHandle);
    }
    else {
        return 1;
    }
}

int cl_wavread_recvsample(WAVREAD_HANDLE* handle, bool compressedStream)
{
    if (handle == NULL) {
        return 0;
    }
    else {
        wavefile_info_t* info = static_cast<wavefile_info_t*>(handle);
        unsigned char           ucData[4] = { 0,0,0,0 };
        int                     nItemsRead;
        int                     bytesPerSample;
        int                     retValue;

        FILE* fp = info->fileHandle;

        bytesPerSample = info->bitsPerSample >> 3;

        if (!compressedStream)
        {
            /* input file - PCM wave */
            nItemsRead = fread(ucData, sizeof(uint8_t), bytesPerSample, fp);
            if (nItemsRead < 1)
            {
                return 0;
            }
            info->nCurrentSample++;

            retValue = ucData[0] << 24;
            for (int i = 1; i < bytesPerSample; i++)
            {
                retValue >>= 8;
                retValue &= 0x00ffffff;
                retValue |= ucData[i] << 24;
            }
        }
        else
        {
            /* input file - compressed bitstream */

            assert((bytesPerSample != 3) && "Invalid size of input bitstream sample (24 bits)!");

            /* Read 32 bits from input file */
            nItemsRead = fread(ucData, sizeof(uint8_t), 4, fp);
            if (nItemsRead < 1)
            {
                return 0;
            }

            retValue = (ucData[0] << 24) | (ucData[1] << 16) | (ucData[2] << 8) | ucData[3];

            /* Adjust samples count */
            if (bytesPerSample == 4) info->nCurrentSample++;
            if (bytesPerSample == 1) info->nCurrentSample += 4;

            /* Do the correct byte order for DTS 16bits WAVE input samples */
            if (bytesPerSample == 2)
            {
                info->nCurrentSample += 2;
                retValue = (ucData[1] << 24) | (ucData[0] << 16) | (ucData[3] << 8) | ucData[2];
            }

        }


        return retValue;
    }
}

int cl_wavwrite_open(char* filename, int wBitsPerSample, int nChannels, int nFrameRate, WAVREAD_HANDLE** info)
{
    std::string fileName = filename;
    *info = static_cast<WAVREAD_HANDLE*>(&outputWaveFileInfo);

    FILE* fileHandle = fopen(fileName.c_str(), "wb");
    if (fileHandle == NULL)
    {
        return -1;
    }

    COMBINEDHDR hdr;

    //
    // Create the riff header.
    //
    memcpy(hdr.riffHdr.chunkID, "RIFF", 4);
    hdr.riffHdr.fileSize = 0;
    memcpy(hdr.riffHdr.riffType, "WAVE", 4);
    memcpy(hdr.formatHdr.fmtID, "fmt ", 4);
    hdr.formatHdr.fmtSize = sizeof(FORMATHDR) - sizeof(hdr.formatHdr.fmtID) - sizeof(hdr.formatHdr.fmtSize);
    hdr.formatHdr.nAvgBytesPerSec = nFrameRate * nChannels * wBitsPerSample / 8;
    hdr.formatHdr.nBlockAlign = wBitsPerSample * nChannels / 8;
    hdr.formatHdr.nChannels = nChannels;
    hdr.formatHdr.nSamplesPerSec = nFrameRate;
    hdr.formatHdr.wBitsPerSample = wBitsPerSample;
    hdr.formatHdr.wFormatTag = WAVE_FORMAT_PCM;

    //
    // Create the data header.
    //
    memcpy(hdr.dataHdr.dataID, "data", 4);
    hdr.dataHdr.dataSize = 0;

    int numItems = fwrite(&hdr, sizeof(COMBINEDHDR), 1, fileHandle);
    if (numItems == 0)
    {
        fclose(fileHandle);
        return -1;
    }

    //
    // create an info structure for this file
    //
    outputWaveFileInfo.fileHandle = fileHandle;
    outputWaveFileInfo.nChannels = nChannels;
    outputWaveFileInfo.bitsPerSample = wBitsPerSample;
    outputWaveFileInfo.formatHdr = hdr.formatHdr;
    outputWaveFileInfo.nCurrentSample = 0;
    outputWaveFileInfo.nChannelSamples = 0;
    outputWaveFileInfo.nSamplesPerSecond = hdr.formatHdr.nSamplesPerSec;

    return 0;
}

int cl_wavwrite_reopen(char* filename, WAVWRITE_HANDLE* handle)
{
    if (handle == NULL)
    {
        return -1;
    }
    pWavefile_info_t info = static_cast<pWavefile_info_t>(handle);
    std::string fileName = filename;

    info->fileHandle = fopen(fileName.c_str(), "rb+");
    if (info->fileHandle == NULL)
    {
        return -1;
    }
    fseek(info->fileHandle, 0, SEEK_END);
    return 0;
}

void cl_wavwrite_close(WAVWRITE_HANDLE* handle)
{
    if (handle != NULL)
    {
        pWavefile_info_t info = static_cast<pWavefile_info_t>(handle);

        mod_wave_header(info);
        //
        // Perform the actual fclose on the pc.
        //
        fclose(info->fileHandle);


    }
}

void cl_wavwrite_sendsample(WAVWRITE_HANDLE* handle, int sample, bool rounidng)
{
    if (handle != NULL)
    {
        pWavefile_info_t info = static_cast<pWavefile_info_t>(handle);
        FILE* fileHandle = info->fileHandle;
        sample = sample >> (32 - info->bitsPerSample);
        fwrite(&sample, info->bitsPerSample >> 3, 1, fileHandle);
        info->nCurrentSample++;
    }
}

int cl_wavwrite_sample_number(WAVWRITE_HANDLE* handle)
{
    if (handle != NULL) {
        pWavefile_info_t info = static_cast<pWavefile_info_t>(handle);
        return info->nCurrentSample;
    }
    else {
        return 0;
    }
}

int cl_wavwrite_number_of_channel_samples(WAVREAD_HANDLE* handle)
{
    if (handle != NULL) {
        wavefile_info_t* info = static_cast<wavefile_info_t*>(handle);
        int bytesPerFrame = info->formatHdr.nChannels * (info->formatHdr.wBitsPerSample / 8);
        unsigned int dwDataSize = info->nCurrentSample * info->bitsPerSample / 8;
        info->nChannelSamples = dwDataSize / bytesPerFrame;
        return info->nChannelSamples;
    }
    else {
        return -1;
    }
}

void cl_wavwrite_update_wave_header(WAVWRITE_HANDLE* handle)
{
    if (handle != NULL)
    {
        pWavefile_info_t info = (pWavefile_info_t)handle;
        FILE* fileHandle = info->fileHandle;

        // write number of channels
        fseek(fileHandle, 22, SEEK_SET);
        fwrite(&info->nChannels, 2, 1, fileHandle);

        // write number of average bytes per second
        fseek(fileHandle, 28, SEEK_SET);
        int nAvgBytesPerSec = (info->nSamplesPerSecond * info->bitsPerSample * info->nChannels) / 8;
        fwrite(&nAvgBytesPerSec, 4, 1, fileHandle);

        // write block align
        fseek(fileHandle, 32, SEEK_SET);
        int nBlockAlign = (info->bitsPerSample * info->nChannels) / 8;
        fwrite(&nBlockAlign, 2, 1, fileHandle);

        // return to the end of the file
        fseek(fileHandle, 0, SEEK_END);
    }
}

//====================== local helper functions ==============================
int read_wave_hdr(FILE* handle, FORMATHDR* waveFormat, RIFFHDR* riffHdr, DATAHDR* dataHdr)
{
    int i;
    short fmtExtraSize;
    if (fread(riffHdr, sizeof(RIFFHDR), 1, handle) == 0)
    {
        return -1;
    }

    if (memcmp(riffHdr->chunkID, "RIFF", 4) &&  // "RIFF"
        memcmp(riffHdr->riffType, "WAVE", 4))
    {
        return -1;
    }

    /*
        if(fread(waveFormat, sizeof(FORMATHDR), 1, handle) == 0)
        {
            return -1;
        }
    */

    /* Check 'fmt ' chunk tag and size */
    if (fread(waveFormat, 8, 1, handle) == 0)
    {
        return -1;
    }

    /* Check if 'JUNK' chunk inserted */
    if (!memcmp(waveFormat->fmtID, "JUNK", 4))
    {
        /* Skip and discard JUNK chunk data encountered in the input file */
        for (i = 0; i < waveFormat->fmtSize; i++)
        {
            fgetc(handle);
        }

        /* Read 'fmt ' chunk tag & size */
        if (fread(waveFormat, 8, 1, handle) == 0)
        {
            return -1;
        }
    }

    /* Read the rest of thr 'fmt ' chunk */
    if (fread(&waveFormat->wFormatTag, sizeof(FORMATHDR) - 8, 1, handle) == 0)
    {
        return -1;
    }


    if (memcmp(waveFormat->fmtID, "fmt ", 4) &&
        !(waveFormat->fmtSize == 16 || waveFormat->fmtSize == 18) &&
        waveFormat->wFormatTag != WAVE_FORMAT_PCM)
    {

        return -1;

    }

    if (waveFormat->wBitsPerSample != 8 &&
        waveFormat->wBitsPerSample != 16 &&
        waveFormat->wBitsPerSample == 24 &&
        waveFormat->wBitsPerSample == 32)
    {
        return -1;
    }

    //
    // If the format is 18, read the extra information.
    //
    if (waveFormat->fmtSize == 18 || waveFormat->fmtSize == 40)
    {
        fread(&fmtExtraSize, 1, sizeof(unsigned short), handle);
        for (i = 0; i < fmtExtraSize; i++)
        {
            fgetc(handle);
        }

    }

    if (fread(dataHdr, sizeof(DATAHDR), 1, handle) == 0)
    {
        return -1;
    }
    if (memcmp(dataHdr->dataID, "data", 4) != 0)
    {
        return -1;
    }

    return 0;
}

// ****************************************************************************
// mod_wave_header
// ****************************************************************************
/// Performs a seek and modifies the wave header with the correct filesize and
/// data size.
///
/// @param[in]  writeInfo
///             Instance of the wavewrite class that contains the informations
///             about the wavew file being written to.
///
/// @retval     0               Sucess
/// @retval     nonzero         Failure
///
static void mod_wave_header(const pWavefile_info_t info)
{
    FILE* fileHandle = info->fileHandle;
    unsigned int dwDataSize = info->nCurrentSample * info->bitsPerSample / 8;
    unsigned int dwFileSize = dwDataSize + 36;

    //
    // Current Sample * Number of bytes per sample.
    //
    fseek(fileHandle, 4, SEEK_SET);
    fwrite(&dwFileSize, sizeof(unsigned int), 1, fileHandle);

    fseek(fileHandle, 40, SEEK_SET);
    fwrite(&dwDataSize, sizeof(unsigned int), 1, fileHandle);

}
