/***************************************************************************
 *   Copyright (C) 2010-2024 by Terraneo Federico                          *
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

/**
 * \addtogroup Interfaces
 * \{
 */

/// \internal Size in words of vector to store CPU context during context switch
/// (17*4=68). All ARM7 CPUs have the same number of registers.
const unsigned int CTXSAVE_SIZE=17;

/// \internal Size of additional context saved on the stack during context switch.
/// If zero, this architecture does not save anything on stack during context
/// save. Size is in bytes, not words. MUST be divisible by 4. This constant is
/// used to increase the stack size by the size of context save frame.
const unsigned int CTXSAVE_ON_STACK=0;

/// \internal Stack alignment required by the CPU
const unsigned int CTXSAVE_STACK_ALIGNMENT=4;

/// \internal Offset in words to retrieve the thread stack pointer in ctxsave
const unsigned int STACK_OFFSET_IN_CTXSAVE=13;

/**
 * \}
 */

} //namespace miosix
