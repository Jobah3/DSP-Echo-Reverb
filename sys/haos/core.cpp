

#include <assert.h>

#include "haos.h"
#include "bitripper_sim.h"

namespace Core
{
    /*
     * Initializes a FIFO descriptor for the given ID, binding it to a
     * custom buffer address and size. This allows BitRipper to extract bits
     * from arbitrary module-owned buffers instead of the default system FIFO.
     *
     * Parameters:
     *    id   – FIFO identifier (0 .. MAX_FIFO_CNT–1), defined in haos_api.h.
     *    addr – Start address of the buffer in memory (must not be NULL).
     *    size – Buffer size in 32-bit words (must be > 0).
     *
     * Notes:
     *    - MAX_FIFO_CNT is currently set to 2 for testing purposes, but this
     *      value is arbitrary and can be changed.
     *    - FIFO 0 should always remain bound to the system FIFO.
     *    - Implemented in the haos_core namespace.
     */

    void initFIFO(uint32_t id, uint32_t* addr, int32_t size)
    {
        assert(id < MAX_FIFO_CNT && "Invalid FIFO ID!");

        pHAOS_Core_t pCore = (pHAOS_Core_t)HAOS::getActiveCore();

        pCore->FIFODescArray[id].ID = id;
        pCore->FIFODescArray[id].startAddr = addr;
        pCore->FIFODescArray[id].size = size;

    }

    /*
     * This function binds the BitRipper instance in the active core to the FIFO
     * descriptor identified by the given ID, and then initializes its internal
     * state for bit extraction.
     *
     * Parameters:
     *    id – FIFO identifier (0 .. MAX_FIFO_CNT–1), defined in haos_api.h.
     *
     * Notes:
     *    - MAX_FIFO_CNT is currently set to 2 for testing purposes only; the
     *      value can be adjusted if required.
     *    - FIFO 0 should always remain bound to the system FIFO for standard
     *      decoding.
     *    - This API is implemented in the haos_core namespace.
     *    - Intended for scenarios where a BitRipper needs to operate on a custom
     *      FIFO previously initialized with initFIFO().
     */
    void initBitripper(uint32_t id)
    {
        assert(id < MAX_FIFO_CNT && "Invalid FIFO ID!");

        pHAOS_Core_t pCore = (pHAOS_Core_t)HAOS::getActiveCore();

        pCore->pBitRipper = &pCore->bitRipperArray[id];

        /* Initialize the BitRipper state for this core */
        bitripper_internal::init(&pCore->FIFODescArray[id], pCore->pBitRipper);
    }

    /*
     * This function changes the currently active BitRipper pointer in the
     * active core to the instance associated with the specified FIFO ID.
     * It does not reinitialize the FIFO or BitRipper state; it simply
     * switches context so that subsequent bit extraction calls operate on
     * the selected FIFO.
     *
     * Parameters:
     *    id – FIFO identifier (0 .. MAX_FIFO_CNT–1), defined in haos_api.h.
     *
     * Notes:
     *    - MAX_FIFO_CNT is currently set to 2 for testing purposes only; the
     *      value can be increased or decreased as needed.
     *    - FIFO 0 should always remain bound to the system FIFO for standard
     *      decoding.
     *    - This API is implemented in the haos_core namespace.
     *    - Typically used after initFIFO() and initBitripper() to quickly
     *      switch between multiple configured FIFOs without reinitializing.
     */
    void switchBitripperFIFO(uint32_t id)
    {
        assert(id < MAX_FIFO_CNT && "Invalid FIFO ID!");

        pHAOS_Core_t pCore = (pHAOS_Core_t)HAOS::getActiveCore();

        pCore->pBitRipper = &pCore->bitRipperArray[id];
    }

}


