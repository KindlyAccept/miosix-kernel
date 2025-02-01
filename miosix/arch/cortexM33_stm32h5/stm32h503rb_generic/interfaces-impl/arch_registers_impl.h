#pragma once

#define STM32H503xx
#include "CMSIS/Device/ST/STM32H5xx/Include/stm32h5xx.h"
#include "CMSIS/Include/core_cm33.h"
#include "CMSIS/Device/ST/STM32H5xx/Include/system_stm32h5xx.h"

//RCC_SYNC is a Miosix-defined primitive for synchronizing the CPU with the RCC
//after modifying peripheral clocks/resets. It should be defined to a no-op on
//architectures without bus access reordering, and to a __DSB() on all other
//ARM architectures. The DMB is required for example in stm32f42x
//microcontrollers. Note that reordering does not necessarily happen at the
//CPU level alone, the bus matrices and peripherals themselves may also reorder
//accesses as a side-effect of how they work.
#define RCC_SYNC() __DSB()

//Peripheral interrupt start from 0 and the last one is 133, so there are 134
#define MIOSIX_NUM_PERIPHERAL_IRQ 134
