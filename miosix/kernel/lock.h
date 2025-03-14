/***************************************************************************
 *   Copyright (C) 2025 by Terraneo Federico                               *
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

#include "config/miosix_settings.h"
#include "interfaces/interrupts.h"

namespace miosix {

/**
 * Disable interrupts, if interrupts were enable prior to calling this function.
 * 
 * Please note that starting from Miosix 1.51 disableInterrupts() and
 * enableInterrupts() can be nested. You can therefore call disableInterrupts()
 * multiple times as long as each call is matched by a call to
 * enableInterrupts().<br>
 * This replaced disable_and_save_interrupts() and restore_interrupts()
 *
 * disableInterrupts() cannot be called within an interrupt routine, but can be
 * called before the kernel is started (and does nothing in this case)
 */
void disableInterrupts();

/**
 * Enable interrupts.<br>
 * Please note that starting from Miosix 1.51 disableInterrupts() and
 * enableInterrupts() can be nested. You can therefore call disableInterrupts()
 * multiple times as long as each call is matched by a call to
 * enableInterrupts().<br>
 * This replaced disable_and_save_interrupts() and restore_interrupts()
 *
 * enableInterrupts() cannot be called within an interrupt routine, but can be
 * called before the kernel is started (and does nothing in this case)
 */
void enableInterrupts();

/**
 * This class is a RAII lock for disabling interrupts. This call avoids
 * the error of not reenabling interrupts since it is done automatically.
 */
class InterruptDisableLock
{
public:
    /**
     * Constructor, disables interrupts.
     */
    InterruptDisableLock()
    {
        disableInterrupts();
    }

    /**
     * Destructor, reenables interrupts
     */
    ~InterruptDisableLock()
    {
        enableInterrupts();
    }

private:
    //Unwanted methods
    InterruptDisableLock(const InterruptDisableLock& l);
    InterruptDisableLock& operator= (const InterruptDisableLock& l);
};

/**
 * This class allows to temporarily re enable interrpts in a scope where
 * they are disabled with an InterruptDisableLock.<br>
 * Example:
 * \code
 *
 * //Interrupts enabled
 * {
 *     InterruptDisableLock dLock;
 *
 *     //Now interrupts disabled
 *
 *     {
 *         InterruptEnableLock eLock(dLock);
 *
 *         //Now interrupts back enabled
 *     }
 *
 *     //Now interrupts again disabled
 * }
 * //Finally interrupts enabled
 * \endcode
 */
class InterruptEnableLock
{
public:
    /**
     * Constructor, enables back interrupts.
     * \param l the InteruptDisableLock that disabled interrupts. Note that
     * this parameter is not used internally. It is only required to prevent
     * erroneous use of this class by making an instance of it without an
     * active InterruptEnabeLock
     */
    InterruptEnableLock(InterruptDisableLock& l)
    {
        (void)l;
        enableInterrupts();
    }

    /**
     * Destructor.
     * Disable back interrupts.
     */
    ~InterruptEnableLock()
    {
        disableInterrupts();
    }

private:
    //Unwanted methods
    InterruptEnableLock(const InterruptEnableLock& l);
    InterruptEnableLock& operator= (const InterruptEnableLock& l);
};

/// Retrocompatbility alias. Do not use in new code.
/// TODO: Remove!
using FastInterruptDisableLock = GlobalInterruptLock;
/// Retrocompatbility alias. Do not use in new code.
/// TODO: Remove!
using FastInterruptEnableLock = GlobalInterruptUnlock;

/**
 * Pause the kernel.<br>Interrupts will continue to occur, but no preemption is
 * possible. Call to this function are cumulative: if you call pauseKernel()
 * two times, you need to call restartKernel() two times.<br>Pausing the kernel
 * must be avoided if possible because it is easy to cause deadlock. Calling
 * file related functions (fopen, Directory::open() ...), serial port related
 * functions (printf ...) or kernel functions that cannot be called when the
 * kernel is paused will cause deadlock. Therefore, if possible, it is better to
 * use a Mutex instead of pausing the kernel<br>This function is safe to be
 * called even before the kernel is started. In this case it has no effect.
 */
void pauseKernel();

/**
 * Restart the kernel.<br>This function will yield immediately if a tick has
 * been missed. Since calls to pauseKernel() are cumulative, if you call
 * pauseKernel() two times, you need to call restartKernel() two times.<br>
 * This function is safe to be called even before the kernel is started. In this
 * case it has no effect.
 */
void restartKernel();

/**
 * This class is a RAII lock for pausing the kernel. This call avoids
 * the error of not restarting the kernel since it is done automatically.
 */
class PauseKernelLock
{
public:
    /**
     * Constructor, pauses the kernel.
     */
    PauseKernelLock()
    {
        pauseKernel();
    }

    /**
     * Destructor, restarts the kernel
     */
    ~PauseKernelLock()
    {
        restartKernel();
    }

private:
    //Unwanted methods
    PauseKernelLock(const PauseKernelLock& l);
    PauseKernelLock& operator= (const PauseKernelLock& l);
};

/**
 * This class allows to temporarily restart kernel in a scope where it is
 * paused with an InterruptDisableLock.<br>
 * Example:
 * \code
 *
 * //Kernel started
 * {
 *     PauseKernelLock dLock;
 *
 *     //Now kernel paused
 *
 *     {
 *         RestartKernelLock eLock(dLock);
 *
 *         //Now kernel back started
 *     }
 *
 *     //Now kernel again paused
 * }
 * //Finally kernel started
 * \endcode
 */
class RestartKernelLock
{
public:
    /**
     * Constructor, restarts kernel.
     * \param l the PauseKernelLock that disabled interrupts. Note that
     * this parameter is not used internally. It is only required to prevent
     * erroneous use of this class by making an instance of it without an
     * active PauseKernelLock
     */
    RestartKernelLock(PauseKernelLock& l)
    {
        (void)l;
        restartKernel();
    }

    /**
     * Destructor.
     * Disable back interrupts.
     */
    ~RestartKernelLock()
    {
        pauseKernel();
    }

private:
    //Unwanted methods
    RestartKernelLock(const RestartKernelLock& l);
    RestartKernelLock& operator= (const RestartKernelLock& l);
};

/**
 * Prevent the microcontroller from entering a deep sleep state. Most commonly
 * used by device drivers requiring clocks or power rails that would be disabled
 * when entering deep sleep to perform blocking operations while informing the
 * scheduler that deep sleep is currently not possible.
 * Can be nested multiple times and called by different device drivers
 * simultaneously. If N calls to deepSleepLock() are made, then N calls to
 * deepSleepUnlock() need to be made before deep sleep is enabled back.
 */
void deepSleepLock();

/**
 * Used to signal the scheduler that a critical section where deep sleep should
 * not be entered has completed. If N calls to deepSleepLock() are made, then N
 * calls to deepSleepUnlock() need to be made before deep sleep is enabled back.
 */
void deepSleepUnlock();

/**
 * This class is a RAII lock for temporarily prevent entering deep sleep.
 * This call avoids the error of not reenabling deep sleep capability since it
 * is done automatically.
 */
class DeepSleepLock
{
public:       
    DeepSleepLock() { deepSleepLock(); }

    ~DeepSleepLock() { deepSleepUnlock(); }

private: 
    DeepSleepLock(const DeepSleepLock&);
    DeepSleepLock& operator= (const DeepSleepLock&);
};

} //namespace miosix
