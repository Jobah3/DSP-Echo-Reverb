/*
 * FX Module - Audio Effect Processing
 * Author: Jovan Janjic
 *
 * Implementacija audio efekata prema datoj semi
 * Ulazni signali procesuiraju se kroz:
 * - Switch sa 4 stanja (SWn) 0, 1, 2, 3
 * - Blokove kasnjenja: n*10ms i (n+1)*3ms (n vrednost switcha)
 * - Pojacavacke elemente: -6dB, -3dB, i -1dB
 * - Sabirace
 *
 * Parametri obrade:
 * - Delay
 * - Mute
 * - Upmix
 * - Maksu izlaznih kanala (maximum 6)
 */

#include "fx.h"
#include <string.h>
#include "haos_api.h"

#define BLOCK_SIZE BRICK_SIZE
#define sampleBuffer HAOS::getIOChannelPointerTable()

#define IO_MAX_NUM_CHANNEL 6

// Sample rate (typically 48kHz for audio processing)
#define SAMPLE_RATE 48000

static FX_ControlPanel moduleControl;

// Delay buffer size - odgovara najvecem delayu (30ms at 48kHz = 1440 samples)
#define DBUFSIZE 2048

// Vrednosti pojacanja (linearna skala)
//-------------------------------------------------
#define GAIN_6DB  0.501  // -6dB = 10^(-6/20)
#define GAIN_3DB  0.708  // -3dB = 10^(-3/20)
#define GAIN_1DB  0.891  // -1dB = 10^(-1/20)

// Delay state struktura
//-------------------------------------------------
typedef struct
{
    double* delayBuffer;
    const double* bufferEndPointer;
    double* readPointer;
    double* writePointer;
    int delaySamples;
} DelayState;

// Delay buffers - nizovi za svaki od kanala
//-------------------------------------------------
#define MAX_DELAY_CHANNELS 6
static DelayState delayChannels[MAX_DELAY_CHANNELS];  // (switchState*10ms)
static double delayBuffers[MAX_DELAY_CHANNELS][DBUFSIZE];

static DelayState delayN1_3ms[MAX_DELAY_CHANNELS]; // ((switchState+1)*3ms)
static double delayBufferN1_3ms[MAX_DELAY_CHANNELS][DBUFSIZE];

// Inicijalizacija delaya
//-------------------------------------------------
static void delayInit(DelayState* delayState, double* delayBuffer, const int delayBufLen, int delaySamples)
{
    // Dodela bafera i ogranièavanje maksimalnog kašnjenja
    delayState->delayBuffer = delayBuffer;
    delayState->bufferEndPointer = delayState->delayBuffer + delayBufLen;
    delayState->delaySamples = (delaySamples < delayBufLen) ? delaySamples : delayBufLen - 1;

    // Inicijalizovanje bafera na 0
    memset(delayBuffer, 0, delayBufLen * sizeof(double));

    // Postavljanje writePointera na poèetak
    delayState->writePointer = delayState->delayBuffer;

    // Postavljanje readPointera na delay offset iza writePointera
    delayState->readPointer = delayState->delayBuffer + (delayBufLen - delayState->delaySamples);
    if (delayState->readPointer >= delayState->bufferEndPointer)
        delayState->readPointer = delayState->delayBuffer;
}

// Procesiranje delaya
//-------------------------------------------------
static double delayProcess(double input, DelayState* delayState)
{
    // Ako je delay onemoguæen vraæa se neizmenjen signal
    if (!moduleControl.delay)
    {
        return input;
    }

    // Proèitaj delay izlaz (pre pisanja novog)
    double output = *delayState->readPointer;

    // Upisivanje ulaza na trenutnu write poziciju
    *delayState->writePointer = input;

    // Pomeraj pokazivaèa
    delayState->writePointer++;
    if (delayState->writePointer >= delayState->bufferEndPointer)
        delayState->writePointer = delayState->delayBuffer;

    delayState->readPointer++;
    if (delayState->readPointer >= delayState->bufferEndPointer)
        delayState->readPointer = delayState->delayBuffer;

    return output;
}

// Funkcija sabiranja sa ugradjenim limitiranjem
//-------------------------------------------------
static double add(double input0, double input1)
{
    double ret = input0 + input1;
    if (ret >= 1.0) ret = 0.99999999;
    if (ret < -1.0) ret = -1.0;
    return ret;
}

// Izraèunavanje delay semplova iz milisekundi      10ms=480 30ms=1440
//-------------------------------------------------
static int msToSamples(double ms)
{
    return (int)(ms * SAMPLE_RATE / 1000.0);
}

// FX inicijalizacija
//-------------------------------------------------
void FX_init(FX_ControlPanel* controlsInit)
{
    // Prihvatanje vrednosti parametara
    memcpy(&moduleControl, controlsInit, sizeof(FX_ControlPanel));

    // Ogranicavanje vrednosti switcha
    for (int i = 0; i < 6; i++)
    {
        if (moduleControl.switchState[i] > 3)
            moduleControl.switchState[i] = 3;
    }

    // Inicijalizacija delay bafera za sve kanale, svako kasnjenje zavisi od stanja konkretnog switcha
    for (int ch = 0; ch < MAX_DELAY_CHANNELS; ch++)
    {
        int n = moduleControl.switchState[ch];

        // n*10ms
        int delaySamples1 = msToSamples(n * 10.0);
        delayInit(&delayChannels[ch], delayBuffers[ch], DBUFSIZE, delaySamples1);

        // (n+1)*3ms
        int delaySamples2 = msToSamples((n + 1) * 3.0);
        delayInit(&delayN1_3ms[ch], delayBufferN1_3ms[ch], DBUFSIZE, delaySamples2);
    }
}

// Blok obrade izlaznih kanala
//-------------------------------------------------
void FX_processBlock()
{
    if (!moduleControl.on)
    {
        return;
    }

    for (int32_t i = 0; i < BLOCK_SIZE; i++)
    {
        // Procesiranje svih kanala
        if (moduleControl.upmix)
        {
            // Razvijati CH0, CH2, CH4 iz CH0 
            double inputLeft = sampleBuffer[0][i];
            if (moduleControl.mute) inputLeft = 0.0;
            double mainPathLeft = inputLeft * GAIN_6DB;

            // Razvijati CH1, CH3, CH5 iz CH1
            double inputRight = sampleBuffer[1][i];
            if (moduleControl.mute) inputRight = 0.0;
            double mainPathRight = inputRight * GAIN_6DB;

            // CH0
            double effectPath = inputLeft;
            effectPath = delayProcess(effectPath, &delayChannels[0]); //(switchState*10ms)
            effectPath = effectPath * GAIN_3DB;
            double firstSum = add(mainPathLeft, effectPath);
            double effectPath2 = delayProcess(effectPath, &delayN1_3ms[0]); // (switchState[0]+1)*3ms
            effectPath2 = effectPath2 * GAIN_1DB;
            sampleBuffer[0][i] = add(firstSum, effectPath2);

            // CH1
            effectPath = inputRight;
            effectPath = delayProcess(effectPath, &delayChannels[1]); //(switchState * 10ms)
            effectPath = effectPath * GAIN_3DB;
            firstSum = add(mainPathRight, effectPath);
            effectPath2 = delayProcess(effectPath, &delayN1_3ms[1]); // (switchState[1]+1)*3ms
            effectPath2 = effectPath2 * GAIN_1DB;
            sampleBuffer[1][i] = add(firstSum, effectPath2);

            // CH2
            effectPath = inputLeft;
            effectPath = delayProcess(effectPath, &delayChannels[2]); //(switchState * 10ms)
            effectPath = effectPath * GAIN_3DB;
            firstSum = add(mainPathLeft, effectPath);
            effectPath2 = delayProcess(effectPath, &delayN1_3ms[2]); // (switchState[2]+1)*3ms
            effectPath2 = effectPath2 * GAIN_1DB;
            sampleBuffer[2][i] = add(firstSum, effectPath2);

            // CH3
            effectPath = inputRight;
            effectPath = delayProcess(effectPath, &delayChannels[3]); //(switchState * 10ms)
            effectPath = effectPath * GAIN_3DB;
            firstSum = add(mainPathRight, effectPath);
            effectPath2 = delayProcess(effectPath, &delayN1_3ms[3]); // (switchState[3]+1)*3ms
            effectPath2 = effectPath2 * GAIN_1DB;
            sampleBuffer[3][i] = add(firstSum, effectPath2);

            // CH4
            effectPath = inputLeft;
            effectPath = delayProcess(effectPath, &delayChannels[4]); //(switchState * 10ms)
            effectPath = effectPath * GAIN_3DB;
            firstSum = add(mainPathLeft, effectPath);
            effectPath2 = delayProcess(effectPath, &delayN1_3ms[4]); // (switchState[4]+1)*3ms
            effectPath2 = effectPath2 * GAIN_1DB;
            sampleBuffer[4][i] = add(firstSum, effectPath2);

            // CH5
            effectPath = inputRight;
            effectPath = delayProcess(effectPath, &delayChannels[5]); //(switchState * 10ms)
            effectPath = effectPath * GAIN_3DB;
            firstSum = add(mainPathRight, effectPath);
            effectPath2 = delayProcess(effectPath, &delayN1_3ms[5]); // (switchState[5]+1)*3ms
            effectPath2 = effectPath2 * GAIN_1DB;
            sampleBuffer[5][i] = add(firstSum, effectPath2);
        }
        else
        {
            // Razvijati CH0 iz CH0 
            double inputLeft = sampleBuffer[0][i];
            if (moduleControl.mute) inputLeft = 0.0;
            double mainPathLeft = inputLeft * GAIN_6DB;

            // Razvijati CH1 iz CH1
            double inputRight = sampleBuffer[1][i];
            if (moduleControl.mute) inputRight = 0.0;
            double mainPathRight = inputRight * GAIN_6DB;

            // CH0
            double effectPath = inputLeft;
            effectPath = delayProcess(effectPath, &delayChannels[0]); //(switchState*10ms)
            effectPath = effectPath * GAIN_3DB;
            double firstSum = add(mainPathLeft, effectPath);
            double effectPath2 = delayProcess(effectPath, &delayN1_3ms[0]); // (switchState[0]+1)*3ms
            effectPath2 = effectPath2 * GAIN_1DB;
            sampleBuffer[0][i] = add(firstSum, effectPath2);

            // CH1
            effectPath = inputRight;
            effectPath = delayProcess(effectPath, &delayChannels[1]); //(switchState * 10ms)
            effectPath = effectPath * GAIN_3DB;
            firstSum = add(mainPathRight, effectPath);
            effectPath2 = delayProcess(effectPath, &delayN1_3ms[1]); // (switchState[1]+1)*3ms
            effectPath2 = effectPath2 * GAIN_1DB;
            sampleBuffer[1][i] = add(firstSum, effectPath2);

        }
    }
}