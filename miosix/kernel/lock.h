/***************************************************************************
 *   Copyright (C) 2008-2025 by Terraneo Federico                          *
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
 * Acquire the global lock from non-interrupt context. The global lock is a
 * fine-grained lock that is used to protect kernel data structures.
 * You should try to keep the critical sections that hold this lock as short
 * as possible. Holding this lock grants you the capability to call kernel
 * function whose function name starts with the IRQ prefix.
 *
 * globalIrqLock() can be nested, like recursive mutexes. If you call this
 * function multiple times, the lock will be released only when an equal number
 * of globalIrqUnlock() calls is made. This function is also safe to be called
 * before the kernel is started, and in this case it does nothing, since
 * interrupts aren't yet enabled and only one core is running.
 *
 * On single core architectures, the global lock is implemented by disabling
 * interrupts, while on multi core architectures interrupts on the core that
 * acquired the lock are disabled and an implementation-defined mechanism is
 * used to guarantee that only one core at a time can hold the global lock,
 * hence the global name.
 */
void globalIrqLock() noexcept;

/**
 * See the documentation for globalIrqLock()
 */
void globalIrqUnlock() noexcept;

/**
 * This class is a RAII lock for holding the global lock.
 * This class automatically releases the lock when it goes out of scope.
 */
class GlobalIrqLock
{
public:
    /**
     * Constructor, acquire the lock
     */
    GlobalIrqLock() { globalIrqLock(); }

    /**
     * Destructor, release the lock
     */
    ~GlobalIrqLock() { globalIrqUnlock(); }

    GlobalIrqLock(const GlobalIrqLock&)=delete;
    GlobalIrqLock& operator= (const GlobalIrqLock&)=delete;
};

/**
 * This class allows to temporarily release the global lock in a scope it was
 * held using a GlobalIrqLock
 *
 * Example:
 * \code
 * {
 *     GlobalIrqLock dLock;
 *     //Now holding the lock
 *     {
 *         GlobalIrqUnlock eLock(dLock);
 *         //Now lock released
 *     }
 *     //Now holding again the lock
 * }
 * //Finally lock released
 * \endcode
 */
class GlobalIrqUnlock
{
public:
    /**
     * Constructor, release the lock
     * \param l the GlobalIrqLock that was used to acquire the lock
     */
    GlobalIrqUnlock(GlobalIrqLock& l) { (void)l; globalIrqUnlock(); }

    /**
     * Destructor.
     * Acquire back the lock.
     */
    ~GlobalIrqUnlock() { globalIrqLock(); }

    GlobalIrqUnlock(const GlobalIrqUnlock&)=delete;
    GlobalIrqUnlock& operator= (const GlobalIrqUnlock&)=delete;
};

//Forward declarations, these are commented below
inline void fastIrqLock() noexcept;
inline void fastIrqUnlock() noexcept;
inline void fastGlobalLockFromIrq() noexcept;
inline void fastGlobalUnlockFromIrq() noexcept;

/**
 * Acquire the global lock from non-interrupt context. The global lock is a
 * fine-grained lock that is used to protect kernel data structures.
 * You should try to keep the critical sections that hold this lock as short
 * as possible. Holding this lock grants you the capability to call kernel
 * function whose function name starts with the IRQ prefix.
 *
 * NOTE: fastGlobalIrqLock() cannot be nested and cannot be used before the
 * kernel is started! Attempting to do so will lead to undefined behavior.
 * For such cases, use globalIrqLock() instead.
 *
 * On single core architectures, the global lock is implemented by disabling
 * interrupts, while on multi core architectures interrupts on the core that
 * acquired the lock are disabled and an implementation-defined mechanism is
 * used to guarantee that only one core at a time can hold the global lock,
 * hence the global name.
 */
inline void fastGlobalIrqLock() noexcept
{
    fastIrqLock();
    fastGlobalLockFromIrq();
}

/**
 * See the documentation for fastGlobalIrqLock()
 */
inline void fastGlobalIrqUnlock() noexcept
{
    fastGlobalUnlockFromIrq();
    fastIrqUnlock();
}

/**
 * This class is a RAII lock for holding the global lock.
 * This class automatically releases the lock when it goes out of scope.
 */
class FastGlobalIrqLock
{
public:
    /**
     * Constructor, acquire the lock
     */
    FastGlobalIrqLock() { fastGlobalIrqLock(); }

    /**
     * Destructor, release the lock
     */
    ~FastGlobalIrqLock() { fastGlobalIrqUnlock(); }

    FastGlobalIrqLock(const FastGlobalIrqLock&)=delete;
    FastGlobalIrqLock& operator= (const FastGlobalIrqLock&)=delete;
};

/**
 * This class allows to temporarily release the global lock in a scope it was
 * held using a FastGlobalIrqLock
 *
 * Example:
 * \code
 * {
 *     FastGlobalIrqLock dLock;
 *     //Now holding the lock
 *     {
 *         FastGlobalIrqUnlock eLock(dLock);
 *         //Now lock released
 *     }
 *     //Now holding again the lock
 * }
 * //Finally lock released
 * \endcode
 */
class FastGlobalIrqUnlock
{
public:
    /**
     * Constructor, release the lock
     * \param l the GlobalIrqLock that was used to acquire the lock
     */
    FastGlobalIrqUnlock(FastGlobalIrqLock& l) { (void)l; fastGlobalIrqUnlock(); }

    /**
     * Destructor.
     * Acquire back the lock.
     */
    ~FastGlobalIrqUnlock() { fastGlobalIrqLock(); }

    FastGlobalIrqUnlock(const FastGlobalIrqUnlock&)=delete;
    FastGlobalIrqUnlock& operator= (const FastGlobalIrqUnlock&)=delete;
};

/**
 * Acquire the global lock from interrupt context, can only be called inside an
 * interrupt service routine. The global lock is a fine-grained lock that is
 * used to protect kernel data structures. You should try to keep the critical
 * sections that hold this lock as short as possible. Holding this lock grants
 * you the capability to call kernel function whose function name starts with
 * the IRQ prefix.
 *
 * NOTE: fastGlobalLockFromIrq() cannot be nested and cannot be used before the
 * kernel is started (well, interrupts cannot occur before the kernel is started
 * so this can't happen unless you do the mistake of using this lock outside of
 * interrupt context...) Attempting to do so will lead to undefined behavior.
 * There is no nestable version of this lock. Interrupt code should be kept as
 * simple as possible, so you don't need it.
 *
 * On single core architectures, this lock becomes a no-operation as there is
 * no need for it, while on multi core architectures an implementation-defined
 * mechanism is used to guarantee that only one core at a time can hold the
 * global lock even from interrupt context, hence the global name.
 */
#ifdef WITH_SMP
inline void fastGlobalLockFromIrq() noexcept;
#else //WITH_SMP
inline void fastGlobalLockFromIrq() noexcept {} //Not needed in single core CPUs
#endif //WITH_SMP

/**
 * See the documentation for fastGlobalLockFromIrq()
 */
#ifdef WITH_SMP
inline void fastGlobalUnlockFromIrq() noexcept;
#else //WITH_SMP
inline void fastGlobalUnlockFromIrq() noexcept {} //Not needed in single core CPUs
#endif //WITH_SMP

/**
 * This class is a RAII lock for holding the global lock.
 * This class automatically releases the lock when it goes out of scope.
 */
class FastGlobalLockFromIrq
{
public:
    /**
     * Constructor, acquire the lock
     */
    FastGlobalLockFromIrq() { fastGlobalLockFromIrq(); }

    /**
     * Destructor, release the lock
     */
    ~FastGlobalLockFromIrq() { fastGlobalUnlockFromIrq(); }

    FastGlobalLockFromIrq(const FastGlobalLockFromIrq&)=delete;
    FastGlobalLockFromIrq& operator= (const FastGlobalLockFromIrq&)=delete;
};

/**
 * This class allows to temporarily release the global lock in a scope it was
 * held using a FastGlobalLockFromIrq
 *
 * Example:
 * \code
 * {
 *     FastGlobalIrqLock dLock;
 *     //Now holding the lock
 *     {
 *         FastGlobalIrqUnlock eLock(dLock);
 *         //Now lock released
 *     }
 *     //Now holding again the lock
 * }
 * //Finally lock released
 * \endcode
 */
class FastGlobalUnlockFromIrq
{
public:
    /**
     * Constructor, release the lock
     * \param l the GlobalIrqLock that was used to acquire the lock
     */
    FastGlobalUnlockFromIrq(FastGlobalLockFromIrq& l)
    {
        (void)l;
        fastGlobalUnlockFromIrq();
    }

    /**
     * Destructor.
     * Acquire back the lock.
     */
    ~FastGlobalUnlockFromIrq() { fastGlobalLockFromIrq(); }

    FastGlobalUnlockFromIrq(const FastGlobalUnlockFromIrq&)=delete;
    FastGlobalUnlockFromIrq& operator= (const FastGlobalUnlockFromIrq&)=delete;
};

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
void pauseKernel() noexcept;

/**
 * Restart the kernel.<br>This function will yield immediately if a tick has
 * been missed. Since calls to pauseKernel() are cumulative, if you call
 * pauseKernel() two times, you need to call restartKernel() two times.<br>
 * This function is safe to be called even before the kernel is started. In this
 * case it has no effect.
 */
void restartKernel() noexcept;

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
 * paused with a PauseKernelLock.
 *
 * Example:
 * \code
 *
 * //Kernel started
 * {
 *     PauseKernelLock dLock;
 *     //Now kernel paused
 *     {
 *         PauseKernelUnlock eLock(dLock);
 *         //Now kernel back started
 *     }
 *     //Now kernel again paused
 * }
 * //Finally kernel started
 * \endcode
 */
class PauseKernelUnlock
{
public:
    /**
     * Constructor, restarts kernel.
     * \param l the PauseKernelLock that disabled interrupts
     */
    PauseKernelUnlock(PauseKernelLock& l) { (void)l; restartKernel(); }

    /**
     * Destructor.
     * Disable back interrupts.
     */
    ~PauseKernelUnlock() { pauseKernel(); }

    PauseKernelUnlock(const PauseKernelUnlock&)=delete;
    PauseKernelUnlock& operator= (const PauseKernelUnlock&)=delete;
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
void deepSleepLock() noexcept;

/**
 * Used to signal the scheduler that a critical section where deep sleep should
 * not be entered has completed. If N calls to deepSleepLock() are made, then N
 * calls to deepSleepUnlock() need to be made before deep sleep is enabled back.
 */
void deepSleepUnlock() noexcept;

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

    DeepSleepLock(const DeepSleepLock&)=delete;
    DeepSleepLock& operator= (const DeepSleepLock&)=delete;
};

/**
 * \internal
 * Disable interrupts only on the core it is called from.
 *
 * This is currently meant to be an implementation detail used to implement the
 * global lock and we don't expect it to be useful for other uses, that's why
 * we're currently not providing RAII-style lock classes, though this may change
 * in the future if some use case for this function is found.
 * If you're trying to use this for driver development maybe you're looking for
 * fastGlobalIrqLock()?
 */
inline void fastIrqLock() noexcept;

/**
 * \internal
 * Enable back interrupts on the core it was called from, after they have been
 * disabled by a call to fastIrqLock().
 *
 * This is currently meant to be an implementation detail used to implement the
 * global lock and we don't expect it to be useful for other uses, that's why
 * we're currently not providing RAII-style lock classes, though this may change
 * in the future if some use case for this function is found.
 * If you're trying to use this for driver development maybe you're looking for
 * fastGlobalIrqUnlock()?
 */
inline void fastIrqUnlock() noexcept;

/**
 * \internal
 * Provides a way to know if interrupts are enabled or not.
 *
 * \return true if interrupts are enabled
 * \warning Using this function is discouraged
 */
inline bool areInterruptsEnabled() noexcept;

} //namespace miosix


#include "interfaces-impl/lock_impl.h"

#ifdef WITH_SMP
#include "interfaces-impl/lock_smp_impl.h"
#endif
