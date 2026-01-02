/////////////////////////////////////////////////////////
// 	pcmdec_sim.cpp
//
//  Author.......: Katarina Cvetkovic (katarinac)
//	Contact......: katarina.cvetkovic@rt-rk.com
//	Date.........: Apr 15, 2024
//
//	This file contains definitions of all PCM decoder function
//	called in main.cpp that should read samples from input file
//
/////////////////////////////////////////////////////////

#include <iostream>
#include <string>
#include "haos_api.h"
#include "haos.h"
#include "pcmdec_sim.h"
#include "bitripper_sim.h"
#include "wavefile.h"
#include <string.h>

static HAOS_PcmSample_t* tempSampleBuffer;
static HAOS_PcmSample_t* sampleBuffer;
static HAOS_PcmSample_t* rdPtr;  //read pointer of temp input buffer

struct
{
	uint32_t pcmEnable;
	uint32_t srcActiveChannels[NUMBER_OF_IO_CHANNELS];
	uint32_t reserved[32];   // just to be sure no host comm message stomps on something
} PcmDecoder_mcv =
{
	//default values for PcmDecoderMCV
1,						// enable pcm decoder
0,						// input wave channel 0,
1,						// input wave channel 1,
NO_SOURCE,				// input wave channel 2,
NO_SOURCE,				// input wave channel 3,
NO_SOURCE,				// input wave channel 4,
NO_SOURCE,				// input wave channel 5,
NO_SOURCE,				// input wave channel 6,
NO_SOURCE,				// input wave channel 7,
NO_SOURCE,				// input wave channel 8,
NO_SOURCE,				// input wave channel 9,
NO_SOURCE,				// input wave channel 10,
NO_SOURCE,				// input wave channel 11,
NO_SOURCE,				// input wave channel 12,
NO_SOURCE,				// input wave channel 13,
NO_SOURCE,				// input wave channel 14,
NO_SOURCE,				// input wave channel 15
NO_SOURCE,				// input wave channel 16,
NO_SOURCE,				// input wave channel 17,
NO_SOURCE,				// input wave channel 18,
NO_SOURCE,				// input wave channel 19,
NO_SOURCE,				// input wave channel 20,
NO_SOURCE,				// input wave channel 21,
NO_SOURCE,				// input wave channel 22,
NO_SOURCE,				// input wave channel 23,
NO_SOURCE,				// input wave channel 24,
NO_SOURCE,				// input wave channel 25,
NO_SOURCE,				// input wave channel 26,
NO_SOURCE,				// input wave channel 27,
NO_SOURCE,				// input wave channel 28,
NO_SOURCE,				// input wave channel 29,
NO_SOURCE,				// input wave channel 30,
NO_SOURCE,				// input wave channel 31,
};

static void __fg_call PcmDecoder_premallocFunction();
static void __fg_call PcmDecoder_postmallocFunction();

HAOS_Mct_t PcmDecoder_mct =
{
	PcmDecoder_prekickFunction, // Pre-kick
	PcmDecoder_postkickFunction,// Post-kick
	0,	// Timer
	0,	// Frame
	PcmDecoder_brickFunction, // Brick
	0,	// AFAP
	0,	// Background
	PcmDecoder_postmallocFunction,	// Post-malloc
	PcmDecoder_premallocFunction	// Pre-malloc
};

HAOS_Mif_t PcmDecoder_mif = { &PcmDecoder_mcv, &PcmDecoder_mct };

HAOS_Odt_t PcmDecoder_odt =
{
	{&PcmDecoder_mif, 0x10},
	{0, 0}
};

HAOS_OdtEntry_t* PcmDecoder_odtPtr = PcmDecoder_odt;

static HAOS_FrameData_t PcmDecoder_frameData;
static HAOS_CopyToIOPtrs_t PcmDecoder_copyToIOPtrs;

static void __fg_call PcmDecoder_premallocFunction()
{
	std::cout << "start premalloc PCM" << std::endl;

	sampleBuffer = (double*)malloc(BRICK_SIZE * HAOS::getInputStreamChCnt() * sizeof(HAOS_PcmSample_t));
	tempSampleBuffer = (double*)malloc(BRICK_SIZE * HAOS::getInputStreamChCnt() * sizeof(HAOS_PcmSample_t));
}

static void __fg_call PcmDecoder_postmallocFunction()
{
	std::cout << "start postmalloc PCM" << std::endl;

	if (sampleBuffer) memset(sampleBuffer, 0, BRICK_SIZE * HAOS::getInputStreamChCnt() * sizeof(HAOS_PcmSample_t));
	if (tempSampleBuffer) memset(tempSampleBuffer, 0, BRICK_SIZE * HAOS::getInputStreamChCnt() * sizeof(HAOS_PcmSample_t));
}


static void __fg_call PcmDecoder_prekickFunction(void* PcmDecoder_mifPtr)
{
	std::cout << "start prekick PCM" << std::endl;

	std::cout << "end prekick PCM" << std::endl;
}

static void __fg_call PcmDecoder_postkickFunction()
{
	std::cout << "start postkick PCM" << std::endl;

	//set values of frame data structure

	for (int ch = 0; ch < NUMBER_OF_IO_CHANNELS; ch++)
	{
		if (PcmDecoder_mcv.srcActiveChannels[ch] != NO_SOURCE)
		{
			//init io masks of active channels
			PcmDecoder_frameData.inputChannelMask |= (1 << ch);
			PcmDecoder_frameData.outputChannelMask |= (1 << ch);
		}
	}
	PcmDecoder_frameData.sampleRate = HAOS::getInputStreamFS();
	PcmDecoder_frameData.decodeInfo = DECODE_INFO_PCM;
	PcmDecoder_copyToIOPtrs.frameData = &PcmDecoder_frameData;

	HAOS::requestMemoryAllocation(true);

	std::cout << "end postkick PCM" << std::endl;
}

static void __fg_call PcmDecoder_brickFunction()
{
	//1. extract bits from FIFO0 to temp buffers
	//2. copy buffers to IO
	//3. repeat process
	int inputChannelMask;
	int i = 0;
	int j = 0;
	int sample = 0;
	uint32_t nInputChannels = HAOS::getInputStreamChCnt();

	int loopSize = BRICK_SIZE * nInputChannels;

	if (PcmDecoder_mcv.pcmEnable)
	{
		if (!HAOS::getInputStreamEOF())
		{
			for (sample = 0; sample < BRICK_SIZE * nInputChannels;)
			{
				for (int ch = 0; ch < nInputChannels; ch++)
				{
					tempSampleBuffer[sample] = BitRipper::extractBits(32) / SAMPLE_SCALE;
					sample++;
				}
			}

			//copy interleaved samples to individual channels
			for (int ch = 0; ch < nInputChannels; ch++)
			{
				for (int sample = ch; sample < loopSize; sample += nInputChannels)
				{
					sampleBuffer[i] = tempSampleBuffer[sample];
					i++;
				}
			}

			//set read pointer
			rdPtr = sampleBuffer;
			inputChannelMask = PcmDecoder_frameData.inputChannelMask;
			{
				PcmDecoder_frameData.outputChannelMask = PcmDecoder_frameData.inputChannelMask;;
				while (inputChannelMask != 0)
				{
					if (inputChannelMask & 1)
					{
						PcmDecoder_copyToIOPtrs.IOBufferPtrs[j] = rdPtr;
						rdPtr += BRICK_SIZE;
					}
					j++;
					inputChannelMask = inputChannelMask >> 1;
				}
			}

			PcmDecoder_frameData.sampleRate = HAOS::getInputStreamFS();

			//copy Brick to IO
			HAOS::copyBrickToIO(&PcmDecoder_copyToIOPtrs);
			HAOS::setValidChannelMask(PcmDecoder_frameData.outputChannelMask);
		}
	}

	if (HAOS::getEndOfProcessing())
	{
		if (sampleBuffer)
		{
			free(sampleBuffer);
			sampleBuffer = NULL;
		}

		if (tempSampleBuffer)
		{
			free(tempSampleBuffer);
			tempSampleBuffer = NULL;
		}
	}
}
