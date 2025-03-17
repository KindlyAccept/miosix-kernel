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

namespace miosix {

inline void fastDisableIrq() noexcept
{
    //Since this function is inline there's the need for a memory barrier to
    //avoid aggressive reordering
    asm volatile(".set  I_BIT, 0x80     \n\t"
                 "mrs r0, cpsr          \n\t"
                 "orr r0, r0, #I_BIT    \n\t"
                 "msr cpsr_c, r0        \n\t":::"r0", "memory");
}

inline void fastEnableIrq() noexcept
{
    //Since this function is inline there's the need for a memory barrier to
    //avoid aggressive reordering
    asm volatile(".set  I_BIT, 0x80     \n\t"
                 "mrs r0, cpsr          \n\t"
                 "and r0, r0, #~(I_BIT) \n\t"
                 "msr cpsr_c, r0        \n\t":::"r0", "memory");
}

inline bool areInterruptsEnabled() noexcept
{
    int i;
    asm volatile("mrs %0, cpsr	":"=r" (i));
    if(i & 0x80) return false;
    return true;
}

} //namespace miosix
