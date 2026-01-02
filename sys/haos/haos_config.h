/**
 * @file
 * @brief DSP OS support.
 *
 * Home Audio System Kernel V0.0.1
 *
 *
 */
#ifndef HAOS_CONFIG_H__
#define HAOS_CONFIG_H__


 // Maximum number of DSP cores supported by the system.
 //
 // This constant defines the upper limit for how many core instances
 // the system is capable of managing. Used to allocate static resources
 // and to validate user-defined concurrency settings.
#define MAX_CORES_COUNT 		3

// Actual number of DSP cores to be instantiated for this configuration.
//
// This value determines how many cores will be created and initialized
// at runtime (e.g., in simulation or deployment). It must not exceed MAX_CORES_COUNT.
#define NUMBER_OF_CORES 	1


/*
 * Maximum number of modules per one core that can be registered
 * in HaOS. This constant defines the upper bound for module entries
 * in the Overlay Definition Table (ODT).
 */
#define MAX_MODULES_COUNT 	128


 /* Maximum number of host commands that can be handled by the system */
#define MAX_HOST_COMMANDS_COUNT 	1024


// Number of brick slots allocated per audio channel in the I/O buffer.
// Used to implement circular buffering for block-based processing.
#define IO_BUFFER_PER_CHAN_MODULO   4

// Total number of samples allocated per channel in the I/O buffer.
// Computed as number of bricks per channel multiplied by samples per brick.
#define IO_BUFFER_SIZE_PER_CHAN     BRICK_SIZE * IO_BUFFER_PER_CHAN_MODULO

// Default channel mask for post-processing stage (PPM).
// Bitmask enabling channel 0 and channel 2 (binary 00000101).
#define DEFAULT_PPM_CHANNEL_MASK    5


#endif /* HAOS_CONFIG_H__ */
