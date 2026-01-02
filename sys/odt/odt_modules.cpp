#include "haos.h"
#include <iostream>

extern HAOS_Odt_t PcmDecoder_odt;
extern HAOS_Odt_t AudioManager_odt;
extern HAOS_Mif_t fxMIF;

namespace ODT
{
	HAOS_OdtEntry_t coreODT[MAX_NUMBER_OF_CORES][MAX_NUMBER_OF_MODULES_PER_CORE] =
	{
		// Core 0 ODT
		{
			{PcmDecoder_odt->MIF, PcmDecoder_odt->moduleID},
			{&fxMIF, 0x50},
			{AudioManager_odt->MIF, AudioManager_odt->moduleID},
			{0, 0} // null entry terminates the table of modules
		},
		// Core 1 ODT
		{
			{0, 0} // null entry terminates the table of modules
		},
		// Core 2 ODT
		{
			{0, 0} // null entry terminates the table of modules
		}
	};
	
	void* masterODT[MAX_NUMBER_OF_CORES];

	void** getMasterTable()
	{	
		int32_t coreIdx = 0;
		pHAOS_OdtEntry_t pCoreODT = coreODT[coreIdx];
		masterODT[0] = pCoreODT;
		masterODT[1] = NULL;
		masterODT[2] = NULL;

		return masterODT;
	}

}