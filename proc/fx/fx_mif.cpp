#include "fx.h"
#include "haos_api.h"
#include <string>
#include <iostream>

void __fg_call FX_processBrick();
void __fg_call FX_preKick(void* mif);
void __fg_call FX_postKick();

// MCV structure - must match FX_ControlPanel in content and order
struct
{
	uint32_t on;                    // FX On/Off switch
	uint32_t mute;                  // Mute control
	uint32_t upmix;                 // Upmix control
	uint32_t delay;                 // Delay control (1 = enabled, 0 = disabled)
	uint32_t switchState[6];        // SWn switch state (0, 1, 2, or 3) for each output channel (0-5) 
} fxMCV = { 1, 0, 1, 1, {0, 0, 0, 0, 0, 0}};


HAOS_Mct_t fxMCT =
{
	FX_preKick, // Pre-kick
	FX_postKick,// Post-kick
	0,			// Timer
	0,			// Frame
	FX_processBrick, // Brick
	0,			// AFAP
	0,			// Background
	0,			// Post-malloc
	0			// Pre-malloc
};

HAOS_Mif_t fxMIF = { &fxMCV, &fxMCT };

void __fg_call FX_preKick(void* mif)
{
	FX_init((FX_ControlPanel*)&fxMCV);
}

void __fg_call FX_postKick()
{
	FX_init((FX_ControlPanel*)&fxMCV);
}

void __fg_call FX_processBrick()
{
	FX_processBlock();


	HAOS::setValidChannelMask(0b11 + (0b111100 * fxMCV.upmix));
}


