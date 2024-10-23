/***************************************************************************
 *   Copyright (C) 2008-2024 by Terraneo Federico                          *
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

#pragma once

/**
 * \addtogroup Interfaces
 * \{
 */

/**
 * \internal
 * \def saveContextFromSwi()
 * Save context from a software interrupt
 * It is used by the kernel, and should not be used by end users.
 */
#define saveContextFromSwi()                                                 \
    asm volatile(/*push lr on stack, to use it as a general purpose reg.*/   \
                 "stmfd  sp!,{lr}        \n\t"                               \
                 /*load ctxsave and dereference the pointer*/                \
                 "ldr    lr,=ctxsave     \n\t"                               \
                 "ldr    lr,[lr]         \n\t"                               \
                 /*save all thread registers except pc*/                     \
                 "stmia  lr,{r0-lr}^     \n\t"                               \
                 /*add a nop as required after stm ^ (ARM reference stm 2)*/ \
                 "nop                    \n\t"                               \
                 /*move lr to r0, restore original lr and save it*/          \
                 "add    r0,lr,#60       \n\t"                               \
                 "ldmfd  sp!,{lr}        \n\t"                               \
                 "stmia  r0!,{lr}        \n\t"                               \
                 /*save spsr on top of ctxsave*/                             \
                 "mrs    r1,spsr         \n\t"                               \
                 "stmia  r0,{r1}         \n\t");

/**
 * \def saveContextFromIrq()
 * Save context from an IRQ<br>
 * Must be the first line of an IRQ where a context switch can happen.
 * The IRQ must be "naked" to prevent the compiler from generating context save.
 */
#define saveContextFromIrq()                                                 \
    asm volatile(/*Adjust lr, return address in a ISR has a 4 bytes offset*/ \
                 "sub    lr,lr,#4        \n\t");                             \
    saveContextFromSwi();

/**
 * \def restoreContext()
 * Restore context in an IRQ where saveContextFromIrq()
 * (or saveContextFromSwi() ) is used. Must be the last line of an IRQ where
 * a context switch can happen. The IRQ must be "naked" to prevent the compiler
 * from generating context restore.
 */
#define restoreContext()                                                     \
    asm volatile(/*load ctxsave and dereference the pointer*/                \
                 /*also add 64 to make it point to "top of stack"*/          \
                 "ldr   lr,=ctxsave     \n\t"                                \
                 "ldr   lr,[lr]         \n\t"                                \
                 "add   lr,lr,#64       \n\t"                                \
                 /*restore spsr*/                                            \
                 /*after this instructions, lr points to ctxsave[15]*/       \
                 "ldmda lr!,{r1}        \n\t"                                \
                 "msr   spsr,r1         \n\t"                                \
                 /*restore all thread registers except pc*/                  \
                 "ldmdb lr,{r0-lr}^     \n\t"                                \
                 /*add a nop as required after ldm ^ (ARM reference ldm 2)*/ \
                 "nop                   \n\t"                                \
                 /*lr points to return address, return from interrupt*/      \
                 "ldr   lr,[lr]         \n\t"                                \
                 "movs  pc,lr           \n\t");

/**
 * Enable interrupts (both irq and fiq)<br>
 * If you are not using FIQ you should use disableInterrupts()
 * FIQ means fast interrupts, is another level of interrupts available in the
 * ARM7 cpu. They are not used in miosix, and are available to the user.
 * The main advantage of FIQ is that they can even interrupt IRQ, so they have
 * a so high priority that can interrupt the kernel itself.<br>The disadvantage
 * is that, since can interrupt the kernel at any time, all functions, including
 * those marked as IRQ cannot be called when IRQ and FIQ are disabled.
 * Therefore, data transfer between user code and FIQ is more difficult to
 * implement than IRQ. Another disadvantage is that they are only available in
 * the ARM cpu, so if the kernel will be ported to another cpu, they won't be
 * available.<br>
 * To use FIQ the user must change the code of the default FIQ interrupt routine
 * defined in miosix/drivers/interrupts.cpp<br>By default FIQ are enabled but no
 * peripheral is associated with FIQ, so no FIQ interrupts will occur.
 */
#define enableIRQandFIQ()                                                     \
    asm volatile(".set  I_BIT, 0x80           \n\t"                           \
                 ".set  F_BIT, 0x40           \n\t"                           \
                 "mrs r0, cpsr                \n\t"                           \
                 "and r0, r0, #~(I_BIT|F_BIT) \n\t"                           \
                 "msr cpsr_c, r0              \n\t"                           \
                 :::"r0");

///Disable interrupts (both irq and fiq)<br>
///If you are not using FIQ you should use enableInterrupts()
#define disableIRQandFIQ()                                                    \
    asm volatile(".set  I_BIT, 0x80           \n\t"                           \
                 ".set  F_BIT, 0x40           \n\t"                           \
                 "mrs r0, cpsr                \n\t"                           \
                 "orr r0, r0, #I_BIT|F_BIT    \n\t"                           \
                 "msr cpsr_c, r0              \n\t"                           \
                 :::"r0");

namespace miosix {

inline void doYield()
{
    #error TODO update yield code with PendSV equivalent for ARM7
    asm volatile("movs  r3, #0\n\t"
                 "swi   0"
                 :::"r3");
}

/**
 * \internal
 * Allows to retrieve the saved stack pointer in a portable way as
 * ctxsave[stackPtrOffsetInCtxsave]
 *
 * In this architecture, registers are saved in the following order:
 * *ctxsave+64 --> cpsr
 * *ctxsave+60 --> pc (return address)
 * *ctxsave+56 --> lr
 * *ctxsave+52 --> sp
 * *ctxsave+48 --> r12
 * *ctxsave+44 --> r11
 * *ctxsave+40 --> r10
 * *ctxsave+36 --> r9
 * *ctxsave+32 --> r8
 * *ctxsave+28 --> r7
 * *ctxsave+24 --> r6
 * *ctxsave+20 --> r5
 * *ctxsave+16 --> r4
 * *ctxsave+12 --> r3
 * *ctxsave+8  --> r2
 * *ctxsave+4  --> r1
 * *ctxsave+0  --> r0
 */
const int stackPtrOffsetInCtxsave=13;

} //namespace miosix

/**
 * \}
 */
