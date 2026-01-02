#include <stdio.h>
#include <stdint.h>

typedef struct
{
	uint32_t on;                    // FX On/Off switch
	uint32_t mute;                  // Mute control
	uint32_t upmix;                 // Upmix control
	uint32_t delay;                 // Delay control (1 = enabled, 0 = disabled)
	uint32_t switchState[6];        // SWn switch state (0, 1, 2, or 3) for each output channel (0-5)
} FX_ControlPanel;

void FX_init(FX_ControlPanel* controlsInit);
void FX_processBlock();