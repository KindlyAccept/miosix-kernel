
#pragma once

#include "../rp2040/hardware/rp2040.h"
#include "../rp2040/hardware/properties.h"
#include "../rp2040/hardware/platform.h"
#if defined(BOARD_VARIANT_PICO_W)
#include "../rp2040/hardware/boards/pico_w.h"
#else
#include "../rp2040/hardware/boards/pico.h"
#endif

//Peripheral interrupt start from 0 and the last one is 25, so there are 26
#define MIOSIX_NUM_PERIPHERAL_IRQ 26

namespace miosix {

// Definition of statically allocated spinlocks
struct RP2040HwSpinlocks
{
    enum {
        GIL = 0,        // Global interrupt lock
        Atomics,        // Mutual exclusion of atomic operations
        InitCoreSync,   // Used at the end of SMP setup to synchronize the cores
        FirstFree,
        Last = 32
    };
};

}
