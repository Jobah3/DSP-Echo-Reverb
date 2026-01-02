/**
 * @file
 * @brief Functions using to read/write a wavefile in the single core simulator.
 *
 * Project/Subproject: Cirrus Logic C library/ccc <p>
 * Component: Standalone simulator wave file functions.
 *
 * Language: English, Code page: ASCII, Coding standard: MG-CPP-DOXY-OCT08 <p>
 *
 */

#ifndef _H_WAVFILE
#define _H_WAVFILE

typedef void WAVREAD_HANDLE;
typedef void WAVWRITE_HANDLE;

#ifndef NULL
#define NULL ((void*)0)
#endif



/** @name  Wavefile support
 *
 * When running code in the simulator, there is support to load/write a PCM wave file
 * as defined in dsplib/wavefile.h.  The wave functions are implemented as special hooks in the single core simulator
 * and are thus not available when running on an actual hardware DSP platform.
 *
 * Below is an example on how to read in a wave file:
 *
 * @code
 * #include <stdio.h>
 * #include <dsplib\wavefile.h>
 * int main(int argc, char *argv[])
 * {
 *    WAVREAD_HANDLE *handle;
 *    int nChannels;
 *    int bitsPerFrame;
 *    int sampleRate;
 *    int numberOfFrames;
 *    int sample;
 *    int i;
 *    cl_wavread_open("f:\\11k16bitpcm.wav", handle);
 *
 *    if(handle == NULL)
 *    {
 *        printf("Unable to open file\n");
 *        return -1;
 *    }
 *
 *    nChannels = cl_wavread_getnchannels(handle);
 *    bitsPerFrame = cl_wavread_bits_per_sample(handle);
 *    sampleRate = cl_wavread_frame_rate(handle);
 *    numberOfFrames =  cl_wavread_number_of_channel_samples(handle);
 *
 *    for(i = 0; i< numberOfFrames *nChannels ; i++)
 *    {
 *        sample = cl_wavread_recvsample(handle);
 *    }
 *
 *    cl_wavread_close(handle);
 *    return 0;
 * }
 *
 * @endcode
 *
 * Below is an example on how to write an 11Khz, 16bit mono wav file:
 *
 * @code
 * #include <stdio.h>
 * #include <stdfix.h>
 * #include <dsplib\wavefile.h>
 * #include <dsplib\dsp_math.h>
 *
 * int main(int argc, char *argv[])
 * {
 *     WAVWRITE_HANDLE * handle;
 *     int         i;
 *     _Accum      a = 0;
 *     _Accum      b = 0;
 *     int         temp;
 *
 *     // Open a 16 bit, mono, 11Khz wave file to be written.
 *     handle = cl_wavwrite_open("f:\\testfile2.wav", 16, 1, 11025);
 *     if(!handle)
 *     {
 *         printf("Error: Could not open wavefile.\n");
 *         return -1;
 *     }
 *
 *     for(i = 0 ; i < 100000; i++ )
 *     {
 *         a += (_Accum)(2.0 * 1000.0/ 11025.0);
 *         if(a > 1)
 *         {
 *             a-= 2;
 *         }
 *         b = cl_sin(a);
 *         temp= bitsr((_Fract) b);
 *         cl_wavwrite_sendsample(handle, temp);
 *     }
 *
 *     cl_wavwrite_close(handle);
 *     return 0;
 * }
 * @endcode
 */
 /**@{*/

 /**
  * @brief Opens a file for reading
  *
  * Support FOPEN_VARS variable substitution.  See stdio's fopen documentation.
  *
  * NOTE:  Only available in the single core simulator.
  *
  * @param[in] filename
  *            The path and filename to the file on the PC.  Note that this filename located in
  *            X Memory.
  *
  * @retval    Non_null
  *            A valid file handle pointer.
  *
  * @retval    Null
  *            A file error has occurred such as the file may not exist, the file may have a lock on it,
  *            the file may not be a valid wave file.
  *
  * C Include file: <dsp_lib/wavefile.h>
  *
  * @ingroup simulator
  */
int cl_wavread_open(char* filename, WAVREAD_HANDLE** info);

/**
 * @brief Closes a waveread file handle.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Waveread stream handle.
 *
 * @retval      0
 *              Success
 *
 * @retval      -1
 *              Failure
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavread_close(WAVREAD_HANDLE* handle);

/**
 * @brief Returns the number of channels in a wave file.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Waveread stream handle.
 *
 * @return      On success, the number of channels is returned.  On failure, -1 is returned.
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavread_getnchannels(WAVREAD_HANDLE* handle);

/**
 * @brief Returns the bits per channel field in the wave file header.
 *
 * Note that when samples are read using the waveread_recvsample function,
 * they are always 32 bit signed left justified
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Waveread stream handle.
 *
 * @return      On success, 8, 16, 24 or 32 is returned.  On failure, -1 is returned
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavread_bits_per_sample(WAVREAD_HANDLE* handle);

/**
 * @brief Returns the frame rate (sample rate) in a wave read file.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Waveread stream handle.
 *
 * @return  On success the framerate is returned.  On failure, -1 is returned.
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavread_frame_rate(WAVREAD_HANDLE* handle);

/*
 * @brief Returns the total number of frames in a file.
 *
 * For example in a stereo file there are two samples per each frame.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Waveread stream handle.
 *
 * @return On success the total number of frames is returned.  On failure, -1 is returned.
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavread_number_of_channel_samples(WAVREAD_HANDLE* handle);

/**
 * @brief Returns the current sample number in a wave read file.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Waveread stream handle.
 *
 * @return  On success the number of samples read is returned.  On failure, -1 is returned.
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavread_sample_number(WAVREAD_HANDLE* handle);

/**
 * @brief Returns if an end of file has occurred in a wavread file.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Waveread stream handle.
 *
 * @retval      0  Success
 * @retval      -1 Failure
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavread_eof(WAVREAD_HANDLE* handle);

/**
 * @brief Reads the next sample from the wave file.
 *
 * Note that the sample read is always 32 bit signed left justified.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Waveread stream handle.
 *
 * @return      On success a sample is returned.  On failure, 0 is returned. ( Silence)
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 */
int cl_wavread_recvsample(WAVREAD_HANDLE* handle, bool compressedStream);

/**
 * @brief Creates/opens a wave file for writing and creates the proper wave header.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   filename
 *              Address in X memory of the filename string.
 *
 * @param[in]   wBitsPerSample
 *              Bits per sample as stored in the file.
 *
 * @param[in]   nChannels
 *              Number of channels.
 *
 * @param[in]   nFrameRate
 *              Framerate field in the wavefile stored as samples per second.
 *
 * @return      On success a valid wavewrite file pointer is returned.  On failure, 0 (null) is returned.
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavwrite_open(char* filename, int wBitsPerSample, int nChannels, int nFrameRate, WAVWRITE_HANDLE** info);

/*
 * @brief Returns the total number of frames in a written file.
 *
 * For example in a stereo file there are two samples per each frame.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Waveread stream handle.
 *
 * @return On success the total number of frames is returned.  On failure, -1 is returned.
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavwrite_number_of_channel_samples(WAVREAD_HANDLE* handle);

/**
 * @brief Opens a wave file for writing.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   filename
 *              Address in X memory of the filename string.
 *
 * @param[in]   samplenumber
 *              Number of samples previously stored in the file.
 *
 * @return      On success a valid wavewrite file pointer is returned.  On failure, 0 (null) is returned.
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavwrite_reopen(char* filename, WAVWRITE_HANDLE* handle);

/**
 * @brief Closes a wavewrite file handle.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Wavewrite stream handle.
 *
 * @retval      0   Success
 * @retval      -1  Failure
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
void cl_wavwrite_close(WAVWRITE_HANDLE* handle);

/**
 * @brief Writes a sample to the wavefile.
 *
 * Writes a 32 bit left justified signed sample to the wave file.
 * Note it is the callers responsibility to keep track of what channel the data
 * is.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Wavewrite stream handle.
 *
 * @param[in]   sample
 *              A 32 bit left justified signed sample.
 *
 * @param[in]   rounidng
 *              Flag indicating whether PCM samples should be rounded
 *              by adding (1 << (sampleSizeInBits - 1)) before being
 *              written to the output stream/file.
 *
 * @retval      0           Success
 * @retval      nonZero     Failure
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
void cl_wavwrite_sendsample(WAVWRITE_HANDLE* handle, int sample, bool rounidng);

/**
 * @brief Returns how many samples have been written to the wave file.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Wavewrite stream handle.
 *
 * @return      On success the number of samples that have been written will be
 *              returned.  On failure, -1 is returned.
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
int cl_wavwrite_sample_number(WAVWRITE_HANDLE* handle);

/**
 * @brief Updates wave header.
 *
 * NOTE:  Only available in the single core simulator.
 *
 * @param[in]   handle
 *              Wavewrite stream handle.
 *
 * @param[in]   wBitsPerSample
 *             	Bits per sample.
 *
 * @param[in]   nChannels
 *              Number of channels.
 *
 * @param[in]   nFrameRate
 *              Sample rate.
 *
 * @return     	0			Success
 * 				nonzero     Failure
 *
 * C Include file: <dsp_lib/wavefile.h>
 *
 * @ingroup simulator
 *
 */
void cl_wavwrite_update_wave_header(WAVWRITE_HANDLE* handle);

/**@}*/

#endif // _H_WAVFILE
