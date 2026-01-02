/*
 * bitripper_sim.h
 *
 *  Created on: Aug 12, 2024
 *      Author: katarinac
 */
#pragma once
#ifndef __BITRIPPER_H__
#define __BITRIPPER_H__


#include "haos_api.h"


 // Bitmask for internal control flags used in bitripper state
#define BITRIPPER_IN_AUX_STATE_FLAG             BIT_00_SET 			// flag for aux state
#define BITRIPPER_IN_AUX_STATE_CLR              BIT_00_CLR

#define BITRIPPER_FIFO_FULL_FLAG             	BIT_01_SET 			// flag for full FIFO
#define BITRIPPER_FIFO_FULL_CLR             	BIT_01_CLR

#define MAX_BITS 	32

namespace BitRipper
{
    /*
    * Extracts a specified number of bits from the BitRipper input stream.
    *
    * This function extracts `bitsNeeded` bits (in the range [1, MAX_BITS]) from the current BitRipper input stream,
    * managing bit-level access across FIFO words. If the number of requested bits exceeds the remaining bits in the
    * current word, the function refills the internal word by reading the next 32-bit value from the FIFO and continues
    * extracting.
    *
    * The BitRipper state is updated accordingly: `currentWord` is shifted left, and `bitsRemaining` is decremented.
    * If all bits are consumed from the current word, the internal value is cleared.
    *
    * @param bitsNeeded Number of bits to extract (must be between 1 and MAX_BITS).
    * @return The extracted bits packed into the least significant bits of the return value.
    */
    int32_t extractBits(uint32_t bitsNeeded);

    /*
    * Peeks the specified number of bits from the BitRipper input stream without advancing the read pointer.
    *
    * This function allows non-destructive reading of `bitsNeeded` bits (1–32) from the current input position.
    * It behaves similarly to `extractBits()` but does not modify the BitRipper state. If the requested bits
    * span across two FIFO words, the second word is temporarily read but not committed to the decoder state.
    *
    * This is useful in parsing situations where upcoming bits must be examined before a decoding decision
    * is made, without consuming the input stream.
    *
    * @param bitsNeeded Number of bits to peek (must be between 1 and MAX_BITS).
    * @return Extracted bits packed into the least significant bits of the return value.
    */
    uint32_t peek(uint32_t bitsNeeded);

    /*
    * Skips a specified number of bits in the BitRipper input stream.
    *
    * This function moves the read pointer forward or backward by the specified number
    * of bits. It updates the internal state (`readPtr`, `currentWord`, and `bitsRemaining`)
    * to reflect the new bitstream position. Both positive (forward) and negative (rewind)
    * skips are supported.
    *
    * When skipping forward, the FIFO is refilled as needed (ignoring auxiliary state locking),
    * and the pointer wrapping is handled for both forward and backward cases. The resulting
    * state is consistent with the new bit-aligned input position.
    *
    * @param skipBits Signed number of bits to skip. Positive value skips forward,
    *                 negative value skips backward.
    */
    void skipBits(int32_t skipBits);

    /*
    * Returns the number of available 32-bit word slots in the input FIFO.
    *
    * This function calculates how many 32-bit words of space are currently free in the
    * BitRipper input FIFO buffer. It does so by reading the number of occupied bits using
    * `readDipstick()`, converting that value to 32-bit word count (via right shift by 5),
    * and subtracting it from the total FIFO capacity (FIFO_SIZE).
    *
    * @return Number of free 32-bit word slots in the FIFO.
    */
    uint32_t getFreeSpaceInWords();

    /*
    * @brief Returns a pointer to the write position in the BitRipper FIFO buffer.
    *
    * This function provides access to the `writePtr` used by the BitRipper instance
    * associated with the currently active HaOS DSP core. The write pointer indicates
    * the next available location in the FIFO buffer where new input data can be written.
    *
    * It is typically used by refill or input simulation routines to inject data
    * into the bitstream processing pipeline.
    *
    * @return Pointer to the current write position in the BitRipper FIFO buffer.
    */
    uint32_t* getWritePtr();

    /*
    * Advances the FIFO write pointer and updates FIFO full flag.
    *
    * This function advances the write pointer of the BitRipper's input FIFO
    * by the specified number of 32-bit words. It handles circular wrap-around
    * and updates the internal full flag if the FIFO becomes full after advancement.
    *
    * If the system is currently operating in auxiliary state mode, the comparison
    * is made against the `mainStateBackup.readPtr` to maintain consistent behavior.
    *
    * @param wordsWritten Number of 32-bit words to advance the write pointer.
    */
    void advanceWritePtr(uint32_t wordsWritten);

    /*
    * Returns the number of bits available in the input FIFO.
    *
    * This function calculates how many bits are currently available to be read from the
    * input FIFO by evaluating the distance between the current read and write pointers,
    * taking into account the number of bits remaining in the current read word.
    * If the BitRipper is operating in auxiliary state, the backup main state is used instead.
    *
    * @return Number of bits available in the FIFO buffer.
    */
    uint32_t readDipstick();

    /*
    * Waits until the input FIFO contains at least the requested number of bits.
    *
    * This function monitors the number of valid bits currently present in the input FIFO,
    * using the `readDipstick()` function to determine the fill level. It blocks execution until the
    * specified threshold (`bitsRequired`) is reached.
    *
    * If the number of available bits is below the required level, the function triggers FIFO refilling
    * via `refillFIFO()` and rechecks the condition in a polling loop.
    *
    * The function asserts that `bitsRequired` does not exceed the total FIFO capacity (FIFO_SIZE * MAX_BITS),
    * ensuring that the request is valid and physically achievable.
    *
    * @param bitsRequired Minimum number of bits that must be present in the FIFO before continuing.
    */
    void waitOnDipstick(uint32_t bitsRequired);

    /*
    * Freezes the FIFO read pointer by backing up the current BitRipper state.
    *
    * This function freezes the state of the main read pointer in the BitRipper,
    * allowing parsing routines to operate on the input stream without affecting
    * the primary decoding flow. It copies the current BitRipper state into the
    * `mainStateBackup` structure and sets the control flag indicating that the
    * system is now operating in an auxiliary state.
    *
    * Used when temporary bitstream inspection is required, such as CRC
    * routines, ensuring the original decoding position remains unaffected.
    */
    void saveMainState();

    /*
    * Restores the previously backed-up FIFO read pointer state.
    *
    * This function restores the main BitRipper state (`currState`) from the backup
    * structure (`mainStateBackup`). It effectively resets the decoder's read position
    * to the point where `saveMainState()` was previously invoked.
    *
    * After restoring, the auxiliary state flag is cleared, signaling the end of
    * temporary parsing and the resumption of normal decoding flow.
    *
    * Typically used after speculative or non-destructive bitstream inspection
    * (e.g., CRC checks or sync probing), to return the system to its original state.
    */
    void restoreMainState();

    /*
    * Loads an auxiliary BitRipper state as the new main state.
    *
    * This function replaces the current BitRipper state (`currState`) with the
    * state provided via the `pAuxState` pointer. This enables external routines
    * to set a custom decoding position and state context, effectively overriding
    * the default decoder progress.
    *
    * After loading the new state, the auxiliary mode flag is cleared to indicate
    * the system has exited temporary or speculative parsing.
    *
    * @param pAuxState Pointer to the BitRipper_t structure containing the alternate state.
    */
    void loadMainState(pBitRipper_t pAuxState);

    /*
    * Saves the current BitRipper state into an auxiliary structure.
    *
    * This function copies the current active BitRipper state into the structure
    * pointed to by `pAuxState`. This allows the decoder to preserve its current
    * bitstream parsing context and resume it later, for example after performing
    * temporary inspection or alternate parsing paths.
    *
    * @param pAuxState Pointer to a BitRipper_t structure where the current state will be saved.
    */
    void saveAuxState(pBitRipper_t pAuxState);

    /*
    * Loads auxiliary BitRipper state into the active decoder context
    *
    * This function replaces the current BitRipper state with a previously saved
    * auxiliary state, excluding the write pointer fields. It is typically used to
    * resume decoding or parsing from an alternative position without disrupting
    * the main decoding flow. Only the portion of the state relevant for read
    * operations is copied to preserve system consistency.
    *
    * @param pAuxState Pointer to the auxiliary BitRipper state structure
    */
    void loadAuxState(pBitRipper_t pAuxState);

    /*
    * Checks whether the BitRipper is currently operating in auxiliary state.
    *
    * This function reads the control flag BITRIPPER_IN_AUX_STATE_FLAG from the BitRipper
    * instance associated with the active HaOS core. It returns true if the system is currently
    * using an auxiliary state for bitstream parsing (e.g., during temporary inspection),
    * indicating that the main decoding state is frozen.
    *
    * @return true if auxiliary state is active, false otherwise.
    */
    bool getAuxStateFlag();

    /*
    * Calculates the bit distance between two BitRipper states.
    *
    * This function computes the number of bits between two BitRipper parsing states:
    * `pStateStart` and `pStateEnd`. It accounts for both the byte-level offset between
    * the `readPtr` pointers and the bit-level offset within the individual 32-bit words.
    * If the read pointer has wrapped around the FIFO, the offset is corrected accordingly.
    *
    * The result represents how many bits would be consumed by the BitRipper in order to
    * advance from `pStateStart` to `pStateEnd`.
    *
    * @param pStateStart Pointer to the starting BitRipper state.
    * @param pStateEnd   Pointer to the ending BitRipper state.
    * @return            Number of bits between the two states.
    */
    int32_t bitCntStates(pBitRipper_t pStateStart, pBitRipper_t pStateEnd);

    /*
    * Calculates the number of bits between the main BitRipper state and a given end state.
    *
    * This function computes the bitwise distance between the main decoding position (read pointer and bit offset)
    * and a specified auxiliary (end) state. If the BitRipper is currently operating in auxiliary mode,
    * it uses the backed-up main state as the starting reference.
    *
    * @param pStateEnd Pointer to the end BitRipper state to compare against.
    * @return Number of bits between the main state and the given end state.
    */
    int32_t bitCntMainState(pBitRipper_t pStateEnd);

    /*
    * Saves the bitstream alignment offset for future reference.
    *
    * This function computes the bit-level alignment between the current read pointer
    * position in the BitRipper and a bit located at a signed offset (`bitsOffset`)
    * from the current bit position. The result represents how far from an aligned
    * boundary (e.g., 8, 16, or 31 bits) the referenced position is.
    *
    * The computed alignment is stored in the `alignmentInfo` field,
    * which may be used later for restoring alignment or validating bit boundaries.
    *
    * @param bitsOffset Signed number of bits ahead or behind the current bit position
    *                   used to compute alignment relative to that position.
    */
    void saveAlignment(int32_t bitsOffset);

    /*
    * Aligns the BitRipper input stream to the nearest 8-bit boundary.
    *
    * This routine adjusts the BitRipper's internal state by skipping the minimal number of bits
    * necessary to reach the next byte-aligned position, based on the alignment reference previously
    * saved using `saveAlignment()`. It uses `alignCommon()` to compute the bit distance from the
    * saved alignment point and ensures alignment to an 8-bit boundary by applying a mask.
    *
    * Typically used before parsing byte-aligned data structures to ensure correct field extraction.
    */
    void alignToByte();

    /*
    * Aligns the BitRipper input stream to a 16-bit boundary.
    *
    * This function adjusts the current input pointer so that the BitRipper state is aligned
    * to the next 16-bit (2-byte) boundary relative to the alignment position saved using
    * saveAlignment(). It computes the required number of bits to skip by comparing
    * the current bit position with the stored alignment reference and masks the result to
    * ensure alignment to a 16-bit boundary.
    *
    * This is typically used when word-aligned access is needed during parsing of input streams.
    */
    void alignToWord();

    /*
    * Aligns the BitRipper input stream to a 32-bit (DWord) boundary.
    *
    * This function adjusts the current read position so that the BitRipper stream becomes aligned
    * to the next 32-bit boundary with respect to the reference position saved via SaveAlignment().
    * It calculates the number of bits to skip using the common alignment offset and masks the result
    * to round up to the nearest 32-bit aligned position.
    *
    * Typically used when parsing structures or data types that require natural 4-byte alignment.
    */
    void alignToDWord();

}

namespace bitripper_internal
{
    void init(pBitRipperFIFO_t pFIFOCfg, pBitRipperState_t pState);
}


#endif /* __BITRIPPER_H__ */