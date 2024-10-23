/***************************************************************************
 *   Copyright (C) 2010-2021 by Terraneo Federico                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include "interfaces_private/cpu.h"
#include "kernel/kernel.h"
#include "kernel/error.h"
#include "interfaces_private/bsp.h"
#include "kernel/scheduler/scheduler.h"
#include "kernel/process.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cassert>

namespace miosix {

void IRQsystemReboot()
{
    NVIC_SystemReset();
}

void initCtxsave(unsigned int *ctxsave, unsigned int *sp,
                 void (*pc)(void *(*)(void*),void*),
                 void *(*arg0)(void*), void *arg1)
{
    unsigned int *stackPtr=sp;
    //Stack is full descending, so decrement first
    stackPtr--; *stackPtr=0x01000000;                                 //--> xPSR
    stackPtr--; *stackPtr=reinterpret_cast<unsigned int>(pc);         //--> pc
    stackPtr--; *stackPtr=0xffffffff;                                 //--> lr
    stackPtr--; *stackPtr=0;                                          //--> r12
    stackPtr--; *stackPtr=0;                                          //--> r3
    stackPtr--; *stackPtr=0;                                          //--> r2
    stackPtr--; *stackPtr=reinterpret_cast<unsigned int>(arg1);       //--> r1
    stackPtr--; *stackPtr=reinterpret_cast<unsigned int>(arg0);       //--> r0

    ctxsave[0]=reinterpret_cast<unsigned int>(stackPtr);              //--> psp
    //leaving the content of r4-r11 uninitialized
}

#ifdef WITH_PROCESSES

//
// class FaultData
//

void FaultData::print() const
{
    using namespace fault;
    switch(id)
    {
        case MP:
            iprintf("* Attempted data access @ 0x%x (PC was 0x%x)\n",arg,pc);
            break;
        case MP_NOADDR:
            iprintf("* Invalid data access (PC was 0x%x)\n",pc);
            break;
        case MP_XN:
            iprintf("* Attempted instruction fetch @ 0x%x\n",pc);
            break;
        case UF_DIVZERO:
            iprintf("* Dvide by zero (PC was 0x%x)\n",pc);
            break;
        case UF_UNALIGNED:
            iprintf("* Unaligned memory access (PC was 0x%x)\n",pc);
            break;
        case UF_COPROC:
            iprintf("* Attempted coprocessor access (PC was 0x%x)\n",pc);
            break;
        case UF_EXCRET:
            iprintf("* Invalid exception return sequence (PC was 0x%x)\n",pc);
            break;
        case UF_EPSR:
            iprintf("* Attempted access to the EPSR (PC was 0x%x)\n",pc);
            break;
        case UF_UNDEF:
            iprintf("* Undefined instruction (PC was 0x%x)\n",pc);
            break;
        case UF_UNEXP:
            iprintf("* Unexpected usage fault (PC was 0x%x)\n",pc);
            break;
        case HARDFAULT:
            iprintf("* Hardfault (PC was 0x%x)\n",pc);
            break;
        case BF:
            iprintf("* Busfault @ 0x%x (PC was 0x%x)\n",arg,pc);
            break;
        case BF_NOADDR:
            iprintf("* Busfault (PC was 0x%x)\n",pc);
            break;
        case STACKOVERFLOW:
            iprintf("* Stack overflow\n");
            break;
    }
}

void initCtxsave(unsigned int *ctxsave, void *(*pc)(void *), int argc,
    void *argvSp, void *envp, unsigned int *gotBase, unsigned int *heapEnd)
{
    unsigned int *stackPtr=reinterpret_cast<unsigned int*>(argvSp);
    //Stack is full descending, so decrement first
    stackPtr--; *stackPtr=0x01000000;                                 //--> xPSR
    stackPtr--; *stackPtr=reinterpret_cast<unsigned int>(pc);         //--> pc
    stackPtr--; *stackPtr=0xffffffff;                                 //--> lr
    stackPtr--; *stackPtr=0;                                          //--> r12
    stackPtr--; *stackPtr=reinterpret_cast<unsigned int>(heapEnd);    //--> r3
    stackPtr--; *stackPtr=reinterpret_cast<unsigned int>(envp);       //--> r2
    stackPtr--; *stackPtr=reinterpret_cast<unsigned int>(argvSp);     //--> r1
    stackPtr--; *stackPtr=argc;                                       //--> r0

    ctxsave[0]=reinterpret_cast<unsigned int>(stackPtr);              //--> psp
    ctxsave[6]=reinterpret_cast<unsigned int>(gotBase);               //--> r9
    //leaving the content of r4-r8,r10-r11 uninitialized
    //NOTE: on armv7m without fpu ctxsave does not contain lr
}

#endif //WITH_PROCESSES

void IRQportableStartKernel()
{
    //Enable fault handlers
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk
            | SCB_SHCSR_MEMFAULTENA_Msk;
    //Enable traps for division by zero. Trap for unaligned memory access
    //was removed as gcc starting from 4.7.2 generates unaligned accesses by
    //default (https://www.gnu.org/software/gcc/gcc-4.7/changes.html)
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
    NVIC_SetPriorityGrouping(7);//This should disable interrupt nesting
    NVIC_SetPriority(SVCall_IRQn,3);//High priority for SVC (Max=0, min=15)
    NVIC_SetPriority(MemoryManagement_IRQn,2);//Higher priority for MemoryManagement (Max=0, min=15)

    #ifdef WITH_PROCESSES
    miosix::IRQenableMPUatBoot();
    #endif //WITH_PROCESSES

    //create a temporary space to save current registers. This data is useless
    //since there's no way to stop the sheduler, but we need to save it anyway.
    unsigned int s_ctxsave[miosix::CTXSAVE_SIZE];
    ctxsave=s_ctxsave;//make global ctxsave point to it
    //Note, we can't use enableInterrupts() now since the call is not matched
    //by a call to disableInterrupts()
    __enable_fault_irq();
    __enable_irq();
    miosix::Thread::yield();
    //Never reaches here
}

} //namespace miosix
