
#ifndef HAOS_API_H
#define HAOS_API_H

#include <stdint.h>


#define MAX_NUMBER_OF_CORES             3
#define MAX_NUMBER_OF_MODULES_PER_CORE  128


#define MAX_FIFO_CNT    2
#define FIFO0_SIZE 	    2048
#define FIFO1_SIZE 	    2048

#if FIFO0_SIZE > FIFO1_SIZE
#define MAX_FIFO_SIZE FIFO0_SIZE
#else
#define MAX_FIFO_SIZE FIFO1_SIZE
#endif


#define __fg_primitive_call
#define __bg_primitive_call
#define __fg_call
#define __bg_call


////////////////////////////////////////////////////////////////////////////////////////
// Global constants visible to all modules
//
// These constants define system-wide limits and processing parameters,
// and are used by modules to ensure consistency in buffer management,
// channel indexing, and block-based audio processing.
//
////////////////////////////////////////////////////////////////////////////////////////

/// Maximum number of audio channels supported by the system.
///
/// This value defines the upper bound for audio stream channels,
/// including input, output, and internal routing. Modules should not
/// attempt to access channels beyond this limit.
#define MAX_NUM_CHANNELS            32

/// Number of I/O channels allocated per DSP core.
///
/// This defines how many distinct input/output channels are assigned
/// to each core's I/O buffer matrix. Typically equals MAX_NUM_CHANNELS.
#define NUMBER_OF_IO_CHANNELS       MAX_NUM_CHANNELS

/// Size of a single audio processing block ("brick").
///
/// Defines the number of audio samples processed per channel in one pass.
/// Used to manage circular buffers and frame-based signal flow across modules.
#define BRICK_SIZE                  16


// Special flag indicating a disconnected or unassigned I/O source.
//
// This constant is used in I/O routing or remapping configurations to explicitly mark
// that no data source is connected to a given input or output buffer.
#define NO_SOURCE (0x08000000)


/* Define the various types of framework entry points */

/**
 *  @brief a function that is called in the foreground.
 *
 *  Every mct_t function callback, except the preKickStart and
 *  background callbacks, are foreground functions.
 *
 *  @param (none)
 *
 *  @ingroup module_api
 */
typedef void __fg_call HAOS_ForegroundCallback_t();

/**
 *  @brief A pre kickstart function gets called before the KickStart message is received and acted-on.
 *
 *  @param mif_ptr Pointer to the module interface.
 *
 *  @ingroup module_api
 */
typedef void __fg_call HAOS_PrekickCallback_t(void* mif);


/**
 *  @brief a function that is called in the background
 *
 *  A background function is used to perform interruptible, lower priority processing.
 *  Called from an infinite-loop in the main body of the OS code.
 *
 *  @param (none)
 *
 *  @ingroup module_api
 */
typedef void __bg_call HAOS_BackgroundCallback_t();

/**
 *  @brief The Module Call Table (MCT) is a table of framework entry points.
 *
 *  This table contains 8 optional entry points that may be called by
 *  the OS Framework.
 *
 *  @param framework_prekick_entry_point_t* <b>prekick_func_ptr</b>
 *  Unconditional initializations.
 *  Initialize vars whose values don't depend on MCV.
 *  This function gets called before the KickStart message is received and acted-on.
 *  Called only when the OS re-boots.
 *
 *  @param framework_entry_point_t* <b>postkick_func_ptr</b>
 *  Unconditional initializations OR conditional initializations.
 *  This function gets called immediately after the KickStart message is received and acted-on (i.e., prior to any other Framework Entry-point).
 *  This function also gets called upon App-Restart (which happens after recovering from an audio under-flow).
 *  This function gets called before any block-processing functions are called.
 *  An example of the kind of initialization that must happen here would be any filter state that must be cleared
 *  prior to starting audio again.
 *
 *  @param framework_entry_point_t* <b>timer_func_ptr</b>
 *   Foreground function called less frequently than frame or block.
 *   Called from the Block ISR (a.k.a., Brick ISR or Foreground ISR)
 *   Called only when the Timer flag is set (gets set by the Timer ISR)
 *   Use this to act on MCV changes when the block and frame functions are not being called
 *
 *  @param framework_entry_point_t* <b>frame_func_ptr</b>
 *   called on input frame boundaries..
 *   Called from the Block ISR (a.k.a., Brick ISR or Foreground ISR).
 *   This function gets called on frame boundaries (integer multiples of sample-blocks)
 *     -#:  AAC Frame-size is 1024 samples.
 *     -#:  Our PCM frame-size was chosen to be 256 samples.
 *   Use this as another place to act on MCV changes.  This is called more often than
 *   the Timer functions, but is only called when the DAO clocks are active (only an issue
 *   with Decoder chips).
 *
 *  @param framework_entry_point_t* <b>block_func_ptr</b>
 *   Called once for each brick (16 samples) of PCM input.
 *   Called from the Block ISR (a.k.a., Brick ISR or Foreground ISR) whenever there is one or more
 *   blocks of unprocessed PCM in the IO Buffer.
 *   This function can process only one block of any channel, but all channels are available.
 *   This function is inherently an in-place processor.
 *   There is an array of I/O buffer pointers, located at YMEM addresses 0x60 - 0x6F.
 *   These pointers will point at block N for all channels.
 *
 *  @param 	void* <b>reserved1</b>
 *
 *  @param bg_framework_entry_point_t* <b>background_func_ptr</b>
 *  Perform interruptible, lower priority processing.
 *  Called from an infinite-loop in the main body of the OS code.
 *  Can be interrupted, so exercise caution when using global vars that are used in the foreground ISR.
 *
 *  @param framework_entry_point_t* <b>postmalloc_func_ptr</b>
 *  Called after successful malloc by all modules in system.
 *  Called from the Block ISR (a.k.a., Brick ISR or Foreground ISR).
 *  Called if the mallocations requested by the modules' Pre-Mallocs succeeds.
 *  This gives the module designer a chance to act upon the newly acquired buffers
 *  if so desired.  For example, a malloced FIR-filter history-buffer should be
 *  cleared here.
 *  This function is also a good place to set a malloc-success flag for the benefit
 *  of the Block function.  That is, the Block function should never try to operate on
 *  a buffer that has not yet been malloc'ed.
 *
 *  @param framework_entry_point_t* <b>premalloc_func_ptr</b>
 *   Request memory from the heap.
 *   Called from the Block ISR (a.k.a., Brick ISR or Foreground ISR).
 *   Called immediately after the frame functions are called and immediately prior to the PostMalloc functions.
 *   This function is only called if one or more modules set the X_VX_NextFrm_Reinit_Req flag (in the Frame function).
 *   When this happens, all modules' PreMalloc functions get called so that each module has the chance
 *   to ask for heap memory.
 *   If this function gets called, the OS has already freed all of the heap, and all heap-using modules
 *   MUST request heap again.
 *   There are 6 types of malloc requests: X, Y, L modulo and non-modulo.
 *   Mallocation is all or nothing.  If there is not enough available heap to accommodate
 *   the entire set of requests, the systems halts.
 *   If the Malloc succeeds, the OS calls the Post Malloc functions.
 *
 *  @ingroup module_api
 */
typedef struct {
    HAOS_PrekickCallback_t*    Prekick;
    HAOS_ForegroundCallback_t* Postkick;
    HAOS_ForegroundCallback_t* Timer;
    HAOS_ForegroundCallback_t* Frame;
    HAOS_ForegroundCallback_t* Brick;
    HAOS_ForegroundCallback_t* AFAP;
    HAOS_BackgroundCallback_t* Background;
    HAOS_ForegroundCallback_t* Postmalloc;
    HAOS_ForegroundCallback_t* Premalloc;
} HAOS_Mct_t, * pHAOS_Mct_t;


// Bitfield containing control flags that manage HaOS execution.
// Each bit represents a specific control state or request.
// These flags are used to signal module-specific conditions or system-wide states.
typedef uint32_t HAOS_CtrlFlags_t;

// Control flags used by Sistem Memory Manager (SMM).
#define MMA_DECODING_STARTED_FLAG		BIT_00_SET	/* Indicates that decoder allocated memory and processed at least one frame.*/
#define MMA_DECODING_STARTED_CLR		BIT_00_CLR	/* Clears the decoding started flag */
#define MMA_FORCE_REALLOC_FLAG			BIT_01_SET	/* Indicates that memory reallocation is required */
#define MMA_FORCE_REALLOC_CLR			BIT_01_CLR	/* Clears the force reallocation flag

/**
 * @brief Enumeration of supported memory regions in the HaOS memory model.
 *
 * This enum defines the memory regions available for allocation by the HaOS memory manager
 * for use by the individual modules. Each region corresponds to a specific physical memory class.
 *
 * The regions are:
 * - TCM_CORE0:       Tightly Coupled Memory (TCM) for Core 0
 * - TCM_CORE1:       Tightly Coupled Memory (TCM) for Core 1
 * - TCM_CORE2:       Tightly Coupled Memory (TCM) for Core 2
 * - SRAM:            Shared Static RAM (SRAM) for all cores
 * - EXTMEM:          External memory accessible by all cores
 *
 * @ingroup memory_management
 */
enum MEM_REGION
{
    TCM_CORE0 = 0,            // Core 0 TCM memory region
    TCM_CORE1,                // Core 1 TCM memory region
    TCM_CORE2,                // Core 2 TCM memory region
    SRAM,                     // Shared SRAM memory region
    EXTMEM,                   // External memory region
    NUMBER_OF_MEM_REGIONS     // Total number of memory regions defined in the system
};

/**
 * @brief Enumeration of memory types used in the HaOS memory model.
 *
 * This enum defines the types of memory used by modules in the HaOS system.
 * Each type corresponds to a specific memory allocation strategy:
 * - PERSISTENT:    Memory holds data that needs to remain valid between two executions
 * of the same module — for example, state variables, buffer positions, or history-dependent
 * values.
 * - TEMPORARY:     Memory used for intermediate computations that can be safely
 * overwritten on each new processing call, such as temporary buffers or scratch space.
 *
 * @ingroup memory_management
 */
enum MEM_TYPE
{
    PERSISTENT = 0,          // Persistent memory type
    TEMPORARY,               // Temporary memory type
    NUMBER_OF_MEM_TYPES      // Total number of memory types defined in the system
};

#define HAOS_SMM_SECTION_START_BIT      BIT_16_SET

#define HAOS_SMM_TCM_CORE0_PERSISTENT   (HAOS_SMM_SECTION_START_BIT << (TCM_CORE0 * NUMBER_OF_MEM_TYPES + PERSISTENT))
#define HAOS_SMM_TCM_CORE0_TEMPORARY    (HAOS_SMM_SECTION_START_BIT << (TCM_CORE0 * NUMBER_OF_MEM_TYPES + TEMPORARY))
#define HAOS_SMM_TCM_CORE1_PERSISTENT   (HAOS_SMM_SECTION_START_BIT << (TCM_CORE1* NUMBER_OF_MEM_TYPES + PERSISTENT))
#define HAOS_SMM_TCM_CORE1_TEMPORARY    (HAOS_SMM_SECTION_START_BIT << (TCM_CORE1 * NUMBER_OF_MEM_TYPES + TEMPORARY))
#define HAOS_SMM_TCM_CORE2_PERSISTENT   (HAOS_SMM_SECTION_START_BIT << (TCM_CORE2 * NUMBER_OF_MEM_TYPES + PERSISTENT))
#define HAOS_SMM_TCM_CORE2_TEMPORARY    (HAOS_SMM_SECTION_START_BIT << (TCM_CORE2 * NUMBER_OF_MEM_TYPES + TEMPORARY))
#define HAOS_SMM_SRAM_PERSISTENT        (HAOS_SMM_SECTION_START_BIT << (SRAM * NUMBER_OF_MEM_TYPES + PERSISTENT))
#define HAOS_SMM_SRAM_TEMPORARY         (HAOS_SMM_SECTION_START_BIT << (SRAM * NUMBER_OF_MEM_TYPES + TEMPORARY))
#define HAOS_SMM_EXTMEM_PERSISTENT      (HAOS_SMM_SECTION_START_BIT << (EXTMEM * NUMBER_OF_MEM_TYPES + PERSISTENT))
#define HAOS_SMM_EXTMEM_TEMPORARY       (HAOS_SMM_SECTION_START_BIT << (EXTMEM * NUMBER_OF_MEM_TYPES + TEMPORARY))

#define HAOS_SMM_SECTION_START_MASK     (HAOS_SMM_TCM_CORE0_PERSISTENT   | HAOS_SMM_TCM_CORE0_TEMPORARY    | \
                                         HAOS_SMM_TCM_CORE1_PERSISTENT   | HAOS_SMM_TCM_CORE1_TEMPORARY    | \
                                         HAOS_SMM_TCM_CORE2_PERSISTENT   | HAOS_SMM_TCM_CORE2_TEMPORARY    | \
                                         HAOS_SMM_SRAM_PERSISTENT        | HAOS_SMM_SRAM_TEMPORARY         | \
                                         HAOS_SMM_EXTMEM_PERSISTENT      | HAOS_SMM_EXTMEM_TEMPORARY )


/**
 * @brief Memory allocation entry structure used by the HaOS memory manager and all modules.
 *
 * This structure defines a single memory block, including its size (in bytes)
 * and a pointer to the allocated region. It is used internally by the memory manager to
 * describe both persistent and temporary memory areas assigned to a module.
 * The pointer is guaranteed to be aligned to 8 bytes, ensuring efficient memory access.
 *
 * @ingroup memory_management
 */
typedef struct
{
    uint32_t size; // Size of the memory block in bytes
    uint8_t* ptr; // Pointer to the allocated memory aligned to 8 bytes
} HAOS_Mma_Entry_t, * pHAOS_Mma_Entry_t;

/**
 * @brief Structure representing the memory allocation map for a module.
 *
 * This structure tracks the allocation status and descriptors for all memory regions
 * assigned to a module. It includes control flags for memory allocation states
 * as well as an array of memory descriptors indexed by NUMBER_OF_MEM_REGIONS and NUMBER_OF_MEM_TYPES.
 *
 * - memDescTable: Array of memory descriptors for each memory region and type.
 * - ctrlFlags: Control flags for memory allocation state.
 *
 * Example usage:
 *   mma.memDescTable[TCM_CORE0][PERSISTENT].ptr; provides access to Core 0's TCM persistent memory.
 *
 * @ingroup memory_management
 */
typedef struct
{
    HAOS_CtrlFlags_t ctrlFlags; // Control flags for memory allocation state
    HAOS_Mma_Entry_t memDescTable[NUMBER_OF_MEM_REGIONS][NUMBER_OF_MEM_TYPES];
} HAOS_Mma_t, * pHAOS_Mma_t;

/**
 *  @brief Structure pointed to by the ODT, which points to the MCV and MCT.
 *
 *  @ingroup module_api
 */
typedef struct
{
    void*       MCV;
    pHAOS_Mct_t MCT;
    void* reserved1;
    void* reserved2;
    void* reserved3;
    void* reserved4;
    void* reserved5;
    void* reserved6;
} HAOS_Mif_t, * pHAOS_Mif_t;


// Represents a single PCM audio sample in the I/O buffer.
// Typically 32-bit signed integer in fixed-point systems.
typedef double HAOS_PcmSample_t;

// Pointer to a PCM audio sample in the I/O buffer.
typedef HAOS_PcmSample_t* HAOS_PcmSamplePtr_t;

// Represents one block (brick) of audio data for a single channel.
// A brick contains BRICK_SIZE number of consecutive samples.
typedef HAOS_PcmSample_t HAOS_BrickBuffer_t[BRICK_SIZE];

// Bitmask representing a set of active audio channels.
// Each bit corresponds to one logical I/O channel.
typedef uint32_t HAOS_ChannelMask_t;


// -----------------------------------------------------------------------------
// Double Buffering Support
//
// In this implementation, a double-buffering strategy is employed to separate
// I/O operations from audio processing. Specifically, the Brick routine performs
// input/output buffer data copying between shared HaOS buffers and local memory,
// while the Background routine processes audio samples stored in the local
// double buffer.
//
// This approach ensures that even if the Background routine fails to complete
// within the available processing window, only the current frame will be affected,
// thereby improving overall system robustness and responsiveness.
// -----------------------------------------------------------------------------

// Structure defining the double buffer control state
typedef struct
{
    HAOS_PcmSamplePtr_t readPtr;     // Pointer to current read position (for background processing)
    HAOS_PcmSamplePtr_t writePtr;    // Pointer to current write position (from I/O buffer in block routine)
    HAOS_PcmSamplePtr_t beginPtr;    // Start of the buffer
    HAOS_PcmSamplePtr_t endPtr;      // End of the buffer

    int32_t samplesCnt;     // Total number of samples currently stored
} HAOS_dblBuffCtrl_t, * pHAOS_dblBuffCtrl_t;
// -----------------------------------------------------------------------------

// Enum indicating the decoding format of the input audio stream.
enum DECODE_INFO
{
    DECODE_INFO_UNKNOWN = 0,                        // Format not specified or unsupported
    DECODE_INFO_PCM = 1,                            // Uncompressed PCM audio
    DECODE_INFO_MP3 = 2                             // Compressed MP3 audio
};

// Describes per-frame metadata required for processing.
// Includes sample rate, input/output channel masks, and decode type.
typedef struct
{
    HAOS_ChannelMask_t inputChannelMask;            // Bitmask of active input channels
    HAOS_ChannelMask_t outputChannelMask;           // Bitmask of active output channels
    int32_t sampleRate;                                 // Sampling rate of the current frame (e.g., 48000 Hz)
    DECODE_INFO decodeInfo;                         // Decoding format (e.g., PCM or MP3)
} HAOS_FrameData_t, * pHAOS_FrameData_t;


// Struct used when copying decoded samples into the system I/O buffer.
// Contains a pointer to current frame metadata and per-channel destination pointers.
typedef struct
{
    pHAOS_FrameData_t frameData;                                // Pointer to current frame's metadata, nullptr if not start of frame
    HAOS_PcmSamplePtr_t IOBufferPtrs[NUMBER_OF_IO_CHANNELS];    // Array of decoded data brick pointers, one per output channel
} HAOS_CopyToIOPtrs_t, * pHAOS_CopyToIOPtrs_t;
// -----------------------------------------------------------------------------

#define ALL_BITS_CLR    0x00000000
#define ALL_BITS_SET    0xffffffff
#define BIT_00_SET      0x00000001
#define BIT_01_SET      0x00000002
#define BIT_02_SET      0x00000004
#define BIT_03_SET      0x00000008
#define BIT_04_SET     	0x00000010
#define BIT_05_SET      0x00000020
#define BIT_06_SET      0x00000040
#define BIT_07_SET      0x00000080
#define BIT_08_SET      0x00000100
#define BIT_09_SET      0x00000200
#define BIT_10_SET      0x00000400
#define BIT_11_SET      0x00000800
#define BIT_12_SET      0x00001000
#define BIT_13_SET      0x00002000
#define BIT_14_SET      0x00004000
#define BIT_15_SET      0x00008000
#define BIT_16_SET      0x00010000
#define BIT_17_SET      0x00020000
#define BIT_18_SET      0x00040000
#define BIT_19_SET      0x00080000
#define BIT_20_SET      0x00100000
#define BIT_21_SET      0x00200000
#define BIT_22_SET      0x00400000
#define BIT_23_SET      0x00800000
#define BIT_24_SET      0x01000000
#define BIT_25_SET      0x02000000
#define BIT_26_SET      0x04000000
#define BIT_27_SET      0x08000000
#define BIT_28_SET      0x10000000
#define BIT_29_SET      0x20000000
#define BIT_30_SET      0x40000000
#define BIT_31_SET      0x80000000


#define BIT_00_CLR      (~BIT_00_SET)
#define BIT_01_CLR      (~BIT_01_SET)
#define BIT_02_CLR      (~BIT_02_SET)
#define BIT_03_CLR      (~BIT_03_SET)
#define BIT_04_CLR     	(~BIT_04_SET)
#define BIT_05_CLR      (~BIT_05_SET)
#define BIT_06_CLR      (~BIT_06_SET)
#define BIT_07_CLR      (~BIT_07_SET)
#define BIT_08_CLR      (~BIT_08_SET)
#define BIT_09_CLR      (~BIT_09_SET)
#define BIT_10_CLR      (~BIT_10_SET)
#define BIT_11_CLR      (~BIT_11_SET)
#define BIT_12_CLR      (~BIT_12_SET)
#define BIT_13_CLR      (~BIT_13_SET)
#define BIT_14_CLR      (~BIT_14_SET)
#define BIT_15_CLR      (~BIT_15_SET)
#define BIT_16_CLR      (~BIT_16_SET)
#define BIT_17_CLR      (~BIT_17_SET)
#define BIT_18_CLR      (~BIT_18_SET)
#define BIT_19_CLR      (~BIT_19_SET)
#define BIT_20_CLR      (~BIT_20_SET)
#define BIT_21_CLR      (~BIT_21_SET)
#define BIT_22_CLR      (~BIT_22_SET)
#define BIT_23_CLR      (~BIT_23_SET)
#define BIT_24_CLR      (~BIT_24_SET)
#define BIT_25_CLR      (~BIT_25_SET)
#define BIT_26_CLR      (~BIT_26_SET)
#define BIT_27_CLR      (~BIT_27_SET)
#define BIT_28_CLR      (~BIT_28_SET)
#define BIT_29_CLR      (~BIT_29_SET)
#define BIT_30_CLR      (~BIT_30_SET)
#define BIT_31_CLR      (~BIT_31_SET)

typedef struct
{
    uint32_t  	ID;
    uint32_t*   startAddr;
    uint32_t 	size;
    uint32_t	reserved[5];
} BitRipperFIFO_t, * pBitRipperFIFO_t;


typedef struct
{
    uint32_t  	currentWord;
    uint32_t  	bitsRemaining;
    uint32_t*   readPtr;
    uint32_t*   endAddrPlus1;
    uint32_t*   baseAddr;
    int32_t     size;
    uint32_t	reserved;
    uint32_t**  pWritePtr;
} BitRipper_t, * pBitRipper_t;

typedef struct
{
    BitRipper_t currState;     			// current state
    BitRipper_t mainStateBackup;    	// backup of main state

    HAOS_CtrlFlags_t ctrlFlags;         // Bitmask of control flags used to manage the bitripper state.

    uint32_t alignmentInfo;             // BitsRemaining at alignment
    uint32_t overflowCntFIFO;          // overflow counter

    uint32_t* writePtr;

} BitRipperState_t, * pBitRipperState_t;

extern const HAOS_PcmSample_t SAMPLE_SCALE;

namespace HAOS
{

    ////////////////////////////////////////////////////////////////////////////////////////
    // Demo application callback functions
    ////////////////////////////////////////////////////////////////////////////////////////

    // Initializes the emulation environment using command-line arguments.
    //
    // This function should be called once at the beginning of the `main()` function
    // to configure the runtime environment (e.g., logging, tracing, test mode, etc.).
    //
    // @param argc Argument count from the command line.
    // @param argv Argument vector from the command line.
    void init(int argc, const char* argv[]);

    // Registers a list of modules (algorithms) to be managed by the OS.
    //
    // This function can be called multiple times to add different sets of modules.
    // Each entry in `moduleList` is a pointer to a module interface (MIF) that the OS
    // will manage and schedule during execution.
    //
    // @param moduleList An array of module interface pointers (platform-specific format).
    void addModules(void* moduleList[]);

    // Starts execution of the emulated OS.
    //
    // Once all required modules have been registered via `add_modules()`, this function
    // starts the scheduling and execution of modules as defined by the OS kernel logic.
    void run();



    const char* get_toml_file_name();

    ////////////////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////////////////////////
    // Core context accessor functions for modules
    //
    // These functions allow modules to access core-specific runtime data
    // such as I/O buffers, frame state, masks, BitRipper context, etc.
    // They provide abstraction and isolation from the global system state,
    // enabling reusability of modules across cores.
    //
    ////////////////////////////////////////////////////////////////////////////////////////


    // Returns a pointer to the active input buffer for a given I/O channel.
    // This buffer is owned by the current core executing the module.
    //
    // @param channelIndex Index of the input channel [0 .. NUMBER_OF_IO_CHANNELS-1]
    // @return Pointer to the circular audio buffer for that input channel.
    HAOS_PcmSamplePtr_t* getIOChannelPointerTable();

    // @brief Returns the current channel mask used for post-processing.
    //
    // Each bit in the returned mask indicates whether the corresponding
    // audio channel (bit 0 = ch0, ..., bit 31 = ch31) is active.
    //
    // This is typically used by modules to determine
    // which channels should be processed in the current frame.
    //
    // @return HAOS_ChannelMask_t Bitmask of active channels (1 bit per channel).
    HAOS_ChannelMask_t getValidChannelMask();

    // @brief Sets the channel mask for post-processing.
    //
    // Updates the global or core-local bitmask that defines which audio channels
    // are valid for processing. Each bit enables (1) or disables (0) a channel.
    //
    // @param mask Bitmask specifying which channels are active (1 bit per channel).
    void setValidChannelMask(HAOS_ChannelMask_t);

    // @brief Returns the amount of unused space in the I/O buffer for the current core.
    //
    // This function retrieves the number of available (free) PCM samples in the I/O buffer
    // for the currently active DSP core. It is typically used by decoder modules to check
    // whether there is enough space to write new audio data without overwriting unread content.
    //
    // @return Number of free sample slots in the I/O buffer (in PCM samples).
    int32_t getIOUnusedSpace();


    //@brief Returns the end-of-stream status of the input stream.
    //
    //This function checks whether the input stream has reached its end,
    //typically due to end-of-file (EOF) in file-based input or termination
    //of a real-time data source. It allows processing modules to handle
    //graceful shutdown or transition when no more input data is available.
    //
    //@return true if the input stream is at EOF, false otherwise.
    bool getInputStreamEOF();

    // @brief Sets the end-of-stream (EOF) status for the input stream.
    //
    // This function allows external components (e.g., simulation framework or I/O handlers)
    // to update the EOF flag indicating whether the input stream has reached its end.
    // Typically used to signal the end of available input data during runtime.
    //
    // @param eofStatus Set to true to indicate end-of-stream; false to reset the flag.
    void setInputStreamEOF(bool);


    //@brief Returns the end-of-processing status of the system.
    //
    //This function checks whether all input data has been fully processed.
    //The condition is met when the end-of-file (EOF) has been reached in the
    //input stream and the system flush counter has decremented to zero,
    //indicating that all buffered or pending data has been processed.
    //
    //@return true if EOF is reached and all pending data has been flushed, false otherwise.
    bool getEndOfProcessing();


    // @brief Returns the sample rate (number of samples per second) of the input stream.
    //
    // This function provides the current input stream sampling frequency (FS),
    // in Hz (e.g., 48000 for 48 kHz audio). It can be used by modules or system
    // components to adapt processing parameters based on the input rate.
    //
    // @return The sampling frequency of the input stream in Hz.
    int32_t getInputStreamFS();

    // @brief Returns the number of audio channels in the input stream.
    //
    // This function retrieves the number of audio channels present in the input stream,
    // such as 1 for mono or 2 for stereo. It can be used by modules or system components
    // to adjust buffer allocation, processing logic, or channel-specific operations.
    //
    // @return The number of channels in the input stream.
    int32_t getInputStreamChCnt();

    // @brief Sets the number of audio channels for the output stream.
    //
    // This function allows system components or configuration routines to specify
    // how many audio channels the output stream should use (e.g., 1 for mono, 2 for stereo).
    // This information is typically used for output buffer sizing, file formatting,
    // or hardware interface setup.
    //
    // @param chCount The desired number of output audio channels.
    void setOutputStreamChCnt(int32_t);

    // @brief Copies decoded audio samples into the system I/O buffer.
    //
    // This function transfers a block (brick) of decoded PCM samples—produced by the decoder—
    // into the corresponding channel locations in the I/O buffer of the active core.
    // It is typically called after each decoding step to prepare audio data for further
    // processing stages within the audio pipeline.
    //
    // @param copyToIOPtrs Pointer to a structure containing source sample data and
    //                     target I/O buffer channel and offset information.
    void copyBrickToIO(pHAOS_CopyToIOPtrs_t copyToIOPtrs);

    // @brief Refills the input bitstream FIFO buffer from the input file.
    //
    // This function is called when the input FIFO—used by the decoder to read compressed audio data—
    // becomes empty. It reads a new chunk of data from the input file and appends it to the FIFO,
    // ensuring continuous decoding without underruns. Typically used in the simulation environment
    // to maintain a steady input flow.
    //
    // @note If the end of the input stream is reached, no data is added and EOF status may be set.
    void fillInputFIFO();

    // @brief Returns a pointer to the BitRipper instance of the active core.
    //
    // This function provides access to the BitRipper state structure associated with
    // the currently active DSP core. BitRipper is responsible for managing the input
    // bitstream, including bit-level extraction and FIFO operations. This pointer can
    // be used by modules or system functions that require direct access to bitstream
    // manipulation functionality.
    //
    // @return Void pointer to the BitRipper instance of the active core.
    void* getActiveCoreBitRipper();
    void* getActiveCore();

    // @brief Requests memory allocation from the system.
    //
    // This function sets the system memory allocation request flag and optionally
    // clears the first frame received status. The first frame received flag can only
    // be cleared by the decorder module in case of new memory allocation request.
    //
    // @param clearFirstFrameRecieved If true, sets the firstFrameRecieved flag to false,
    //                                 otherwise sets it to true.
    void requestMemoryAllocation(bool);

    // @brief Returns the current system frame counter.
    //
    // This function provides access to the global frame counter maintained by the
    // HaOS system. The frame counter is incremented each time a new frame is processed
    // and can be used by modules to track frame-based timing and trigger reallocation
    // or other frame-dependent operations.
    //
    // @return Current frame counter value.
    uint32_t getFrameCounter();

    void setCompressedInputStream(bool value);
    bool getCompressedInputStream();


    void setInputStreamFS(int fs);

    void set_fg2bg_ratio(int fg2bgRatio);

    bool isActiveChannel(int32_t chIdx);

} /* end of haos namespace */



namespace Core
{
    /* Initializes a FIFO descriptor for given ID, binding it to a custom buffer address and size. */
    void initFIFO(uint32_t id, uint32_t* addr, int32_t size);

    /* Initializes the BitRipper state for the specified FIFO ID in the active HaOS core. */
    void initBitripper(uint32_t id);

    /* Switches the active BitRipper instance in the active HaOS core to use the FIFO identified by the given ID. */
    void switchBitripperFIFO(uint32_t id);

} /* end of haos_core namespace */


#endif // HAOS_API_H
