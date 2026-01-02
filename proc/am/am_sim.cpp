////////////////////////////////////////////////////////////////////////////////
//
// Author.......: Milica Tadic (tmilica)
// Contact......: milica.tadic@rt-rk.com
// Date.........: 24.04.2024.
//
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <string>
#include "am_sim.h"
#include "haos.h"
#include "haos_api.h"
#include "wavefile.h"
#include <stdint.h>

struct
{
	HAOS_PcmSample_t	gain;
	bool				mute;
	HAOS_PcmSample_t	channelTrim[NUMBER_OF_IO_CHANNELS];
	int32_t				channelRemap[NUMBER_OF_IO_CHANNELS];
	int32_t				reserved[32];
} AudioManager_mcv =
{
	1,	    // gain - 0.5
	0,		// mute - 0 - not muted, 1 - muted
	1,		// l_trim - 1 in 1.31 (0x7fffffff -> 0.9999999995343387)
	1,		// c_trim 1
	1,		// r_trim 2
	1,		// ls_trim 3
	1,		// rs_trim 4
	1,		// lb_trim 5
	1,		// rb_trim 6
	1,		// lfe0_trim 7
	1,		// lh_trim 8
	1,		// rh_trim 9
	1,		// lw_trim 10
	1,		// rw_trim 11 
	1,		// lt_trim 12
	1,		// rt_trim 13
	1,		// lfe1_trim 14
	1,		// res_trim 15
	1,		// l_trim - 16
	1,		// c_trim 17
	1,		// r_trim 18
	1,		// ls_trim 19
	1,		// rs_trim 20
	1,		// lb_trim 21
	1,		// rb_trim 22
	1,		// lfe0_trim 23
	1,		// lh_trim 24
	1,		// rh_trim 25
	1,		// lw_trim 26
	1,		// rw_trim 27 
	1,		// lt_trim 28
	1,		// rt_trim 29
	1,		// lfe1_trim 30
	1,		// res_trim 31
	0,						// IOBUFFER[0] --> output wave channel 0
	1,						// IOBUFFER[1] --> output wave channel 1
	2,						// IOBUFFER[2] --> output wave channel 2
	3,						// IOBUFFER[3] --> output wave channel 3
	4,            			// IOBUFFER[4] --> output wave channel 4
	5,            			// IOBUFFER[5] --> output wave channel 5
	NO_SOURCE,            	// IOBUFFER[6] --> output wave channel 6
	NO_SOURCE,            	// IOBUFFER[7] --> output wave channel 7
	NO_SOURCE,            	// IOBUFFER[8] --> output wave channel 8
	NO_SOURCE,            	// IOBUFFER[9] --> output wave channel 9
	NO_SOURCE,           	// IOBUFFER[10] --> output wave channel 10
	NO_SOURCE,           	// IOBUFFER[11] --> output wave channel 11
	NO_SOURCE,           	// IOBUFFER[12] --> output wave channel 12
	NO_SOURCE,           	// IOBUFFER[13] --> output wave channel 13
	NO_SOURCE,           	// IOBUFFER[14] --> output wave channel 14
	NO_SOURCE,            	// IOBUFFER[15] --> output wave channel 15
	NO_SOURCE,				// IOBUFFER[16] --> output wave channel 16
	NO_SOURCE,				// IOBUFFER[17] --> output wave channel 17
	NO_SOURCE,            	// IOBUFFER[18] --> output wave channel 18
	NO_SOURCE,            	// IOBUFFER[19] --> output wave channel 19
	NO_SOURCE,            	// IOBUFFER[20] --> output wave channel 20
	NO_SOURCE,            	// IOBUFFER[21] --> output wave channel 21
	NO_SOURCE,            	// IOBUFFER[22] --> output wave channel 22
	NO_SOURCE,            	// IOBUFFER[23] --> output wave channel 23
	NO_SOURCE,           	// IOBUFFER[24] --> output wave channel 24
	NO_SOURCE,           	// IOBUFFER[25] --> output wave channel 25
	NO_SOURCE,           	// IOBUFFER[26] --> output wave channel 26
	NO_SOURCE,           	// IOBUFFER[27] --> output wave channel 27
	NO_SOURCE,           	// IOBUFFER[28] --> output wave channel 28
	NO_SOURCE,            	// IOBUFFER[29] --> output wave channel 29
	NO_SOURCE,            	// IOBUFFER[30] --> output wave channel 30
	NO_SOURCE            	// IOBUFFER[31] --> output wave channel 31
};

HAOS_Mct_t AudioManager_mct =
{
	0,	// Pre-kick
	0,	// Post-kick
	0,	// Timer
	0,	// Frame
	AudioManager_brickFunction, // Brick
	0,	// AFAP
	0,	// Background
	0,	// Post-malloc
	0	// Pre-malloc
};

HAOS_Mif_t AudioManager_mif = { &AudioManager_mcv, &AudioManager_mct };

HAOS_Odt_t AudioManager_odt =
{
	{&AudioManager_mif, 0x60},
	{0,0} // null entry terminates the table of modules
};

HAOS_OdtEntry_t* AudioManager_odtPtr = AudioManager_odt;

void __fg_call AudioManager_brickFunction()
{
	HAOS_ChannelMask_t validChannelMask = HAOS::getValidChannelMask();
	HAOS_ChannelMask_t outputChannelMask = 0;
	int32_t samplesToWrite = BRICK_SIZE;
	int32_t channel = 0;
	int32_t inpCh = 0;
	int32_t currChannel = 0;
	HAOS_PcmSamplePtr_t* ioTablePtr = HAOS::getIOChannelPointerTable();
	HAOS_BrickBuffer_t temporaryBricks[NUMBER_OF_IO_CHANNELS];

	memset(temporaryBricks, 0, NUMBER_OF_IO_CHANNELS * sizeof(HAOS_BrickBuffer_t));

	// apply gain, trim and mute and write the result to TEMPBUFFERs
	for (int ioCh = 0; ioCh < NUMBER_OF_IO_CHANNELS; ioCh++)
	{
		HAOS_PcmSamplePtr_t ioChData = ioTablePtr[ioCh];
		HAOS_PcmSample_t globalGain = AudioManager_mcv.gain * !AudioManager_mcv.mute;

		if (HAOS::isActiveChannel(ioCh))
		{
			HAOS_PcmSample_t channelVolumeTrim = AudioManager_mcv.channelTrim[ioCh];
			HAOS_PcmSample_t* tempSample = temporaryBricks[ioCh];

			for (int sample = 0; sample < BRICK_SIZE; sample++)
			{
				if (globalGain * channelVolumeTrim == 1.0)
					*tempSample++ = ioChData[sample];
				else
					*tempSample++ = (globalGain * channelVolumeTrim * ioChData[sample]);

				// Data processed into temporary buffer, no need to have the original value anymore
				ioChData[sample] = 0;
			}
		}

		//refresh channels mask based on AudioManager MCV
		if (AudioManager_mcv.channelRemap[ioCh] != NO_SOURCE)
		{
			outputChannelMask |= 1 << ioCh;
		}
	}

	HAOS::setValidChannelMask(outputChannelMask);

	channel = 0;
	// apply remap, move samples to IOBUFFERs
	while (outputChannelMask != 0)
	{
		if (outputChannelMask & 1)
		{
			int ioCh = AudioManager_mcv.channelRemap[channel];
			HAOS_PcmSamplePtr_t ioChData = ioTablePtr[channel];
			for (int sample = 0; sample < samplesToWrite; sample++)
			{
				ioChData[sample] = temporaryBricks[ioCh][sample];
			}
			channel++;
		}
		outputChannelMask >>= 1;
	}

	if (!(HAOS::getFrameCounter() % 1000))
		std::cout << ".";
}
//==============================================================================
