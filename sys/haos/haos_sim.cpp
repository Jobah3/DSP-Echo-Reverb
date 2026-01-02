/*
 * haos_sim.c
 *
 *  Created on: Apr 10, 2024
 *      Author: pekez
 */

#include "haos.h"
#include "bitripper_sim.h"
#include "wavefile.h"
#include "colormod.h"
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <cmath>
#include <iostream>

#define VERSION_STRING "0.3.0"

extern HAOS_OdtEntry_t* PcmDecoder_odtPtr;
extern HAOS_OdtEntry_t* AudioManager_odtPtr;

const HAOS_PcmSample_t SAMPLE_SCALE = -(HAOS_PcmSample_t)(1 << 31);

static Color::Modifier yellow(Color::FG_LIGHT_YELLOW);
static Color::Modifier red(Color::FG_LIGHT_RED);
static Color::Modifier def(Color::FG_DEFAULT);
static Color::Modifier cyan(Color::FG_LIGHT_CYAN);
static Color::Modifier grey(Color::FG_DARK_GRAY);

namespace HAOS
{
	// static memory allocation
	static uint32_t sharedInputFIFO[MAX_CORES_COUNT][MAX_FIFO_CNT][MAX_FIFO_SIZE] = { 0 }; // FIFO0 buffer - read input samples
	static int32_t sharedIObuffer[MAX_CORES_COUNT][NUMBER_OF_IO_CHANNELS][IO_BUFFER_PER_CHAN_MODULO][BRICK_SIZE] = { 0 };

	// @brief Global system context instance used by the HAOS runtime.
	//
	// This static instance holds the complete state of the audio processing system,
	// including core configurations, stream management, I/O buffers, and runtime flags.
	//
	static HAOS_System_t haOS;

	static void parseCmdLine(int argc, const char* argv[]);
	static void makeCoresList();
	static void addCoreModules(void* moduleList, pHAOS_Core_t pCore);
	static void initCores();
	static void callAllModules(HAOS_ROUTINE entryPoint);
	static void readPrekickConfigs();
	static void openInputFile();
	static bool openOutputFile();
	static void writeToFile();
	static void flushFrameToFile();
	static void updatePtrs();
	static void initSystemStruct();
	static void usage(const char* programName);
	static std::string trim(std::string const& str);

	//==============================================================================
	//========================== EXTERNAL API FUNCTIONS ============================
	//==============================================================================

	// @brief Initializes the HAOS simulation environment using command-line arguments.
	//
	// This function sets up the global system state at the beginning of the simulation.
	// It parses user-provided arguments (such as number of DSP cores and config file paths),
	// and initializes the list of active core instances based on the requested concurrency.
	//
	// Should be called once at the beginning of main(), before calling add_modules().
	//
	// @param argc Number of command-line arguments.
	// @param argv Array of command-line argument strings.
	void init(int argc, const char* argv[])
	{
		std::cout << cyan;
		std::cout << "---------------Home Audio Operating System (haOS)---------------"<< std::endl;
		std::cout << "Arch: x86 Lightweight Simulation" << std::endl;
		std::cout << "OS: Windows 10/11" << std::endl;
		std::cout << "Version: " << VERSION_STRING << std::endl;
		std::cout << "----------------------------------------------------------------" << std::endl;

		std::cout << yellow;
		std::cout << ">>Booting haOS" << std::endl;
		std::cout << def;
	
		// Initialize global system structure with default values
		initSystemStruct();

		// Parse command-line options provided by the user (e.g., -input, -output, -cfg, -cores)
		parseCmdLine(argc, argv);
	}

	// @brief Assigns module lists (ODT) to each active core in the system.
	//
	// Iterates through the global core table and assigns corresponding module lists
	// from the provided array. Each module list is added to the matching core
	// using the add_core_modules() function. Only non-null module lists are processed.
	//
	// @param moduleLists Array of pointers to ODT tables (one per core).
	//                    Entries may be nullptr if no modules should be assigned to that core.
	void addModules(void* moduleLists[])
	{

		/* Get number of cores from system ODT table */
		haOS.coresNumber = 0;

		for (int coreIndex = 0; coreIndex < MAX_CORES_COUNT; coreIndex++)
		{
			if (moduleLists[coreIndex] != nullptr)
			{
				haOS.coresNumber++;
			}
		}

		/* Create and initialize the list of DSP cores based on parsed configuration */
		makeCoresList();


		/* Iterate through cores and corresponding module lists */
		for (int coreIndex = 0; coreIndex < MAX_CORES_COUNT; coreIndex++)
		{
			/* If a module list is provided for this core, assign it */
			if (moduleLists[coreIndex] != nullptr)
			{
				addCoreModules(moduleLists[coreIndex], &haOS.coreTable[coreIndex]);
			}
		}
	}

	// @brief Starts the main simulation loop.
	//
	// Initializes core runtime variables and executes all registered module
	// entry points in the expected order: PREKICK, POSTKICK, TIMER, FRAME, BRICK,
	// and BACKGROUND. Reads optional configuration files and handles I/O buffers,
	// output file writing, and channel mask updates.
	//
	// This function runs until the end of the input stream.
	// Should be called after init() and add_modules().
	void run()
	{
		std::cout << yellow << ">>Running haOS" << def << std::endl;

		// Initialize I/O buffers, internal pointers, and bitripper states for all cores
		initCores();
		
		// Open output file for writing decoded/processed PCM frames
		openInputFile();

		// Execute pre-initialization entry points of all modules (e.g., set host MCV to default values)
		callAllModules(PREKICK);

		// The MCV is first set to default values; afterwards, readPrekickConfigs updates it with values from the configuration file.

		// Load additional config from .cfg file (if provided) to override defaults
		readPrekickConfigs();

		// Execute post-initialization entry points of all modules
		callAllModules(POSTKICK);

		// Execute time-based initialization (e.g., initial delay, timing sync)
		callAllModules(TIMER);

		// Main loop: runs until end-of-file is detected in the input stream
		while (haOS.flushDataCnt)
		{
			/* If EOF is detected, process two additional dummy frames to flush the remaining data from the system */
			if (haOS.inStream.ctrlFlags & HAOS_STREAM_END_OF_FILE_FLAG)
			{
				haOS.flushDataCnt--;
			}

			// For each BRICK in the frame (depending on fg2bg ratio)
			for (int brick = 0; brick < haOS.fg2bg_ratio; brick++)
			{
				// Execute any asynchronous frame-ahead processing (optional for some modules)
				callAllModules(AFAP);

				// Execute one frame if the frameTriggered flag is set
				if (haOS.ctrlFlags & HAOS_FRAME_TRIGGERED_FLAG)
				{
					haOS.frameCounter++;

					callAllModules(FRAME);
					haOS.ctrlFlags &= HAOS_FRAME_TRIGGERED_CLR;
				}

				// Execute SMM if required

				if (haOS.ctrlFlags & HAOS_SYS_MEM_ALLOC_REQUESTED_FLAG)
				{
					callAllModules(PREMALLOC);

					callAllModules(POSTMALLOC);

					haOS.ctrlFlags &= HAOS_SYS_MEM_ALLOC_REQUESTED_CLR;
				}


				// Execute audio processing BRICK stage across all modules
				callAllModules(BRICK);


				//if (haosSystem.ctrlFlags & HAOS_DECODING_STARTED_FLAG)
				if (haOS.coreTable[0].IOfree < IO_BUFFER_SIZE_PER_CHAN)
				{
					// Write current brick data to output stream/file
					writeToFile();

					//flushFrameToFile();

					// Advance I/O buffer pointers to next brick location
					updatePtrs();
				}

			}

			// Execute background processing step for all modules
			callAllModules(BACKGROUND);

			// Flush buffered frame data to file so it’s visible during runtime
			flushFrameToFile();

		}

		std::cout << yellow;
		std::cout << ">>Total frames: " << getFrameCounter() << std::endl;
		std::cout << ">>Shutting down haOS" << std::endl;
		std::cout << def;
	}
	//==============================================================================


	static void callAllModules(HAOS_ROUTINE entryPoint)
	{
		for (int coreIdx = 0; coreIdx < haOS.coresNumber; coreIdx++)
		{
			pHAOS_Core_t pCore = &haOS.coreTable[coreIdx];

			haOS.pActiveCore = pCore;

			pHAOS_OdtEntry_t mb = pCore->moduleMIFs;

			for (int moduleIdx = 0; moduleIdx < pCore->modulesCnt; moduleIdx++)
			{
				HAOS_Mct_t* HAOS_mctPtr = mb->MIF->MCT;

				if (!HAOS_mctPtr) continue;

				switch (entryPoint)
				{
				case PREKICK:
					if (HAOS_mctPtr->Prekick) HAOS_mctPtr->Prekick(mb->MIF);
					break;
				case POSTKICK:
					if (HAOS_mctPtr->Postkick) HAOS_mctPtr->Postkick();
					break;
				case TIMER:
					if (HAOS_mctPtr->Timer) HAOS_mctPtr->Timer();
					break;
				case FRAME:
					if (HAOS_mctPtr->Frame) HAOS_mctPtr->Frame();
					break;
				case BRICK:
					if (HAOS_mctPtr->Brick) HAOS_mctPtr->Brick();
					break;
				case AFAP:
					if (HAOS_mctPtr->AFAP) HAOS_mctPtr->AFAP();
					break;
				case BACKGROUND:
					if (HAOS_mctPtr->Background) HAOS_mctPtr->Background();
					break;
				case POSTMALLOC:
					if (HAOS_mctPtr->Postmalloc) HAOS_mctPtr->Postmalloc();
					break;
				case PREMALLOC:
					if (HAOS_mctPtr->Premalloc) HAOS_mctPtr->Premalloc();
					break;
				}

				mb++;
			}
		}
	}
	//==============================================================================

	/**
 * @brief Initializes the list of cores used in the current concurrency context.
 *
 * This function populates the global haosSystem.coreTable with HAOS_Core_t instances,
 * one for each core (from 0 to haosSystem.coresNumber - 1). Each core entry is initialized
 * with its corresponding coreID and an empty module list.
 */
	static void makeCoresList()
	{
		memset(haOS.coreTable, 0, sizeof(HAOS_CoreTable_t));

		// Loop through the number of active cores defined in haosSystem.coresNumber
		for (int idx = 0; idx < haOS.coresNumber; idx++)
		{
			// Create a new core instance and assign its ID
			HAOS_Core_t* pCore = &haOS.coreTable[idx];

			haOS.pActiveCore = pCore;

			pCore->coreID = idx;

			// Clear the module list (initially empty)
			memset(pCore->moduleMIFs, 0, sizeof(HAOS_ModuleTable_t));
			pCore->modulesCnt = 0;

			// Assign the shared static I/O buffer to current core
			// All cores point to the same global I/O buffer
			pCore->IOBUFFER = (HAOS_BrickBuffer_t(*)[IO_BUFFER_PER_CHAN_MODULO]) sharedIObuffer[0];

			// Assign the input FIFO buffer only to core 0; others receive nullptr
			//static_cast<bitripper::BitRipperState_t*>(core.bitRipper)->inputFIFO = (idx == 0) ? inputFIFO : nullptr;
			for (int fifo = 0; fifo < MAX_FIFO_CNT; fifo++)
			{
				uint32_t size = 0;
				switch (fifo)
				{
				case 0:
					size = FIFO0_SIZE;
					break;
				case 1:
					size = FIFO1_SIZE;
					break;
				}
				if (size)
				{
					Core::initFIFO(fifo, sharedInputFIFO[idx][fifo], size);

				}
			}
		}

	}

	/**
	 * @brief Adds modules from a raw ODT (Overlay Definition Table) to the specified core.
	 *
	 * This function takes a void pointer to an ODT table, interprets it as an array of
	 * HAOS_OdtEntry_t structures, and appends each valid module entry to the core's
	 * moduleMIFs array. The table is terminated by a null MIF pointer.
	 *
	 * @param moduleList Pointer to an array of HAOS_OdtEntry_t entries (ODT table).
	 * @param pCore Pointer to the HAOS_Core_t structure representing the target core.
	 */
	static void addCoreModules(void* moduleList, pHAOS_Core_t pCore)
	{
		// Proceed only if the module list is provided
		if (moduleList != nullptr)
		{
			// Cast the generic pointer to a typed ODT entry pointer
			pHAOS_OdtEntry_t pOdt = (pHAOS_OdtEntry_t)moduleList;
			while (pOdt != NULL)
			{
				// A valid entry has a non-null MIF pointer
				if (pOdt->MIF != nullptr)
				{
					// Add the module entry to the core's list of modules
					memcpy(&pCore->moduleMIFs[pCore->modulesCnt++], pOdt, sizeof(HAOS_OdtEntry_t));
				}
				else
				{
					// Stop processing when a null MIF marks the end of the table
					break;
				}
				pOdt++;
			}
		}
	}
	//==============================================================================


	// @brief Initializes all DSP cores in the haOS system.
	//
	// This function performs one-time setup for each core defined in haosSystem.coreTable.
	// It clears I/O buffer memory, initializes input/output buffer pointers for each channel,
	// sets the available free-space counter (IOfree), applies the default post-processing
	// channel mask (HAOS_PPM_VALID_CHANNELS), and initializes the BitRipper state.
	//
	// Should be called once during system initialization, before any processing begins.
	//
	// @note Assumes that haosSystem.coreTable has already been populated.

	static void initCores()
	{
		// Loop through all cores defined in the haOS system
		for (int coreIdx = 0; coreIdx < haOS.coresNumber; coreIdx++)
		{
			pHAOS_Core_t pCore = &haOS.coreTable[coreIdx];

			haOS.pActiveCore = pCore;

			// Get raw pointer to the beginning of the I/O buffer for this core
			HAOS_PcmSamplePtr_t ioBufferPtr = (HAOS_PcmSamplePtr_t)pCore->IOBUFFER;

			// Size of one channel's I/O buffer (all its bricks)
			int32_t chChunkSize = IO_BUFFER_PER_CHAN_MODULO * sizeof(HAOS_BrickBuffer_t);

			// Set initial free-space value per channel (in samples)
			pCore->IOfree = IO_BUFFER_SIZE_PER_CHAN;

			// Initialize I/O pointers for each channel
			for (int i = 0; i < NUMBER_OF_IO_CHANNELS; ++i)
			{
				// Clear buffer memory for this channel
				memset(ioBufferPtr, 0, chChunkSize);

				// Set IO pointers to the start of the channel's buffer
				pCore->HAOS_IOBUFFER_INP_PTRS[i] = ioBufferPtr;
				pCore->HAOS_IOBUFFER_PTRS[i] = ioBufferPtr;

				// Move to the next channel's buffer location
				ioBufferPtr += IO_BUFFER_PER_CHAN_MODULO * BRICK_SIZE;
			}

			// Set the default valid channel mask for post-processing
			pCore->HAOS_PPM_VALID_CHANNELS = DEFAULT_PPM_CHANNEL_MASK;

			// init FIFO and open input file

			/* This is for kickstart - for first switchInputFIFO() call after Init to work properly */
			Core::initBitripper(0);
		}
	}
	//==============================================================================

	HAOS_PcmSamplePtr_t* getIOChannelPointerTable()
	{
		return haOS.pActiveCore->HAOS_IOBUFFER_PTRS;
	}
	//==============================================================================

	void setInputStreamEOF(bool value)
	{
		haOS.inStream.ctrlFlags &= HAOS_STREAM_END_OF_FILE_CLR;
		if (value)
		{
			haOS.inStream.ctrlFlags |= HAOS_STREAM_END_OF_FILE_FLAG;
		}
	}
	//==============================================================================

	bool getInputStreamEOF()
	{
		return (haOS.inStream.ctrlFlags & HAOS_STREAM_END_OF_FILE_FLAG);
	}
	//==============================================================================

	bool getEndOfProcessing()
	{
		return (getInputStreamFS() && !haOS.flushDataCnt);
	}
	//==============================================================================

	int32_t getInputStreamFS()
	{
		return haOS.inStream.samplingFrequency;
	}
	//==============================================================================

	int32_t getInputStreamChCnt()
	{
		return haOS.inStream.channelCount;
	}
	//==============================================================================

	HAOS_ChannelMask_t getValidChannelMask()
	{
		return haOS.pActiveCore->HAOS_PPM_VALID_CHANNELS;
	}
	//==============================================================================

	bool isActiveChannel(int32_t chIdx)
	{
		bool retValue = false;
		if (haOS.pActiveCore->HAOS_PPM_VALID_CHANNELS & (1U << chIdx))
			retValue = true;

		return retValue;
	}
	//==============================================================================

	void setValidChannelMask(HAOS_ChannelMask_t newMask)
	{
		haOS.pActiveCore->HAOS_PPM_VALID_CHANNELS = newMask;
	}
	//==============================================================================

	void* getActiveCore()
	{
		return (void*)haOS.pActiveCore;
	}
	//==============================================================================

	void* getActiveCoreBitRipper()
	{
		return (void*)haOS.pActiveCore->pBitRipper;
	}
	//==============================================================================

	void setCompressedInputStream(bool value)
	{
		haOS.inStream.ctrlFlags &= HAOS_STREAM_COMMPRESSED_CLR;
		if (value)
		{
			haOS.inStream.ctrlFlags |= HAOS_STREAM_COMMPRESSED_FLAG;
		}
	}
	//==============================================================================

	bool getCompressedInputStream()
	{
		return (haOS.inStream.ctrlFlags & HAOS_STREAM_COMMPRESSED_FLAG);
	}
	//==============================================================================

	void requestMemoryAllocation(bool clearFirstFrameReceived)
	{
		haOS.ctrlFlags |= HAOS_SYS_MEM_ALLOC_REQUESTED_FLAG;
		if (clearFirstFrameReceived)
		{
			haOS.ctrlFlags &= HAOS_DECODING_STARTED_CLR;
		}

	}
	//==============================================================================

	uint32_t getFrameCounter()
	{
		return haOS.frameCounter;
	}
	//==============================================================================

	//==============================================================================
	//======================= INTERNAL LIBRARY FUNCTIONS ===========================
	//==============================================================================
	// 
	// Initializes the main HAOS system structure with default values.
	//
	// This function sets up internal HAOS system fields prior to simulation.
	// It defines the number of DSP cores, configures shared I/O and FIFO buffers,
	// and resets various counters and flags to their initial state.
	//
	// This should be called before any processing or module loading begins.

	static void initSystemStruct()
	{
		// Set the number of DSP cores for the current concurrency mode
		haOS.coresNumber = NUMBER_OF_CORES;

		// Set the foreground-to-background brick processing ratio (e.g., 16 bricks per frame)
		haOS.fg2bg_ratio = 16;

		// Assign pointers to globally shared I/O and FIFO buffers
		haOS.systemIObuffers = (int32_t*)sharedIObuffer;
		haOS.systemFIFObuffers = (uint32_t*)sharedInputFIFO;

		// Clear the content of the shared I/O buffer
		memset(sharedIObuffer, 0, sizeof(sharedIObuffer));

		// Clear the content of the shared input FIFO buffer
		memset(sharedInputFIFO, 0, sizeof(sharedInputFIFO));

		// Initialize read and write brick counters
		haOS.readBrickCnt = 0;
		haOS.writeBrickCnt = 0;

		// Clear the all control flags
		haOS.ctrlFlags = HAOS_CLEAR_ALL_FLAGS;

		// Reset input stream state: not at end-of-file
		haOS.inStream.ctrlFlags = HAOS_CLEAR_ALL_FLAGS;

		// Mark input stream as not yet opened (used for lazy file open)
		haOS.inStream.ctrlFlags |= HAOS_STREAM_FIRST_OPEN_FLAG;

		/* Number of bits per sample in output wave file */
		haOS.outStream.bitsPerSample = OUTPUT_HANDLER_BITS_PER_SAMPLE_DFLT;

		// Reset output stream state
		haOS.outStream.ctrlFlags = HAOS_CLEAR_ALL_FLAGS;

		/* Initialize frame counter */
		haOS.frameCounter = -1;

		/* Initialize flush frame counter */
		haOS.flushDataCnt = HAOS_FLUSH_FRAMES_CNT_DFLT;
	}

	static void parseCmdLine(int argc, const char* argv[])
	{
		std::string programName = argv[0];
		programName.erase(0, programName.find_last_of("\\/") + 1);
		std::string arg;

		if (argc <= 1)
		{
			usage(programName.c_str());
			exit(1);
		}

		std::cout << yellow;
		std::cout << ">>Command line: ";
		for (int i = 1; i < argc; i++)
		{
			std::cout << argv[i] << " ";
		}
		std::cout << def << std::endl;

		for (int i = 1; i < argc; )
		{
			arg = argv[i++];

			if (arg.find("--help") == 0)
			{
				usage(programName.c_str());
				exit(0);
			}
			else if (arg.find("--fg2bg") == 0)
			{
				if (i < argc)
				{
					std::istringstream is(argv[i++]);
					is >> haOS.fg2bg_ratio;
				}
				else
				{
					usage(programName.c_str());
					exit(1);
				}
			}
			else if (arg.find("--cfg") == 0)
			{
				if (i < argc)
				{
					haOS.cfgPath = argv[i++];
				}
				else
				{
					usage(programName.c_str());
					exit(1);
				}
			}
			else if (arg.find("--input") == 0)
			{
				if (i < argc)
				{
					haOS.inStream.filePath = argv[i++];
				}
				else
				{
					usage(programName.c_str());
					exit(1);
				}
			}
			else if (arg.find("--output") == 0)
			{
				if (i < argc)
				{
					haOS.outStream.filePath = argv[i++];
				}
				else
				{
					usage(programName.c_str());
					exit(1);
				}
			}
			else if (arg.find("--osample") == 0)
			{
				if (i < argc)
				{
					std::istringstream is(argv[i++]);
					is >> haOS.outStream.bitsPerSample;
				}
				else
				{
					usage(programName.c_str());
					exit(1);
				}
			}
			else if (arg.find("--ofs") == 0)
			{
				if (i < argc)
				{
					std::istringstream is(argv[i++]);
					is >> haOS.outStream.samplingFrequency;
				}
				else
				{
					usage(programName.c_str());
					exit(1);
				}
			}
			else
			{
				if (i-- < argc)
					std::cerr << red << "ERROR: Unknown option " << argv[i] << def << std::endl;
				exit(1);
			}
		}
	}
	//==============================================================================

	static void openInputFile()
	{
		if (!haOS.inStream.filePath.empty())
		{
			/* Handle first-time file open */
			if (haOS.inStream.ctrlFlags & HAOS_STREAM_FIRST_OPEN_FLAG)
			{
				/* Try to open the WAV input file */
				int openFileStatus = cl_wavread_open(const_cast<char*>(haOS.inStream.filePath.c_str()), &haOS.inStream.fileHandle);
				if (openFileStatus < 0)
				{
					/* Print error message and exit if file cannot be opened */
					std::cerr << red << "ERROR: Unable to open input file '" << haOS.inStream.filePath << "': file does not exist or is not a WAV file" << def << std::endl;
					exit(1);
				}

				/* Confirm file successfully opened */
				std::cout << yellow;
				std::cout << ">>Input file: " << haOS.inStream.filePath << std::endl;
				
				/* Clear first-open flag */
				haOS.inStream.ctrlFlags &= HAOS_STREAM_FIRST_OPEN_CLR;

				/* Default stream type is unknown */
				haOS.inStream.type = DECODE_INFO_UNKNOWN;

				/* openFileStatus == 1 => WAVE input file */
				if (openFileStatus)
				{
					/* Read sampling frequency from WAV file */
					haOS.inStream.samplingFrequency = cl_wavread_frame_rate(haOS.inStream.fileHandle);

					/* If valid frequency, assume PCM file */
					if (haOS.inStream.samplingFrequency)
					{
						/* Set stream type to PCM */
						haOS.inStream.type = DECODE_INFO_PCM;

						/* Read WAV file metadata */
						haOS.inStream.channelCount = cl_wavread_getnchannels(haOS.inStream.fileHandle);
						haOS.inStream.bitsPerSample = cl_wavread_bits_per_sample(haOS.inStream.fileHandle);
						haOS.inStream.chSamplesCnt = cl_wavread_number_of_channel_samples(haOS.inStream.fileHandle);
						haOS.inStream.fileSamplesCnt = haOS.inStream.chSamplesCnt * haOS.inStream.channelCount;

						std::cout << ">>Sample rate: " << haOS.inStream.samplingFrequency << std::endl;
						std::cout << ">>Bits per sample: " << haOS.inStream.bitsPerSample << std::endl;
						std::cout << ">>Channels: " << haOS.inStream.channelCount << std::endl;
						std::cout << ">>Samples per channel: " << haOS.inStream.chSamplesCnt << std::endl;
						std::cout << def;

						/* Set EOF flag and close file if empty */
						setInputStreamEOF(haOS.inStream.chSamplesCnt <= 0);
						if (getInputStreamEOF())
						{
							cl_wavread_close(haOS.inStream.fileHandle);
						}
					}
				}
				else
				{
					haOS.inStream.samplingFrequency = 48000;
				}
			}
		}
	}
	//==============================================================================

	static uint32_t calcChCntBasedOnChMask(HAOS_ChannelMask_t chMask)
	{
		uint32_t ret = 0;
		while (chMask) {
			ret += (chMask & 1);
			chMask >>= 1;
		}
		return ret;
	}
	//==============================================================================

	static bool openOutputFile()
	{
		// if an output file is specified, try to open it and be prepared to read it
		if (!haOS.outStream.filePath.empty())
		{
			haOS.outStream.channelCount = calcChCntBasedOnChMask(HAOS::getValidChannelMask());
			haOS.outStream.samplingFrequency = haOS.inStream.samplingFrequency;

			if (cl_wavwrite_open(
				const_cast<char*>(haOS.outStream.filePath.c_str()),
				haOS.outStream.bitsPerSample,
				haOS.outStream.channelCount,
				haOS.outStream.samplingFrequency,
				&haOS.outStream.fileHandle))
			{
				std::cerr << "Unable to open output file '" << haOS.outStream.filePath << "'" << std::endl;
				exit(1);
			}

			return true;
		}

		return false;
	}
	//==============================================================================

	static void writeToFile()
	{
		if (haOS.outStream.fileHandle == nullptr)
		{
			openOutputFile();
		}


		cl_wavwrite_update_wave_header(haOS.outStream.fileHandle);

		// Access the last core in haosSystem.coreTable
		if (haOS.coresNumber == 0)
			return;

		pHAOS_Core_t lastCore = &haOS.coreTable[haOS.coresNumber - 1];

		for (int sample = 0; sample < BRICK_SIZE; sample++)
		{
			// Write one sample for each valid output channel from the last core
			for (int channel = 0; channel < haOS.outStream.channelCount; channel++)
			{
				int32_t sampleToWrite = lastCore->HAOS_IOBUFFER_PTRS[channel][sample] * SAMPLE_SCALE;
				//std::cout << "sampleToWrite: 0x" << std::hex << sampleToWrite << std::endl;
				cl_wavwrite_sendsample(haOS.outStream.fileHandle, sampleToWrite, haOS.outStream.ctrlFlags & HAOS_STREAM_ROUNDING_FLAG);
			}
		}
	}
	//==============================================================================

	static void flushFrameToFile()
	{
		if (haOS.outStream.fileHandle == nullptr)
		{
			return;
		}

		// update file size and number of samples
		cl_wavwrite_close(haOS.outStream.fileHandle);

		if (getEndOfProcessing()) {
			std::cout << yellow;
			std::cout << ">>Output file: " << haOS.outStream.filePath << std::endl;
			std::cout << ">>Sample rate: " << haOS.outStream.samplingFrequency << std::endl;
			std::cout << ">>Bits per sample: " << haOS.outStream.bitsPerSample << std::endl;
			std::cout << ">>Channels: " << haOS.outStream.channelCount << std::endl;
			std::cout << ">>Samples per channel: " << cl_wavwrite_number_of_channel_samples(haOS.outStream.fileHandle) << std::endl;
			std::cout << def;
		}

		if (cl_wavwrite_reopen(const_cast<char*>(haOS.outStream.filePath.c_str()), haOS.outStream.fileHandle))
		{
			std::cerr << red << "ERROR: Unable to open output file <flush> '" << haOS.outStream.filePath << "'" << def << std::endl;
			exit(1);
		}

	}
	//==============================================================================

	static void updatePtrs()
	{
		haOS.readBrickCnt++;
		if (haOS.readBrickCnt == IO_BUFFER_PER_CHAN_MODULO)
		{
			haOS.readBrickCnt = 0;
		}

		// Iterate over all active cores and update their buffer pointers
		for (auto& core : haOS.coreTable)
		{
			for (int i = 0; i < NUMBER_OF_IO_CHANNELS; ++i)
			{
				core.HAOS_IOBUFFER_PTRS[i] = core.IOBUFFER[i][haOS.readBrickCnt];
			}
			core.IOfree += BRICK_SIZE;
		}


	}
	//==============================================================================

	static void readCfgFile(const std::string pathName, HAOS_HostCommQ_t message, uint32_t* msgCnt)
	{
		std::ifstream is;
		is.open(pathName.c_str());

		if (is.is_open())
		{
			std::string line;

			while (std::getline(is, line))
			{
				if (line.find("#include") == 0)
				{
					// process include file
					std::string includePath = trim(line.substr(8, std::string::npos));
					readCfgFile(includePath, message, msgCnt);
				}
				else if (line.find("# Frame: ") == 0)
				{
					/* Placeholder for code used to simulate dynamic modification of ucmd commands based on the current frame number */
				}
				else if (line.find("#") == 0)
				{
					// this is a comment, ignore it
				}
				else if (!line.empty())
				{
					/* assume these are hex digits - parse them into the message queue */
					if (line.length() > 8 && line.find(' ') == std::string::npos)
					{
						/* Separate two control words*/
						line = line.substr(0, 8) + " " + line.substr(8);
					}

					std::istringstream iss(line);
					uint32_t msg;
					while (iss >> std::hex >> msg)
					{
						message[*msgCnt] = msg;
						*msgCnt += 1;
					}

				}
			}
			is.close();
		}
		else
		{
			std::cerr << "Unable to open cfg file '" << pathName.c_str() << "'" << std::endl;
		}
	}
	//==============================================================================

	static void* getMcvPointer(int32_t moduleID)
	{

		for (auto& core : haOS.coreTable)
		{
			pHAOS_OdtEntry_t mb = core.moduleMIFs;

			for (int idx = 0; idx < core.modulesCnt; idx++)
			{
				if (mb->moduleID == moduleID)
				{
					return mb->MIF->MCV;
				}
				mb++;
			}
		}

		return nullptr;
	}
	//==============================================================================

	static void processHostComm(HAOS_HostCommQ_t messageQ, uint32_t msgCnt)
	{
		//HAOS_HostCommQ_t::const_iterator mb, me;

		//for (mb = messageQ.begin(), me = messageQ.end(); mb != me;)


		for (unsigned int msgIdx = 0; msgIdx < msgCnt; )
		{
			uint32_t cmd = messageQ[msgIdx++];

			// parse the command word
			int32_t moduleID = (cmd >> 24) & 0x7f;
			int32_t opCode = (cmd >> 22) & 0x3;
			int32_t numWords = ((cmd >> 16) & 0x1f) + 1;
			int32_t offset = cmd & 0xffff;

			uint32_t* mcv = (uint32_t*)getMcvPointer(moduleID);

			switch (opCode)
			{
			case 3:		// read
			{
				// TODO: figure out if we want to support reads
				break;
			}
			default:	// write, OR, or AND
			{
				// for each word of the payload, do the right thing
				while (msgIdx < msgCnt && numWords > 0)
				{
					int32_t value = messageQ[msgIdx++];
					numWords--;

					if (mcv != 0)
					{
						// messages are for a resident module
						switch (opCode)
						{
						case 0:
						{
							mcv[offset] = value;
							break;
						}
						case 1:
						{
							mcv[offset] |= value;
							break;
						}
						case 2:
						{
							mcv[offset] &= value;
							break;
						}
						}
					}
					++offset;
				}
				break;
			}
			}
		}
	}
	//==============================================================================

	static void readPrekickConfigs()
	{
		HAOS_HostCommQ_t message;
		uint32_t msgCnt = 0;

		if (!haOS.cfgPath.empty())
		{
			readCfgFile(haOS.cfgPath, message, &msgCnt);
			// message is now a list of unsigned int messages
			processHostComm(message, msgCnt);
		}
	}
	//==============================================================================

	void copyBrickToIO(pHAOS_CopyToIOPtrs_t copyToIOPtrs)
	{
		if (haOS.pActiveCore == nullptr)
		{
			return; // No cores present
		}

		// Copy frame data if present
		haOS.ctrlFlags &= HAOS_FRAME_TRIGGERED_CLR;
		if (copyToIOPtrs->frameData != nullptr)
		{
			haOS.ctrlFlags |= HAOS_FRAME_TRIGGERED_FLAG;
			haOS.ctrlFlags |= HAOS_DECODING_STARTED_FLAG;
			memcpy(&haOS.frameData, copyToIOPtrs->frameData, sizeof(HAOS_FrameData_t));

			/* Update output file controls */
			haOS.outStream.samplingFrequency = haOS.frameData.sampleRate;

		}

		// Copy bricks to core0 input buffer
		for (int ch = 0; ch < NUMBER_OF_IO_CHANNELS; ch++)
		{
			if (copyToIOPtrs->IOBufferPtrs[ch])
			{
				memcpy(haOS.pActiveCore->HAOS_IOBUFFER_INP_PTRS[ch], copyToIOPtrs->IOBufferPtrs[ch], sizeof(HAOS_BrickBuffer_t));
			}
			else
			{
				memset(haOS.pActiveCore->HAOS_IOBUFFER_INP_PTRS[ch], 0, sizeof(HAOS_BrickBuffer_t));
			}
		}

		// Update I/O buffer space accounting
		haOS.pActiveCore->IOfree -= BRICK_SIZE;

		// Update input pointers for core0
		haOS.writeBrickCnt++;
		if (haOS.writeBrickCnt == IO_BUFFER_PER_CHAN_MODULO)
		{
			haOS.writeBrickCnt = 0;
		}

		for (int i = 0; i < NUMBER_OF_IO_CHANNELS; ++i)
		{
			haOS.pActiveCore->HAOS_IOBUFFER_INP_PTRS[i] = haOS.pActiveCore->IOBUFFER[i][haOS.writeBrickCnt];
		}
	}
	//==============================================================================

	/*
	 * Fills the BitRipper input FIFO with new samples from the input stream.
	 *
	 * This function handles the initialization and reading of the input stream from a WAV file.
	 * If the file is not yet opened, it performs the initial setup and validates the input.
	 * It then fills the BitRipper FIFO buffer with decoded samples until either the buffer is
	 * full or the end-of-file (EOF) is reached. In the case of EOF, it fills the remaining
	 * FIFO space with zeros to avoid processing garbage data.
	 */
	void fillInputFIFO()
	{
		/* Check if input file path is set */
		if (!haOS.inStream.filePath.empty())
		{
			/* Handle first-time file open */
			if (haOS.inStream.ctrlFlags & HAOS_STREAM_FIRST_OPEN_FLAG)
			{
				openInputFile();
			}

			/* Fill FIFO from input file until EOF is reached */
			while (!getInputStreamEOF())
			{
				/* Get available space in FIFO */
				uint32_t freeSamplesCnt = BitRipper::getFreeSpaceInWords();

				/* Do not proceed unless there are at least 32 free words */
				if (freeSamplesCnt < 32)
				{

					/* Log successful fill */
					//std::cout << "FIFO successfully filled." << std::endl;
	/*
					bitripper::skipBits(32*8);

					int32_t wordR = bitripper::peek(32);
					int32_t shortR = bitripper::peek(16);
					int32_t byteR = bitripper::peek(8);
	*/

					break;
				}

				/* Limit fill operation to 32 samples at a time */
				freeSamplesCnt = 32;

				/* Get FIFO write pointer */
				uint32_t* dstPtr = BitRipper::getWritePtr();

				/* Read samples from file into FIFO */
				for (int sample = 0; sample < freeSamplesCnt; sample++)
				{
					*dstPtr++ = cl_wavread_recvsample(haOS.inStream.fileHandle, haOS.inStream.ctrlFlags & HAOS_STREAM_COMMPRESSED_FLAG);
				}


				/* Advance FIFO write pointer */
				BitRipper::advanceWritePtr(freeSamplesCnt);

				/* Check if end of file has been reached */
				if (cl_wavread_eof(haOS.inStream.fileHandle))
				{
					/* Set EOF flag and close file */
					setInputStreamEOF(true);
					std::cout << std::endl << yellow << ">>EOF reached" << def << std::endl;
					cl_wavread_close(haOS.inStream.fileHandle);
				}
			}
		}

		/* If input file is not set or EOF reached, pad FIFO with zero samples */
		if (haOS.inStream.filePath.empty() || getInputStreamEOF())
		{
			/* Check if there's enough room to write 32 zero samples */
			uint32_t freeSamplesCnt = BitRipper::getFreeSpaceInWords();
			if (freeSamplesCnt >= 32)
			{
				freeSamplesCnt = 32;

				/* Get write pointer */
				uint32_t* dstPtr = BitRipper::getWritePtr();

				/* Write zero samples to FIFO */
				for (int sample = 0; sample < freeSamplesCnt; sample++)
				{
					if (getCompressedInputStream())
						*dstPtr++ = 0xdedaceda;
					else
						*dstPtr++ = 0;
				}

				/* Advance write pointer */
				BitRipper::advanceWritePtr(freeSamplesCnt);
			}
		}
	}
	//==============================================================================

	static std::string trim(std::string const& str)
	{
		if (str.empty())
		{
			return str;
		}

		std::size_t first = str.find_first_not_of(' ');
		std::size_t last = str.find_last_not_of(' ');

		return str.substr(first, last - first + 1);
	}
	//==============================================================================

	static void usage(const char* programName)
	{
		std::cout << grey;
		std::cout << "usage: " << programName << " [options]" << std::endl
			<< "  where options are:" << std::endl
			<< "    --fg2bg <ratio of brick to background entry point calls> - default is 16" << std::endl
			<< "    --cfg <cfg file pathname> : pathname of host comm messages to send prekick" << std::endl
			<< "    --input <input audio WAV file pathname> : pathname of audio file to use as input" << std::endl
			<< "           WAV file channels are mapped to IOBUFFER channels based on the CS498XX PCM decoder MCV" << std::endl
			<< "           settings for index 0x0001 through 0x0010. Those default to mapping each WAV channel" << std::endl
			<< "           to the next IOBUFFER channel, in ascending order from 0 to 15" << std::endl
			<< "    --output <output audio WAV file pathname> : pathname of audio file to write as output" << std::endl
			<< "           Channels in the WAV file are mapped from the IOBUFFER based on the valid channel mask" << std::endl
			<< "           OS variable. The first valid channel is written to WAV channel 0, the 2nd valid channel" << std::endl
			<< "           is written to WAV channel 1, etc." << std::endl
			<< "    --osamplesize <output sample size in bits> - default is 16" << std::endl
			<< "    --ofs <output sample rate> - default is 48000" << std::endl
			;
		std::cout << yellow << ">>Exiting haOS" << std::endl;
		std::cout << def;
	}
	//==============================================================================
}