/////////////////////////////////////////////////////////
// pcmdec_sim.h
//
//  Author.......: Katarina Cvetkovic (katarinac)
//	Contact......: katarina.cvetkovic@rt-rk.com
//	Date.........: Apr 15, 2024
//
//	Declarations of PCM decoder function for reading input wave file
/////////////////////////////////////////////////////////

#ifndef __PCM_DECODER_H__
#define __PCM_DECODER_H__

#define __fg_call
#define __bg_call

void __fg_call PcmDecoder_prekickFunction(void* PcmDecoder_mifPtr);
void __fg_call PcmDecoder_postkickFunction();
void __fg_call PcmDecoder_brickFunction();

void generateFrameData();

#endif /* __PCM_DECODER_H__ */
