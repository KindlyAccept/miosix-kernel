/***************************************************************************
 *   Copyright (C) 2013-2022 by Terraneo Federico and Silvano Seva         *
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

// 包含给STM32F2/F4的I2C驱动的头文件 
#include "stm32f2_f4_i2c.h"
// 包含Miosix操作系统的基本功能
#include <miosix.h>
// 包含调度器相关功能
    // 为什么需要调度器？
        // 调度器是Miosix操作系统中的一个核心组件，用于管理多个线程的执行。
        // 它负责决定哪个线程应该在CPU上运行，以及何时切换到其他线程。
        // 调度器确保所有线程都能公平地使用CPU时间，并根据它们的优先级进行调度。
        // 调度器在多任务环境中非常重要，因为它允许程序在等待I/O操作或长时间运行的函数时继续执行其他任务。
#include <kernel/scheduler/scheduler.h>
#include "stm32f407xx.h"

// 使用miosix命名空间
using namespace miosix;

// 当I2C操作发生错误时设置为true
    // 为什么需要这个变量？
        // 这个变量用于跟踪I2C操作是否发生错误。
        // 当I2C操作发生错误时，设置为true，表示操作失败。
        // 这样可以避免重复尝试相同的操作，或者在错误发生后采取其他措施。
    // 改进方案：
        // 可以考虑使用一个类来管理I2C操作，并提供一个成员变量来跟踪错误。
        // 这样可以更好地组织代码，并提供更好的错误处理。
static volatile bool error;     ///< 在错误发生时由IRQ设置为true
// 指向当前等待I2C操作完成的线程
    // 为什么需要这个变量？
        // 这个变量用于跟踪当前等待I2C操作完成的线程。
        // 当I2C操作完成时，调度器会唤醒等待的线程，并根据它们的优先级进行调度。
static Thread *waiting=nullptr; ///< 等待操作完成的线程

/**
 * DMA I2C接收传输结束
 * 这是中断处理函数的入口点，使用naked属性表示不需要编译器生成函数序言和结尾
 *  naked 作用是：
 *  1. 不生成函数序言和结尾，避免不必要的代码
 *  2. 直接使用汇编代码，避免C语言的函数调用开销
 *  3. 确保中断处理函数在所有寄存器和上下文都保存和恢复后执行
 */
void __attribute__((naked)) DMA1_Stream0_IRQHandler()
{
    // 保存当前上下文（寄存器等）
    saveContext();
    // 调用实际的处理函数
        // 这个函数在I2C1rxDmaHandlerImpl中实现
    asm volatile("bl _Z20I2C1rxDmaHandlerImplv");
    // 恢复上下文并返回
    restoreContext();
}

/**
 * DMA I2C接收传输结束的实际实现
 * 此函数处理DMA接收完成后的操作
 *  used 作用是：
 *  1. 确保函数在链接时被使用，避免被优化器删除
 */
void __attribute__((used)) I2C1rxDmaHandlerImpl()
{
    // 清除DMA流0的所有中断标志（传输完成、传输错误、直接模式错误和FIFO错误）
    DMA1->LIFCR=DMA_LIFCR_CTCIF0
              | DMA_LIFCR_CTEIF0
              | DMA_LIFCR_CDMEIF0
              | DMA_LIFCR_CFEIF0;
    // 如果没有等待的线程，直接返回
    if(waiting==nullptr) return;
    // 唤醒等待的线程
    waiting->IRQwakeup();
    // 如果等待线程的优先级高于当前线程，触发调度器寻找下一个要运行的线程
    if(waiting->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();
    // 清除等待线程指针
    waiting=nullptr;
}

/**
 * DMA I2C发送传输结束
 * 处理DMA传输完成的中断
 */
void DMA1_Stream7_IRQHandler()
{
    // 清除DMA流7的所有中断标志
    DMA1->HIFCR=DMA_HIFCR_CTCIF7
              | DMA_HIFCR_CTEIF7
              | DMA_HIFCR_CDMEIF7
              | DMA_HIFCR_CFEIF7;
    // 我们不能直接唤醒线程，因为I2C是双缓冲的，这个中断在倒数第二个字节开始发送时触发
    // 如果现在返回，主代码会过早发送停止条件，最后一个字节就永远不会被发送
    // 相反，我们从DMA模式切换到IRQ模式，这样当倒数第二个字节发送完成时
    // 触发中断并发送最后一个字节
    // 注意，由于没有从这个IRQ唤醒线程，所以不需要saveContext()、restoreContext()和__attribute__((naked))
    
    // 禁用I2C的DMA模式
    I2C1->CR2 &= ~I2C_CR2_DMAEN;
    // 启用I2C缓冲器和事件中断，处理最后一个字节的传输
    I2C1->CR2 |= I2C_CR2_ITBUFEN | I2C_CR2_ITEVTEN;
}

/**
 * I2C地址发送中断
 * 这是处理I2C事件中断的入口点
 */
void __attribute__((naked)) I2C1_EV_IRQHandler()
{
    // 保存当前上下文
    saveContext();
    // 调用实际的处理函数
    asm volatile("bl _Z15I2C1HandlerImplv");
    // 恢复上下文并返回
    restoreContext();
}

/**
 * I2C地址发送中断的实际实现
 * 处理I2C事件中断
 */
void __attribute__((used)) I2C1HandlerImpl()
{
    // 当被调用解决最后一个字节未发送问题时，清除I2C_CR2_ITBUFEN防止此中断无限重入
    // 因为它不会向I2C发送另一个字节，所以中断会保持挂起状态
    // 当在起始位发送后调用时，清除I2C_CR2_ITEVTEN可防止相同的无限重入
    // 因为此中断不会开始地址传输，这是停止此中断挂起所必需的
    
    // 禁用I2C缓冲器和事件中断，防止无限重入
    I2C1->CR2 &= ~(I2C_CR2_ITBUFEN | I2C_CR2_ITEVTEN);
    // 如果没有等待的线程，直接返回
    if(waiting==nullptr) return;
    // 唤醒等待的线程
    waiting->IRQwakeup();
    // 如果等待线程优先级更高，调度器寻找下一个线程
    if(waiting->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();
    // 清除等待线程指针
    waiting=nullptr;
}

/**
 * I2C错误中断
 * 这是处理I2C错误中断的入口点
 */
void __attribute__((naked)) I2C1_ER_IRQHandler()
{
    // 保存当前上下文
    saveContext();
    // 调用实际的处理函数
    asm volatile("bl _Z18I2C1errHandlerImplv");
    // 恢复上下文并返回
    restoreContext();
}

/**
 * I2C错误中断的实际实现
 * 处理I2C错误中断
 */
void __attribute__((used)) I2C1errHandlerImpl()
{
    // 清除所有错误标志
    I2C1->SR1=0; // 清除错误标志
    // 设置错误标志
    error=true;
    // 如果没有等待的线程，直接返回
    if(waiting==nullptr) return;
    // 唤醒等待的线程
    waiting->IRQwakeup();
    // 如果等待线程优先级更高，调度器寻找下一个线程
    if(waiting->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();
    // 清除等待线程指针
    waiting=nullptr;
}

namespace miosix {

//
// class I2C
//

/**
 * I2C1Master构造函数
 * 初始化I2C控制器和相关GPIO
 * @param sda SDA信号对应的GPIO引脚
 * @param scl SCL信号对应的GPIO引脚
 * @param frequency I2C通信频率(kHz)
 */
I2C1Master::I2C1Master(GpioPin sda, GpioPin scl, int frequency)
{
    // 检查是否已有此类的实例，避免重复创建
    if(checkMultipleInstances) errorHandler(UNEXPECTED);
    // 设置标志，表示已创建实例
    checkMultipleInstances=true;

    // I2C设备连接到APB1，其频率是系统时钟除以RCC->CFGR中PPRE1位设置的值
    const int ppre1=(RCC->CFGR & RCC_CFGR_PPRE1)>>10; // 获取PPRE1位的值
    const int divFactor= (ppre1 & 1<<2) ? (2<<(ppre1 & 0x3)) : 1; // 计算分频因子
    const int fpclk1=SystemCoreClock/divFactor; // 计算APB1的时钟频率
    //iprintf("fpclk1=%d\n",fpclk1);
    
    {
        // 临界区开始，禁用中断
        FastInterruptDisableLock dLock;
        // 注意：在启用外设之前需要配置GPIO，否则第一次读/写调用会永远阻塞
        // 这可能是硬件bug
        // 注意：使用ALTERNATE_OD因为I2C外设不强制开漏模式
        
        // 配置SDA引脚为复用功能模式4
        sda.alternateFunction(4);
        // 设置SDA为开漏输出模式
        sda.mode(Mode::ALTERNATE_OD);
        // 配置SCL引脚为复用功能模式4
        scl.alternateFunction(4);
        // 设置SCL为开漏输出模式
        scl.mode(Mode::ALTERNATE_OD);
        // 启用DMA1时钟
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
        // 启用I2C1外设时钟
        RCC->APB1ENR |= RCC_APB1ENR_I2C1EN; // 启用时钟门控
        // 同步RCC时钟配置
        RCC_SYNC();
    }
    
    // 设置DMA1 Stream7的中断优先级为低优先级(10)
    NVIC_SetPriority(DMA1_Stream7_IRQn,10); // DMA低优先级
    // 清除DMA1 Stream7的中断挂起标志
    NVIC_ClearPendingIRQ(DMA1_Stream7_IRQn); // DMA1 stream 7 channel 1 = I2C1 TX 
    // 启用DMA1 Stream7中断
    NVIC_EnableIRQ(DMA1_Stream7_IRQn);
    
    // 设置DMA1 Stream0的中断优先级为低优先级(10)
    NVIC_SetPriority(DMA1_Stream0_IRQn,10); // DMA低优先级
    // 清除DMA1 Stream0的中断挂起标志
    NVIC_ClearPendingIRQ(DMA1_Stream0_IRQn); // DMA1 stream 0 channel 1 = I2C1 RX 
    // 启用DMA1 Stream0中断
    NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    
    // 设置I2C1事件中断的优先级为低优先级(10)
    NVIC_SetPriority(I2C1_EV_IRQn,10); // I2C低优先级
    // 清除I2C1事件中断挂起标志
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    // 启用I2C1事件中断
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    
    // 设置I2C1错误中断的优先级为低优先级(10)
    NVIC_SetPriority(I2C1_ER_IRQn,10);
    // 清除I2C1错误中断挂起标志
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    // 启用I2C1错误中断
    NVIC_EnableIRQ(I2C1_ER_IRQn);

    // 复位I2C1外设
    I2C1->CR1=I2C_CR1_SWRST;
    // 清除复位标志
    I2C1->CR1=0;
    // 设置I2C时钟频率（以MHz为单位）
    I2C1->CR2=fpclk1/1000000; // 以MHz为单位设置pclk频率

    // 限制频率在合理范围内，但官方仅支持100和400
    frequency=std::max(10,std::min(1000,frequency)); // 限制范围在10-1000kHz
    if(frequency>100) // 如果频率大于100kHz，使用快速模式（Fast Mode）
    {
        // 计算时钟控制寄存器值并设置快速模式标志
        I2C1->CCR=std::max(4,fpclk1/(3000*frequency)) | I2C_CCR_FS;
        /* 
         * TRISE设置最大SCL上升时间。根据I2C规范：
         * 400KHz (2.5us) I2C的最大上升时间为300ns，比例为8.333
         * 1MHz (1us) I2C的最大上升时间为120ns，比例也是8.333
         * 虽然官方不支持高于400kHz的频率，但为了允许一些超频，
         * 我们将使用"8.333规则"设置TRISE。
         * I2Period[s] = 1 / I2CFrequency[Hz]
         * RISETIME[s] = I2CPeriod[s] / 8.333
         * K = 1 / RISETIME
         * TRISE = (fpclk1/K)+1
         * 综合起来，
         * K = I2CFrequency[Hz] * 8.333 = I2CFrequency[kHz] * 8333
         */
        // 设置上升时间寄存器
        I2C1->TRISE=fpclk1/(frequency*8333)+1;
    } else { // 标准模式（Standard Mode）
        // 禁用全速模式时，需要除以2而不是3
        I2C1->CCR=std::max(4,fpclk1/(2000*frequency));
        // 100kHz I2C的上升时间为1000ns，不遵循8.333规则
        I2C1->TRISE=fpclk1/1000000+1;
    }

    // 启用I2C外设
    I2C1->CR1=I2C_CR1_PE; // 启用外设
}

/**
 * 接收数据
 * @param address 设备地址
 * @param data 接收数据的缓冲区
 * @param len 要接收的字节数
 * @return 操作是否成功
 */
bool I2C1Master::recv(unsigned char address, void *data, int len)
{
    // 检查长度是否有效
    if(len<=0 || len>0xffff) return false;
    // 设置地址最低位为1，表示读操作
    address |= 0x01;
    // 使用特殊的启动方式，如果失败或者方向错误则返回错误
    if(startWorkaround(address,len)==false || I2C1->SR2 & I2C_SR2_TRA)
    {
        // 重新启用中断，结束临界区
        fastEnableInterrupts(); // 破解解决方案临界区结束
        // 发送停止条件
        stop();
        // 返回失败
        return false;
    }

    // 清除错误标志
    error=false;
    // 设置当前线程为等待线程
    waiting=Thread::IRQgetCurrentThread();
    
    // 启用I2C的DMA模式、LAST标志和错误中断
    I2C1->CR2 |= I2C_CR2_DMAEN | I2C_CR2_LAST | I2C_CR2_ITERREN;
    
    // 配置DMA1 Stream0控制寄存器为0
    DMA1_Stream0->CR=0;
    // 设置外设地址为I2C1的数据寄存器
    DMA1_Stream0->PAR=reinterpret_cast<unsigned int>(&I2C1->DR);
    // 设置内存地址为接收缓冲区
    DMA1_Stream0->M0AR=reinterpret_cast<unsigned int>(data);
    // 设置数据长度
    DMA1_Stream0->NDTR=len;
    // 配置FIFO控制寄存器，启用FIFO错误中断和禁用直接模式
    DMA1_Stream0->FCR=DMA_SxFCR_FEIE
                    | DMA_SxFCR_DMDIS;
    // 配置DMA控制寄存器
    DMA1_Stream0->CR=DMA_SxCR_CHSEL_0 // 通道1
                   | DMA_SxCR_MINC    // 增加内存指针
                   | DMA_SxCR_TCIE    // 传输完成时中断
                   | DMA_SxCR_TEIE    // 传输错误时中断
                   | DMA_SxCR_DMEIE   // 直接模式错误时中断
                   | DMA_SxCR_EN;     // 启动DMA

    // 重新启用中断，结束临界区
    fastEnableInterrupts(); // 破解解决方案临界区结束

    {
        // 创建临界区，禁用中断
        FastInterruptDisableLock dLock;
        // 等待操作完成
        while(waiting)
        {
            // 当前线程等待
            waiting->IRQwait();
            {
                // 临时启用中断，允许其他线程执行
                FastInterruptEnableLock eLock(dLock);
                // 让出CPU
                Thread::yield();
            }
        }
    }

    // 复位DMA1 Stream7控制寄存器
    DMA1_Stream7->CR=0;
    
    // 清除I2C的DMA使能、LAST标志和错误中断标志
    I2C1->CR2 &= ~(I2C_CR2_DMAEN | I2C_CR2_LAST | I2C_CR2_ITERREN);
    
    // 发送停止条件
    stop();
    // 返回操作结果，如果error为true则操作失败
    return !error;
}

/**
 * 发送数据
 * @param address 设备地址
 * @param data 要发送的数据
 * @param len 数据长度
 * @param sendStop 是否发送停止条件
 * @return 操作是否成功
 */
bool I2C1Master::send(unsigned char address, const void *data, int len, bool sendStop)
{
    // 检查长度是否有效
    if(len<=0 || len>0xffff) return false;
    // 清除地址最低位，表示写操作
    address &= 0xfe; // 掩码位0，因为我们正在写入
    // 发送起始条件并检查操作是否成功
    if(start(address)==false || (I2C1->SR2 & I2C_SR2_TRA)==0)
    {
        // 发送停止条件
        stop();
        // 返回失败
        return false;
    }

    // 清除错误标志
    error=false;

    // 设置当前线程为等待线程
    waiting=Thread::getCurrentThread();
    // 复位DMA1 Stream7控制寄存器
    DMA1_Stream7->CR=0;
    // 设置外设地址为I2C1的数据寄存器
    DMA1_Stream7->PAR=reinterpret_cast<unsigned int>(&I2C1->DR);
    // 设置内存地址为发送缓冲区
    DMA1_Stream7->M0AR=reinterpret_cast<unsigned int>(data);
    // 设置数据长度
    DMA1_Stream7->NDTR=len;
    // 配置FIFO控制寄存器
    DMA1_Stream7->FCR=DMA_SxFCR_FEIE
                    | DMA_SxFCR_DMDIS;
    // 配置DMA控制寄存器
    DMA1_Stream7->CR=DMA_SxCR_CHSEL_0 // 通道1
                   | DMA_SxCR_MINC    // 增加内存指针
                   | DMA_SxCR_DIR_0   // 内存到外设
                   | DMA_SxCR_TCIE    // 传输完成时中断
                   | DMA_SxCR_TEIE    // 传输错误时中断
                   | DMA_SxCR_DMEIE   // 直接模式错误时中断
                   | DMA_SxCR_EN;     // 启动DMA
    
    // 在配置DMA外设之后才在I2C外设中启用DMA
    // 否则会触发虚假中断
    I2C1->CR2 |= I2C_CR2_DMAEN | I2C_CR2_ITERREN;
 
    {
        // 创建临界区，禁用中断
        FastInterruptDisableLock dLock;
        // 等待操作完成
        while(waiting)
        {
            // 当前线程等待
            waiting->IRQwait();
            {
                // 临时启用中断，允许其他线程执行
                FastInterruptEnableLock eLock(dLock);
                // 让出CPU
                Thread::yield();
            }
        }
    }
    
    // 复位DMA1 Stream7控制寄存器
    DMA1_Stream7->CR=0;
    
    // DMA中断例程会更改中断标志！
    // 清除I2C的事件和错误中断标志
    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITERREN);

    // 如果需要发送停止条件
    if(sendStop) stop();
    // 返回操作结果，如果error为true则操作失败
    return !error;
}

/**
 * 析构函数
 * 关闭I2C外设并释放资源
 */
I2C1Master::~I2C1Master()
{
    // 复位I2C1外设
    I2C1->CR1=I2C_CR1_SWRST;
    // 清除复位标志
    I2C1->CR1=0;

    // 禁用所有相关中断
    // 禁用DMA1 Stream7中断
    NVIC_DisableIRQ(DMA1_Stream7_IRQn);
    // 禁用DMA1 Stream0中断
    NVIC_DisableIRQ(DMA1_Stream0_IRQn);
    // 禁用I2C1事件中断
    NVIC_DisableIRQ(I2C1_EV_IRQn);
    // 禁用I2C1错误中断
    NVIC_DisableIRQ(I2C1_ER_IRQn);

    {
        // 创建临界区，禁用中断
        FastInterruptDisableLock dLock;
        // 禁用I2C1外设时钟
        RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;
        // 同步RCC时钟配置
        RCC_SYNC();
    }
    // 清除单例标志，允许重新创建实例
    checkMultipleInstances=false;
}

/**
 * 发送起始条件并发送地址
 * @param address 设备地址
 * @return 操作是否成功
 */
bool I2C1Master::start(unsigned char address)
{
    // 发送起始条件并启用ACK
    I2C1->CR1 |= I2C_CR1_START | I2C_CR1_ACK;
    // 等待状态更新，若失败则返回错误
    if(!waitStatus1()) return false;
    // EV5：必须读取SR1以清除SB
    if((I2C1->SR1 & I2C_SR1_SB)==0) return false;
    // 发送地址
    I2C1->DR=address;
    // 等待状态更新，若失败则返回错误
    if(!waitStatus1()) return false;
    // EV6：必须读取SR1和SR2以清除ADDR
    bool result=true;
    // 检查应答失败标志
    if(I2C1->SR1 & I2C_SR1_AF) result=false;
    // 检查主模式标志
    if((I2C1->SR2 & I2C_SR2_MSL)==0) result=false;
    // 返回操作结果
    return result;
}

/**
 * 特殊的起始条件发送函数，用于处理接收时的特殊情况
 * @param address 设备地址
 * @param len 要接收的字节数
 * @return 操作是否成功
 */
bool I2C1Master::startWorkaround(unsigned char address, int len)
{
    /*
     * 如果这个外设设计合理，这个函数不应该存在。不幸的是，从I2C读取隐藏了一个怪癖
     * 导致使用此驱动程序的代码有时会死锁。怎么会这样？当执行I2C读取时，
     * 一旦称为EV6的条件被清除（一个隐晦的名称，表示"清除指示地址已发送的中断标志"），
     * 外设开始从总线读取。自动进行！软件必须跟上处理事件并设置何时发送NACK
     * 因为最后一个字节已发送，否则会发生三件坏事：
     * 1) 外设可能从总线读取比请求更多的数据
     * 2) 可能来不及在最后一个字节发送NACK，而是发送了ACK
     * 3) 外设可能不再发送所需的中断，导致软件死锁。
     * 但是有DMA，你可能会说，它不应该自动处理所有这些吗？
     * 问题是，如果在清除EV6和配置DMA之间，操作系统决定进行上下文切换，
     * 那么三件坏事就会发生。
     * 应对这种糟糕的硬件设计的干净软件解决方案是创建某种中断驱动的FSM
     * 对事件做出反应并为下一个事件做准备，依靠中断的原子性。
     * 这种解决方案甚至可能与臭名昭著的有bug的STM32F1 I2C外设一起工作。
     * 但现在，我们只需通过禁用中断来创建临界区。
     * 哦，这个函数实现的另一个必要的怪癖是，当接收单个字节时，
     * 除非在EV6之前清除I2C_CR1_ACK，否则一切都会锁定。
     * 这真是脆弱的硬件...
     */
    
    // 发送起始条件并启用ACK
    I2C1->CR1 |= I2C_CR1_START | I2C_CR1_ACK;
    // 等待状态更新，若失败则返回错误
    if(!waitStatus1()) return false;
    // EV5：必须读取SR1以清除SB
    if((I2C1->SR1 & I2C_SR1_SB)==0) return false;
    // 发送地址
    I2C1->DR=address;
    // 等待状态更新，若失败则返回错误
    if(!waitStatus1()) return false;
    // 如果传输是单字节，则必须在清除EV6之前重置ACK
    if(len==1) I2C1->CR1 &= ~I2C_CR1_ACK;
    
    // 开始临界区，禁用中断
    fastDisableInterrupts(); // 破解解决方案临界区开始
    bool result=true;
    // EV6：必须读取SR1和SR2以清除ADDR
    // 检查应答失败标志
    if(I2C1->SR1 & I2C_SR1_AF) result=false;
    // 检查主模式标志
    if((I2C1->SR2 & I2C_SR2_MSL)==0) result=false;
    // 返回操作结果
    return result;
}

/**
 * 等待I2C状态更新
 * @return 操作是否成功
 */
bool I2C1Master::waitStatus1()
{
    // 清除错误标志
    error=false;
    // 设置当前线程为等待线程
    waiting=Thread::getCurrentThread();
    // 启用I2C事件和错误中断
    I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;
    {
        // 创建临界区，禁用中断
        FastInterruptDisableLock dLock;
        // 等待操作完成
        while(waiting)
        {
            // 当前线程等待
            waiting->IRQwait();
            {
                // 临时启用中断，允许其他线程执行
                FastInterruptEnableLock eLock(dLock);
                // 让出CPU
                Thread::yield();
            }
        }
    }
    // 禁用I2C事件和错误中断
    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITERREN);
    // 返回操作结果，如果error为true则操作失败
    return !error;
}

/**
 * 发送I2C停止条件
 */
void I2C1Master::stop()
{
    /*
     * 这个驱动程序的主要思想是避免处理器在等待某些状态标志时空转。
     * 为什么？因为I2C相对于现代处理器来说很慢。一个120MHz的核心在通过时钟为100KHz的
     * I2C传输单个位所需的时间内完成1200个时钟周期。
     * 这段时间可以更好地用于进行上下文切换并让另一个线程做有用的工作，
     * 或者（Miosix在没有就绪线程时会自动执行）让处理器核心休眠。
     * 然而，我对STM32 I2C外设感到相当失望，因为它使用起来似乎过于复杂。
     * 为了接近实现这个目标，我不得不在四个中断处理程序之间进行协调，
     * 两个来自DMA，两个来自I2C本身。最终，更令人失望的是，
     * 我没有找到一种完全避免空转的方法。为什么？
     * 当停止位发送时没有触发中断！
     * 更糟糕的是，文档说在你设置CR2寄存器中的停止位后，
     * 你不能再次写入它（例如，因为两个i2c api调用背靠背进行而发送起始位），
     * 直到MSL位被清除。但是没有与该事件绑定的中断！
     * 更糟糕的是，我在进行I2C发送时找到的最接近的中断标志是在最后一个字节开始发送时触发的。
     * 也许我搜索得不够彻底，但事实是我什么也没找到，
     * 所以下面的代码在最后一个字节的8个数据位加上确认位，再加上停止位期间空转。
     * 那是12000个浪费的CPU周期。谢谢，ST...
     */
    
    // 发送停止条件
    I2C1->CR1 |= I2C_CR1_STOP;
    // 等待主模式标志清除，表示停止条件已发送
    while(I2C1->SR2 & I2C_SR2_MSL) ; // 等待停止位发送
}

// 静态成员变量初始化，用于检查类的多个实例
bool I2C1Master::checkMultipleInstances=false;

} //namespace miosix
