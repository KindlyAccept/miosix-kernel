/***************************************************************************
 *   Copyright (C) 2010, 2011, 2012 by Terraneo Federico                   *
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

#include "priority_scheduler.h"
#include "kernel/error.h"
#include "kernel/process.h"
#include "interfaces_private/cpu.h"
#include "interfaces_private/os_timer.h"
#include <limits>

#ifdef SCHED_TYPE_PRIORITY
namespace miosix {

//These are defined in thread.cpp
extern volatile Thread *runningThread;
extern volatile int kernelRunning;
extern volatile bool pendingWakeup;
extern IntrusiveList<SleepData> sleepingList;

//Internal data
static long long nextPeriodicPreemption=std::numeric_limits<long long>::max();

//
// class PriorityScheduler
//

bool PriorityScheduler::PKaddThread(Thread *thread,
        PrioritySchedulerPriority priority)
{
    thread->schedData.priority=priority;
    threadList[priority.get()].push_back(thread);
    return true;
}

bool PriorityScheduler::PKexists(Thread *thread)
{
    if(thread==runningThread) return true; //Running thread is not in any list
    for(int i=PRIORITY_MAX-1;i>=0;i--)
        for(auto t : threadList[i]) if(t==thread) return !t->flags.isDeleted();
    return false;
}

void PriorityScheduler::PKremoveDeadThreads()
{
    for(int i=PRIORITY_MAX-1;i>=0;i--)
    {
        auto t=threadList[i].begin(), e=threadList[i].end();
        while(t!=e)
        {
            if((*t)->flags.isDeleted())
            {
                void *base=(*t)->watermark;
                (*t)->~Thread();//Call destructor manually because of placement new
                free(base);  //Delete ALL thread memory
                t=threadList[i].erase(t);
            } else ++t;
        }
    }
}

void PriorityScheduler::PKsetPriority(Thread *thread,
        PrioritySchedulerPriority newPriority)
{
    if(thread==runningThread)
    {
        //Thread is running so is not in any list, only change priority value
        thread->schedData.priority=newPriority;
    } else {
        //Remove the thread from its old list
        threadList[thread->schedData.priority.get()].removeFast(thread);
        //Set priority to the new value
        thread->schedData.priority=newPriority;
        //Last insert the thread in the new list
        threadList[newPriority.get()].push_back(thread);
    }
}

void PriorityScheduler::IRQsetIdleThread(Thread *idleThread)
{
    idleThread->schedData.priority=-1;
    idle=idleThread;
}

long long PriorityScheduler::IRQgetNextPreemption()
{
    return nextPeriodicPreemption;
}

static long long IRQsetNextPreemption(bool runningIdleThread)
{
    long long first;
    if(sleepingList.empty()) first=std::numeric_limits<long long>::max();
    else first=sleepingList.front()->wakeupTime;

    long long t=IRQgetTime();
    if(runningIdleThread) nextPeriodicPreemption=first;
    else nextPeriodicPreemption=std::min(first,t+MAX_TIME_SLICE);

    //We could not set an interrupt if the sleeping list is empty and runningThread
    //is idle but there's no such hurry to run idle anyway, so why bother?
    IRQosTimerSetInterrupt(nextPeriodicPreemption);
    return t;
}

void PriorityScheduler::IRQrunScheduler()
{
    if(kernelRunning!=0) //If kernel is paused, do nothing
    {
        pendingWakeup=true;
        return;
    }
    //Add the previous thread to the back of the priority list (round-robin)
    Thread *prev=const_cast<Thread*>(runningThread);
    int prevPriority=prev->schedData.priority.get();
    if(prevPriority!=-1) threadList[prevPriority].push_back(prev);
    for(int i=PRIORITY_MAX-1;i>=0;i--)
    {
        for(auto next : threadList[i])
        {
            if(next->flags.isReady()==false) continue;
            //Found a READY thread, so run this one
            runningThread=next;
            #ifdef WITH_PROCESSES
            if(next->flags.isInUserspace()==false)
            {
                ctxsave=next->ctxsave;
                MPUConfiguration::IRQdisable();
            } else {
                ctxsave=next->userCtxsave;
                //A kernel thread is never in userspace, so the cast is safe
                static_cast<Process*>(next->proc)->mpu.IRQenable();
            }
            #else //WITH_PROCESSES
            ctxsave=next->ctxsave;
            #endif //WITH_PROCESSES
            #ifndef WITH_CPU_TIME_COUNTER
            IRQsetNextPreemption(false);
            #else //WITH_CPU_TIME_COUNTER
            auto t=IRQsetNextPreemption(false);
            IRQprofileContextSwitch(prev->timeCounterData,next->timeCounterData,t);
            #endif //WITH_CPU_TIME_COUNTER
            //Remove the selected thread from the list. This invalidates
            //iterators sho it should be done last
            threadList[i].removeFast(next);
            return;
        }
    }
    //No thread found, run the idle thread
    runningThread=idle;
    ctxsave=idle->ctxsave;
    #ifdef WITH_PROCESSES
    MPUConfiguration::IRQdisable();
    #endif //WITH_PROCESSES
    #ifndef WITH_CPU_TIME_COUNTER
    IRQsetNextPreemption(true);
    #else //WITH_CPU_TIME_COUNTER
    auto t=IRQsetNextPreemption(true);
    IRQprofileContextSwitch(prev->timeCounterData,idle->timeCounterData,t);
    #endif //WITH_CPU_TIME_COUNTER
}

IntrusiveList<Thread> PriorityScheduler::threadList[PRIORITY_MAX];
Thread *PriorityScheduler::idle=nullptr;

} //namespace miosix

#endif //SCHED_TYPE_PRIORITY
