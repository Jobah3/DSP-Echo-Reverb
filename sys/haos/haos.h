/**
 * @file
 * @brief DSP OS support.
 *
 * Home Audio System Kernel V0.0.1
 *
 *
 */
#ifndef HAOS_H__
#define HAOS_H__

#include "stdint.h"
#include <string>
#include "wavefile.h"
#include "haos_api.h"
#include "haos_config.h"

 /* Number of bits per sample */
#define OUTPUT_HANDLER_BITS_PER_SAMPLE_DFLT     16

/* Number of dummy frames to process after EOF is detected in the input stream */
#define HAOS_FLUSH_FRAMES_CNT_DFLT     10


// Bitmask definitions for control flags used in the system.
#define HAOS_STREAM_FIRST_OPEN_FLAG			BIT_00_SET			// Indicates whether the input file is being opened for the first time
#define HAOS_STREAM_FIRST_OPEN_CLR			BIT_00_CLR
#define HAOS_STREAM_END_OF_FILE_FLAG		BIT_01_SET			// Indicates whether the end of the stream has been reached
#define HAOS_STREAM_END_OF_FILE_CLR			BIT_01_CLR
#define HAOS_STREAM_COMMPRESSED_FLAG		BIT_02_SET			// Indicates whether the stream is compressed bitStream
#define HAOS_STREAM_COMMPRESSED_CLR			BIT_02_CLR
#define HAOS_STREAM_ROUNDING_FLAG			BIT_03_SET			// Indicates whether the PCM samples should be rounded
#define HAOS_STREAM_ROUNDING_CLR			BIT_03_CLR



// Bitmask definitions for control flags used in the system.
#define HAOS_CLEAR_ALL_FLAGS				ALL_BITS_CLR
#define HAOS_SYS_MEM_ALLOC_REQUESTED_FLAG	BIT_00_SET	// Indicates a memory allocation request
#define HAOS_SYS_MEM_ALLOC_REQUESTED_CLR	BIT_00_CLR	// Clears the memory allocation request flag
#define HAOS_FRAME_TRIGGERED_FLAG			BIT_01_SET	// Indicates that a new frame has been triggered
#define HAOS_FRAME_TRIGGERED_CLR			BIT_01_CLR	// Clears the frame triggered flag
#define HAOS_DECODING_STARTED_FLAG			BIT_02_SET	// Indicates that the first frame has been received
#define HAOS_DECODING_STARTED_CLR			BIT_02_CLR	// Clears the first frame received flag

enum HAOS_ROUTINE
{
	PREKICK,
	POSTKICK,
	TIMER,
	FRAME,
	BRICK,
	AFAP,
	BACKGROUND,
	POSTMALLOC,
	PREMALLOC
};

// Represents a single entry in the Overlay Definition Table (ODT).
//
// Each ODT entry describes one audio processing module assigned to a core.
// It includes a pointer to the module's Module Interface Structure (MIF),
// and the module's unique identifier.
//
// These entries are used during system initialization to build the per-core
// module execution lists (pipelines).
typedef struct
{
	HAOS_Mif_t* MIF;
	uint32_t 	moduleID;
} HAOS_OdtEntry_t, * pHAOS_OdtEntry_t;


//  Defines a type alias for an array of ODT (Overlay Definition Table) entries.
//
// This represents a complete ODT table for one core, listing all modules assigned to it.
// Each element in the array is a `HAOS_OdtEntry_t` structure, which contains a pointer
// to a module's interface and its unique identifier.
//
// The array should be terminated with a special null entry (e.g., {nullptr, 0}) to mark the end.
typedef HAOS_OdtEntry_t HAOS_Odt_t[];


// type used to represent a sequence of incoming host comm messages
typedef HAOS_OdtEntry_t HAOS_ModuleTable_t[MAX_MODULES_COUNT];


/**
 * @brief Structure representing a single processing core in the HAOS system.
 *
 * This structure encapsulates all per-core data, including core identification,
 * module ODT, and audio I/O buffer management.
 */
typedef struct
{
	// Unique identifier for this core
	uint32_t coreID;

	// List of modules assigned to this core, representing the core's ODT (Overlay Definition Table)
	HAOS_ModuleTable_t  moduleMIFs;

	/* Number of registered modules in the core ODT table s*/
	uint32_t modulesCnt;

	// Bitmask of valid I/O channels for post-processing (PPM).
	// Each bit represents one audio channel (bit 0 = ch0, ..., bit 31 = ch31).
	// A bit value of 1 means the channel is enabled and should be processed.
	// Used by modules to skip inactive channels and improve performance.
	HAOS_ChannelMask_t HAOS_PPM_VALID_CHANNELS;

	// Global I/O buffer used by all modules in this core.
	// Each channel has a circular array of buffers used for block-based audio processing.
	//HAOS_BrickBuffer_t IOBUFFER[NUMBER_OF_IO_CHANNELS][IO_BUFFER_PER_CHAN_MODULO];
	HAOS_BrickBuffer_t(*IOBUFFER)[IO_BUFFER_PER_CHAN_MODULO];

	// Internal input pointers for each I/O channel, used for accessing the active input buffer
	HAOS_PcmSamplePtr_t HAOS_IOBUFFER_INP_PTRS[NUMBER_OF_IO_CHANNELS];

	// Internal output pointers for each I/O channel, used for accessing the active output buffer
	HAOS_PcmSamplePtr_t HAOS_IOBUFFER_PTRS[NUMBER_OF_IO_CHANNELS];

	// Amount of free space in the I/O buffer for a single channel, expressed in PCM samples.
	int32_t IOfree;


	// BitRipper state and FIFO buffer specific to the core
	BitRipperFIFO_t  	FIFODescArray[MAX_FIFO_CNT];
	BitRipperState_t  	bitRipperArray[MAX_FIFO_CNT];
	pBitRipperState_t 	pBitRipper;

}HAOS_Core_t, * pHAOS_Core_t;

// List of cores used in current concurrency
//typedef std::list<HAOS_Core_t> HAOS_CoreTable_t;
typedef HAOS_Core_t HAOS_CoreTable_t[MAX_CORES_COUNT];

// List used for host communication events, storing host commands
//typedef std::list<unsigned int> HAOS_HostCommQ_t;
typedef uint32_t HAOS_HostCommQ_t[MAX_HOST_COMMANDS_COUNT];

// @brief Structure representing a single input or output audio stream in the simulation environment.
//
// This structure holds all relevant metadata and state flags required for handling
// audio file I/O within the haOS simulation framework. It is typically used by modules
// and system components for managing access to WAV files and tracking stream progress.
typedef struct
{
	// Bitmask of control flags used to manage the stream state.
	HAOS_CtrlFlags_t ctrlFlags;

	// Full file path of the audio stream (e.g., WAV input or output file)
	std::string filePath;

	// Handle to the opened audio file (abstracted via WAVREAD_HANDLE)
	WAVREAD_HANDLE* fileHandle;

	// Type of decoder associated with this stream (e.g., PCM, MP3, AAC)
	DECODE_INFO type;

	// Sampling frequency of the stream, in Hz (e.g., 48000 for 48 kHz)
	uint32_t samplingFrequency;

	// Number of audio channels (e.g., 1 for mono, 2 for stereo)
	uint32_t channelCount;

	// Number of bits per audio sample (e.g., 16, 24)
	uint32_t bitsPerSample;

	// Total number of decoded PCM samples per channel so far
	uint32_t chSamplesCnt;

	// Total number of PCM samples in the file (all channels)
	uint32_t fileSamplesCnt;

}HAOS_Stream_t, * pHAOS_Stream_t;


// @brief Global system context structure for the HAOS runtime environment.
//
// This structure encapsulates all system-level state and configuration data required
// for managing concurrency, audio I/O, core-level processing, and stream synchronization.
// It serves as the central context shared by all processing routines and modules.
typedef struct
{
	// Bitmask of control flags used to manage the system state.
	HAOS_CtrlFlags_t ctrlFlags;

	// Number of DSP cores currently active (determined by configuration or command-line)
	int32_t coresNumber;

	// Table containing the list of all core instances in the system
	HAOS_CoreTable_t coreTable;

	// Pointer to the currently active DSP core
	pHAOS_Core_t pActiveCore;

	// Pointer to statically allocated system I/O buffers
	int32_t* systemIObuffers;

	// Pointer to statically allocated system FIFO buffers (used by decoders/BitRipper)
	uint32_t* systemFIFObuffers;

	// Input read brick index (used to wrap the read position in the circular I/O buffer)
	int32_t readBrickCnt;

	// Output write brick index (used to wrap the write position in the circular I/O buffer)
	int32_t writeBrickCnt;

	// Flag used to mark the start of a new frame (e.g., for synchronization or logging)
	// bool frameTriggered;

	// Flag indicating a module-initiated request for system memory (re)allocation
	// bool sysMemAllocRequested;

	// Flag indicating whether the first frame has been finished by the decoder
	// bool firstFrameReceived;

	// Structure representing the input audio stream (file path, handle, format, etc.)
	HAOS_Stream_t inStream;

	// Structure representing the output audio stream
	HAOS_Stream_t outStream;

	// Per-frame audio metadata and channel mask information
	HAOS_FrameData_t frameData;

	// Path to the configuration file used during the prekick stage (e.g., gain.cfg)
	std::string cfgPath;


	const char* tomlPath;

	// Foreground-to-background processing ratio (e.g., 16 for PCM, 72 for MP3)
	// Defines how many bricks are processed in the background per frame
	int fg2bg_ratio;

	/* Frame counter, incremented each time frameTriggered flag is set */
	uint32_t frameCounter;

	/* Flush counter, decremented after each processed frame once EOF is detected */
	uint32_t flushDataCnt;

} HAOS_System_t, * pHAOS_System_t;

#endif /* HAOS_H__ */
