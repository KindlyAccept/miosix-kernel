
// 包含给STM32F0的I2C驱动的头文件 
#include "stm32f0_i2c.h"
// 包含Miosix操作系统的基本功能
#include <miosix.h>
// 包含调度器相关功能
    // 为什么需要调度器？
        // 调度器是Miosix操作系统中的一个核心组件，用于管理多个线程的执行。
        // 它负责决定哪个线程应该在CPU上运行，以及何时切换到其他线程。
        // 调度器确保所有线程都能公平地使用CPU时间，并根据它们的优先级进行调度。
        // 调度器在多任务环境中非常重要，因为它允许程序在等待I/O操作或长时间运行的函数时继续执行其他任务。
#include <kernel/scheduler/scheduler.h>
// #include "stm32f072xb.h"
#include "arch/common/CMSIS/Device/ST/STM32F0xx/Include/stm32f072xb.h"

// 使用miosix命名空间
using namespace miosix;

// 定义一个静态变量，用于跟踪I2C操作是否发生错误
    // 为什么需要这个变量？
        // 这个变量用于跟踪I2C操作是否发生错误。
        // 当I2C操作发生错误时，设置为true，表示操作失败。
        // 这样可以避免重复尝试相同的操作，或者在错误发生后采取其他措施。
static volatile bool error;     ///< Set to true by IRQ on error
// 指向当前等待I2C操作完成的线程
    // 为什么需要这个变量？
        // 这个变量用于跟踪当前等待I2C操作完成的线程。
        // 当I2C操作完成时，设置为nullptr，表示操作完成。
static Thread *waiting=nullptr; ///< Thread waiting for an operation to complete


// ------------------------------
// DMA1 Channel 2 + 3 共用中断处理入口（TX + RX）
// ------------------------------
void __attribute__((naked)) DMA1_Channel2_3_IRQHandler()
{
    // 保存当前线程的上下文
    saveContext();

    // 判断是否是 Channel 2（I2C1 TX）
    if (DMA1->ISR & DMA_ISR_TCIF2)
    {
        asm volatile("bl _Z20I2C1txDmaHandlerImplv"); // TX
    }

    // 判断是否是 Channel 3（I2C1 RX）
    if (DMA1->ISR & DMA_ISR_TCIF3)
    {
        asm volatile("bl _Z20I2C1rxDmaHandlerImplv"); // RX
    }

    // 恢复当前线程的上下文
    restoreContext();
}


// ------------------------------
// DMA I2C1 接收完成处理函数（RX）
// ------------------------------
void __attribute__((used)) I2C1rxDmaHandlerImpl()
{
    // 清除 DMA1 Channel 3 的所有中断标志
    DMA1->IFCR = DMA_IFCR_CTCIF3
               | DMA_IFCR_CTEIF3
               | DMA_IFCR_CHTIF3
               | DMA_IFCR_CGIF3;

    if (waiting == nullptr) return;

    waiting->IRQwakeup();

    if (waiting->IRQgetPriority() > Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();

    waiting = nullptr;
}


// ------------------------------
// DMA I2C1 发送完成处理函数（TX）
// ------------------------------
void __attribute__((used)) I2C1txDmaHandlerImpl()
{
    // 清除 DMA1 Channel 2 的所有中断标志
    DMA1->IFCR = DMA_IFCR_CTCIF2
               | DMA_IFCR_CTEIF2
               | DMA_IFCR_CHTIF2
               | DMA_IFCR_CGIF2;

    // 关闭 DMA 模式
    I2C1->CR1 &= ~I2C_CR1_TXDMAEN;

    // 启用中断：等待最后一个字节完成
    I2C1->CR1 |= I2C_CR1_TXIE   // 可选
              | I2C_CR1_TCIE   // 传输完成中断
              | I2C_CR1_ERRIE; // 开启错误中断
}


// ------------------------------
// I2C1 事件中断（如最后一个字节完成）
// ------------------------------
void __attribute__((naked)) I2C1_EV_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z15I2C1HandlerImplv");
    restoreContext();
}

void __attribute__((used)) I2C1HandlerImpl()
{
    // 获取状态
    uint32_t isr = I2C1->ISR;

    // 避免中断重入：判断是否是 STOPF 或 TC 等终止状态
    if (isr & I2C_ISR_STOPF)
    {
        I2C1->ICR = I2C_ICR_STOPCF; // 清除 STOPF 标志
    }

    // 判断是否传输完成（包含最后一个字节）
    if (isr & I2C_ISR_TC)
    {
        I2C1->CR2 |= I2C_CR2_STOP;  // 可选：手动发送 STOP
    }

    // 禁用相关中断，防止重入
    I2C1->CR1 &= ~(I2C_CR1_TXIE | I2C_CR1_TCIE);

    // 线程唤醒逻辑
    if (waiting == nullptr) return;

    waiting->IRQwakeup();

    if (waiting->IRQgetPriority() > Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();

    waiting = nullptr;
}


// ------------------------------
// I2C1 错误中断处理
// ------------------------------
void __attribute__((naked)) I2C1_ER_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z18I2C1errHandlerImplv");
    restoreContext();
}

void __attribute__((used)) I2C1errHandlerImpl()
{
    uint32_t isr = I2C1->ISR;

    // 清除所有可能的错误标志
    if (isr & I2C_ISR_NACKF)  I2C1->ICR = I2C_ICR_NACKCF;
    if (isr & I2C_ISR_BERR)   I2C1->ICR = I2C_ICR_BERRCF;
    if (isr & I2C_ISR_ARLO)   I2C1->ICR = I2C_ICR_ARLOCF;
    if (isr & I2C_ISR_OVR)    I2C1->ICR = I2C_ICR_OVRCF;
    if (isr & I2C_ISR_TIMEOUT)I2C1->ICR = I2C_ICR_TIMOUTCF;

    error = true;

    // 线程唤醒逻辑
    if (waiting == nullptr) return;

    waiting->IRQwakeup();

    if (waiting->IRQgetPriority() > Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();

    waiting = nullptr;
}


namespace miosix {
    
/**
 * ------------------------------
 * I2C1Master构造函数
 * 初始化I2C控制器和相关GPIO
 * @param sda SDA信号对应的GPIO引脚
 * @param scl SCL信号对应的GPIO引脚
 * @param frequency I2C通信频率(kHz)
 * ------------------------------
 */
I2C1Master::I2C1Master(GpioPin sda, GpioPin scl, int frequency)
{
    // 检查是否已有此类的实例，避免重复创建
    if(checkMultipleInstances) errorHandler(UNEXPECTED);
    // 设置标志，表示已创建实例
    checkMultipleInstances=true;

    // 计算 APB1 总线频率 fpclk1（用于后续设置 TIMINGR）
    const uint32_t ppreBits = (RCC->CFGR & RCC_CFGR_PPRE_Msk) >> RCC_CFGR_PPRE_Pos;
    int apbPrescaler;
    switch (ppreBits)
    {
        case 0b000: apbPrescaler = 1; break;
        case 0b100: apbPrescaler = 2; break;
        case 0b101: apbPrescaler = 4; break;
        case 0b110: apbPrescaler = 8; break;
        case 0b111: apbPrescaler = 16; break;
        default:    apbPrescaler = 1; break;
    }
    const int fpclk1 = SystemCoreClock / apbPrescaler;
    //iprintf("fpclk1=%d\n",fpclk1);
    
    {
        // 临界区开始，禁用中断
        FastInterruptDisableLock dLock;
        // 注意：在启用外设之前需要配置GPIO，否则第一次读/写调用会永远阻塞
        // 这可能是硬件bug
        // 注意：使用ALTERNATE_OD因为I2C外设不强制开漏模式
        
        sda.alternateFunction(1);       // AF1 → I2C1_SDA
        sda.mode(Mode::ALTERNATE_OD);   // 设置为开漏输出模式
        scl.alternateFunction(1);       // AF1 → I2C1_SCL
        scl.mode(Mode::ALTERNATE_OD);   // 设置为开漏输出模式

        // 启用 DMA1 和 I2C1 时钟（使用 AHBENR 和 APB1ENR）
        RCC->AHBENR |= RCC_AHBENR_DMA1EN;
        RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
        RCC_SYNC();
    }
    
    // 设置 DMA 中断优先级（共用 2/3 通道）
    NVIC_SetPriority(DMA1_Channel2_3_IRQn, 10);
    NVIC_ClearPendingIRQ(DMA1_Channel2_3_IRQn);
    NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

    // 设置 I2C1 事件中断优先级
    NVIC_SetPriority(I2C1_IRQn, 10);
    NVIC_ClearPendingIRQ(I2C1_IRQn);
    NVIC_EnableIRQ(I2C1_IRQn);

    // // 设置 I2C1 错误中断优先级
    // NVIC_SetPriority(I2C1_IRQn, 10);
    // NVIC_ClearPendingIRQ(I2C1_IRQn);
    // NVIC_EnableIRQ(I2C1_IRQn);

    // 禁用 I2C 外设，准备配置
    I2C1->CR1 &= ~I2C_CR1_PE;

    // 限制频率在 10kHz ~ 1MHz
    frequency = std::max(10, std::min(1000, frequency));

    // 设置 TIMINGR，使用 ST 官方推荐值（48MHz PCLK1 为基础）
    if (fpclk1 == 48000000)
    {
        if (frequency > 100)
            I2C1->TIMINGR = 0x00B01A4B; // 400kHz Fast Mode
        else
            I2C1->TIMINGR = 0x2000090E; // 100kHz Standard Mode
    }
    else if (fpclk1 == 24000000)
    {
        if (frequency > 100)
            I2C1->TIMINGR = 0x00500A26; // 400kHz Fast Mode
        else
            I2C1->TIMINGR = 0x10320309; // 100kHz Standard Mode
    }
    else
    {
        // 默认设置为安全值（较慢）
        I2C1->TIMINGR = 0x20303E5D; // 50kHz 保守估计
    }

    // 启用 I2C 外设
    I2C1->CR1=I2C_CR1_PE; 
}


/**
 * ------------------------------
 * 接收数据
 * @param address 设备地址
 * @param data 接收数据的缓冲区
 * @param len 要接收的字节数
 * @return 操作是否成功
 * ------------------------------
 */
bool I2C1Master::recv(unsigned char address, void *data, int len)
{
    if(len<=0 || len>0xFFFF) return false;  // 检查长度是否有效
    address |= 0x01;                        // 设置地址最低位为1，表示读操作

    // 启动通信（如发送地址等）
    if (startWorkaround(address, len) == false || (I2C1->ISR & I2C_ISR_DIR)) 
    {
        fastEnableInterrupts();     // 破解解决方案临界区结束
        stop();                     // 发送停止条件
        return false;               // 返回失败
    }

    // 清除错误标志
    error=false;
    // 设置当前线程为等待线程
    waiting=Thread::IRQgetCurrentThread();
    
    // 启用 I2C 接收 DMA 和错误中断
    I2C1->CR1 |= I2C_CR1_RXDMAEN | I2C_CR1_ERRIE;

    // 配置 DMA1_Channel3（对应 I2C1_RX）
    DMA1_Channel3->CCR = 0; // 清除之前配置
    DMA1_Channel3->CPAR = reinterpret_cast<uint32_t>(&I2C1->RXDR);
    DMA1_Channel3->CMAR = reinterpret_cast<uint32_t>(data);
    DMA1_Channel3->CNDTR = len;

    // 启用 DMA 中断、内存递增、传输完成中断
    DMA1_Channel3->CCR = DMA_CCR_MINC   // 内存地址递增
                       | DMA_CCR_TCIE   // 传输完成中断
                       | DMA_CCR_TEIE   // 错误中断
                       | DMA_CCR_EN;    // 启动 DMA

    // 重新启用中断，结束临界区
    fastEnableInterrupts(); // 破解解决方案临界区结束

    {
        // 临界区开始，禁用中断
        FastInterruptDisableLock dLock;
        // 等待操作完成
        while (waiting)
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

    // 关闭 DMA
    DMA1_Channel3->CCR = 0;

    // 关闭 I2C DMA 接收与错误中断
    I2C1->CR1 &= ~(I2C_CR1_RXDMAEN | I2C_CR1_ERRIE);

    stop(); 
    return !error;
}


/**
 * ------------------------------
 * 发送数据
 * @param address 设备地址
 * @param data 要发送的数据
 * @param len 数据长度
 * @param sendStop 是否发送停止条件
 * @return 操作是否成功
 * ------------------------------
 */
bool I2C1Master::send(unsigned char address, const void *data, int len, bool sendStop)
{
    if(len<=0 || len>0xFFFF) return false;
    address &= 0xFE;    // 清除最低位，写操作

    // 发送起始条件并检查方向
    if (start(address) == false || (I2C1->ISR & I2C_ISR_DIR))
    {
        stop();
        return false;
    }

    // 清除错误标志
    error=false;
    // 设置当前线程为等待线程
    waiting=Thread::IRQgetCurrentThread();

     // 配置 DMA1 Channel 2（I2C1 TX）
    DMA1_Channel2->CCR = 0; // 清除原配置
    DMA1_Channel2->CPAR = reinterpret_cast<uint32_t>(&I2C1->TXDR);
    DMA1_Channel2->CMAR = reinterpret_cast<uint32_t>(data);
    DMA1_Channel2->CNDTR = len;

    // 启用 DMA 中断、内存递增、传输完成中断
    DMA1_Channel2->CCR = DMA_CCR_MINC   // 内存地址递增
                       | DMA_CCR_TCIE   // 传输完成中断
                       | DMA_CCR_TEIE   // 错误中断
                       | DMA_CCR_EN;    // 启动 DMA
    
    // 启用 I2C DMA 发送和错误中断
    I2C1->CR1 |= I2C_CR1_TXDMAEN | I2C_CR1_ERRIE;

    // 【额外添加的】重新启用中断，结束临界区 
    fastEnableInterrupts(); // 破解解决方案临界区结束

    {
        // 创建临界区，禁用中断
        FastInterruptDisableLock dLock;
        // 等待操作完成
        while (waiting)
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

    // 停止 DMA 通道
    DMA1_Channel2->CCR = 0;

    // 禁用 I2C 中断（防止误触发）
    I2C1->CR1 &= ~(I2C_CR1_TXIE | I2C_CR1_TCIE | I2C_CR1_ERRIE);
    
    // 如果需要发送停止条件
    if(sendStop) stop();
    // 返回操作结果，如果error为true则操作失败
    return !error;
}


/**
 * ------------------------------
 * 析构函数
 * 关闭I2C外设并释放资源
 * ------------------------------
 */
I2C1Master::~I2C1Master()
{
    // 复位I2C1外设：软件复位
    I2C1->CR1 |= I2C_CR1_SWRST;    // 有些芯片已经去掉了这个寄存器
    I2C1->CR1 &= ~I2C_CR1_SWRST;   // 清除复位标志

    // 禁用所有相关中断
    NVIC_DisableIRQ(DMA1_Channel2_3_IRQn); // Channel2 = TX, Channel3 = RX
    NVIC_DisableIRQ(I2C1_IRQn);
    // NVIC_DisableIRQ(I2C1_IRQn);

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
 * ------------------------------
 * 发送起始条件并发送地址
 * @param address 设备地址
 * @return 操作是否成功
 * ------------------------------
 */
bool I2C1Master::start(unsigned char address)
{
    // 等待之前传输结束（确保 BUSY 状态已清除）
    while (I2C1->ISR & I2C_ISR_BUSY);

    // 清除 NACKF 等旧状态
    I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF;

    // 配置 CR2：
    // - 从设备地址（7bit 左移1位）
    // - 写方向（RD_WRN = 0）
    // - 默认设置 NBYTES = 1（占位，实际使用者应在后续修改）
    // - 不开启 AUTOEND，由上层决定是否 STOP
    I2C1->CR2 = ((static_cast<uint32_t>(address) << 1)    // SADD[7:1]
              | (1 << I2C_CR2_NBYTES_Pos))                // NBYTES = 1（占位）
              & ~I2C_CR2_RD_WRN                          // 写方向
              & ~I2C_CR2_AUTOEND;                        // 手动 STOP

    // 启动 START
    I2C1->CR2 |= I2C_CR2_START;

    // 等待地址已发送（ADDR 或 TXIS）
    int timeout = 10000;
    while (!(I2C1->ISR & (I2C_ISR_TXIS | I2C_ISR_NACKF)) && --timeout > 0);

    // 检查是否收到 NACK
    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF; // 清除 NACK
        return false;
    }

    // 检查是否主模式
    if ((I2C1->ISR & I2C_ISR_BUSY) == 0)
    {
        return false;
    }

    return true;
}


/**
 * ------------------------------
 * 特殊的起始条件发送函数，用于处理接收时的特殊情况
 * @param address 设备地址
 * @param len 要接收的字节数
 * @return 操作是否成功
 * ------------------------------
 */
bool I2C1Master::startWorkaround(unsigned char address, int len)
{
    // 等待总线空闲
    while (I2C1->ISR & I2C_ISR_BUSY);

    // 清除之前的错误标志
    I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_STOPCF;

    // 构建 CR2
    uint32_t cr2 = (static_cast<uint32_t>(address) << 1)  // SADD (7-bit)
                 | ((len & 0xFF) << I2C_CR2_NBYTES_Pos)   // NBYTES
                 | I2C_CR2_START;                         // 生成 START

    // 设置方向为读（RD_WRN = 1）
    cr2 |= I2C_CR2_RD_WRN;

    // 如果是单字节接收，需要设置 NACK 和 AUTOEND（读完即 STOP）
    if (len == 1)
    {
        cr2 |= I2C_CR2_NACK;     // 不再 ACK
        cr2 |= I2C_CR2_AUTOEND;  // 自动 STOP
    }

    // 应用配置
    I2C1->CR2 = cr2;

    // 等待事件触发
    int timeout = 10000;
    while (!(I2C1->ISR & (I2C_ISR_TXIS | I2C_ISR_NACKF)) && --timeout > 0);

    // 进入临界区，防止上下文切换（和原版行为一致）
    fastDisableInterrupts();
    bool result = true;

    // 检查是否收到 NACK
    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF; // 清除 NACK
        result = false;
    }

    // 检查主模式标志
    if ((I2C1->ISR & I2C_ISR_BUSY) == 0)
    {
        result = false;
    }

    // 返回操作结果
    return result;
}


/**
 * ------------------------------
 * 等待I2C状态更新
 * @return 操作是否成功
 * ------------------------------
 */
bool I2C1Master::waitStatus1()
{
    // 清除错误标志
    error=false;
    // 设置当前线程为等待线程
    waiting=Thread::IRQgetCurrentThread();
    
    // 启用错误中断（例如 NACK、BERR 等）
    I2C1->CR1 |= I2C_CR1_ERRIE;

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

    // 禁用错误中断
    I2C1->CR1 &= ~I2C_CR1_ERRIE;

    // 返回操作结果，如果error为true则操作失败
    return !error;
}


/**
 * ------------------------------
 * 发送I2C停止条件
 * ------------------------------
 */
void I2C1Master::stop()
{
    // 发出 STOP 条件
    I2C1->CR2 |= I2C_CR2_STOP;

    // 等待 STOPF 标志（STOP 发送完成）
    int timeout = 10000;
    while ((I2C1->ISR & I2C_ISR_STOPF) == 0 && --timeout > 0);

    // 清除 STOPF 标志
    I2C1->ICR = I2C_ICR_STOPCF;
}


// 静态成员变量初始化，用于检查类的多个实例
bool I2C1Master::checkMultipleInstances=false;

} //namespace miosix