/***************************************************************************
 *   Copyright (C) 2012 - 2024 by Terraneo Federico and Luigi Rucco        *
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

#include <map>
#include <utility>

#ifndef TEST_ALLOC
#include <miosix.h>
#else //TEST_ALLOC
#include <iostream>
#include <typeinfo>
#include <sstream>
#endif //TEST_ALLOC

#ifdef WITH_PROCESSES

namespace miosix {

/**
 * This class allows to handle a memory area reserved for the allocation of
 * processes' images. This memory area is called process pool.
 */
class ProcessPool
{
public:
    /**
     * \return an instance of the process pool (singleton)
     */
    static ProcessPool& instance();
    
    /**
     * Allocate memory inside the process pool.
     * \param size size in bytes (despite the returned pointer is an
     * unsigned int*) of the requested memory
     * \return a pair with the pointer to the allocated memory and the actual
     * allocated size, which could be greater or equal than the requested size
     * to accomodate limitations in the allocator and memory protection unit.
     * Note that due to memory protection unit limitations the pointer is
     * size-aligned, so that for example if a 16KByte block is requested,
     * the returned pointer is aligned on a 16KB boundary.
     * \throws bad_alloc if out of memory
     */
    std::pair<unsigned int *, unsigned int> allocate(unsigned int size);
    
    /**
     * Deallocate a memory block.
     * \param ptr pointer to deallocate.
     * \throws runtime_error if the pointer is invalid
     */
    void deallocate(unsigned int *ptr);
    
    #ifdef TEST_ALLOC
    /**
     * Print the state of the allocator, used for debugging
     */
    void printAllocatedBlocks();
    #endif //TEST_ALLOC
    
private:
    ProcessPool(const ProcessPool&);
    ProcessPool& operator= (const ProcessPool&);
    
    /**
     * Constructor.
     * \param poolBase address of the start of the process pool.
     * \param poolSize size of the process pool. Must be a multiple of blockSize
     */
    ProcessPool(unsigned int *poolBase, unsigned int poolSize);
    
    /**
     * Destructor
     */
    ~ProcessPool();
    
    /**
     * \param bit bit to test, from 0 to poolSize/blockSize
     * \return true if the bit is set
     */
    bool testBit(unsigned int bit)
    {
        return (bitmap[bit/(sizeof(unsigned int)*8)] &
            1<<(bit % (sizeof(unsigned int)*8))) ? true : false;
    }
    
    /**
     * \param bit bit to set, from 0 to poolSize/blockSize
     */
    void setBit(unsigned int bit)
    {
        bitmap[(bit/(sizeof(unsigned int)*8))] |= 
            1<<(bit % (sizeof(unsigned int)*8));
    }
    
    /**
     * \param bit bit to clear, from 0 to poolSize/blockSize
     */
    void clearBit(unsigned int bit)
    {
        bitmap[bit/(sizeof(unsigned int)*8)] &= 
            ~(1<<(bit % (sizeof(unsigned int)*8)));
    }
    
    unsigned int *bitmap;   ///< Pointer to the status of the allocator
    unsigned int *poolBase; ///< Base address of the entire pool
    unsigned int poolSize;  ///< Size of the pool, in bytes
    ///Lists all allocated blocks, allows to retrieve their sizes
    std::map<unsigned int*,unsigned int> allocatedBlocks;
    #ifndef TEST_ALLOC
    miosix::FastMutex mutex; ///< Mutex to guard concurrent access
    #endif //TEST_ALLOC
};

} //namespace miosix

#endif //WITH_PROCESSES
