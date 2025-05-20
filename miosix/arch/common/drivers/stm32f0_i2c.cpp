// include the header file of the I2C driver for STM32F0
#include "stm32f0_i2c.h"
// include the basic functions of the Miosix operating system
#include <miosix.h>
// include the functions related to the scheduler
#include <kernel/scheduler/scheduler.h>
// #include "stm32f072xb.h"
#include "arch/common/CMSIS/Device/ST/STM32F0xx/Include/stm32f072xb.h"

// use the miosix namespace
using namespace miosix;

// define a static variable, used to track whether an I2C operation has occurred
static volatile bool error;     ///< Set to true by IRQ on error
// define a pointer to the thread waiting for an operation to complete
static Thread *waiting=nullptr; ///< Thread waiting for an operation to complete


// ------------------------------
// DMA1 Channel 2 + 3  interrupt handler (TX + RX)
// ------------------------------
void __attribute__((naked)) DMA1_Channel2_3_IRQHandler()
{
    // save the context of the current thread
    saveContext();

    // check if it is Channel 2 (I2C1 TX)
    if (DMA1->ISR & DMA_ISR_TCIF2)
    {
        asm volatile("bl _Z20I2C1txDmaHandlerImplv"); // TX
    }

    // check if it is Channel 3 (I2C1 RX)
    if (DMA1->ISR & DMA_ISR_TCIF3)
    {
        asm volatile("bl _Z20I2C1rxDmaHandlerImplv"); // RX
    }

    // restore the context of the current thread
    restoreContext();
}


// ------------------------------
// DMA I2C1 receive complete handler (RX)
// ------------------------------
void __attribute__((used)) I2C1rxDmaHandlerImpl()
{
    // clear all the interrupt flags of DMA1 Channel 3
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
// DMA I2C1 send complete handler (TX)
// ------------------------------
void __attribute__((used)) I2C1txDmaHandlerImpl()
{
    // clear all the interrupt flags of DMA1 Channel 2
    DMA1->IFCR = DMA_IFCR_CTCIF2
               | DMA_IFCR_CTEIF2
               | DMA_IFCR_CHTIF2
               | DMA_IFCR_CGIF2;

    // disable the DMA mode
    I2C1->CR1 &= ~I2C_CR1_TXDMAEN;

    // enable the interrupt: wait for the last byte to complete
    I2C1->CR1 |= I2C_CR1_TXIE   // optional
              | I2C_CR1_TCIE   // transfer complete interrupt
              | I2C_CR1_ERRIE; // enable error interrupt
}


// ------------------------------
// I2C1 event interrupt (when the last byte is completed)
// ------------------------------
void __attribute__((naked)) I2C1_EV_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z15I2C1HandlerImplv");
    restoreContext();
}

void __attribute__((used)) I2C1HandlerImpl()
{
    // get the status
    uint32_t isr = I2C1->ISR;

    // avoid interrupt re-entry: check if it is the STOPF or TC termination state
    if (isr & I2C_ISR_STOPF)
    {
        I2C1->ICR = I2C_ICR_STOPCF; // clear the STOPF flag
    }

    // check if the transfer is complete (includes the last byte)
    if (isr & I2C_ISR_TC)
    {
        I2C1->CR2 |= I2C_CR2_STOP;  // optional: manually send STOP
    }

    // disable the related interrupts, prevent re-entry
    I2C1->CR1 &= ~(I2C_CR1_TXIE | I2C_CR1_TCIE);

    // thread wakeup logic
    if (waiting == nullptr) return;

    waiting->IRQwakeup();

    if (waiting->IRQgetPriority() > Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();

    waiting = nullptr;
}


// ------------------------------
// I2C1 error interrupt handler
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

    // clear all the possible error flags
    if (isr & I2C_ISR_NACKF)  I2C1->ICR = I2C_ICR_NACKCF;
    if (isr & I2C_ISR_BERR)   I2C1->ICR = I2C_ICR_BERRCF;
    if (isr & I2C_ISR_ARLO)   I2C1->ICR = I2C_ICR_ARLOCF;
    if (isr & I2C_ISR_OVR)    I2C1->ICR = I2C_ICR_OVRCF;
    if (isr & I2C_ISR_TIMEOUT)I2C1->ICR = I2C_ICR_TIMOUTCF;

    error = true;

    // thread wakeup logic
    if (waiting == nullptr) return;

    waiting->IRQwakeup();

    if (waiting->IRQgetPriority() > Thread::IRQgetCurrentThread()->IRQgetPriority())
        Scheduler::IRQfindNextThread();

    waiting = nullptr;
}


namespace miosix {
    
/** 
 * ------------------------------
 * I2C1Master constructor
 * initialize the I2C controller and related GPIO
 * @param sda the GPIO pin corresponding to the SDA signal
 * @param scl the GPIO pin corresponding to the SCL signal
 * @param frequency I2C communication frequency (kHz)
 * ------------------------------
 */
I2C1Master::I2C1Master(GpioPin sda, GpioPin scl, int frequency)
{
    // check if there is already an instance of this class, avoid duplicate creation
    if(checkMultipleInstances) errorHandler(UNEXPECTED);
    // set the flag, indicating that the instance has been created
    checkMultipleInstances=true;

    // calculate the APB1 bus frequency fpclk1 (for later setting TIMINGR)
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
        // the critical section starts, disable the interrupts
        FastInterruptDisableLock dLock;
        // note: before enabling the peripheral, the GPIO needs to be configured, otherwise the first read/write call will block indefinitely
        // this may be a hardware bug
        // note: use ALTERNATE_OD because the I2C peripheral does not force the open-drain mode
        
        sda.alternateFunction(1);       // AF1 → I2C1_SDA
        sda.mode(Mode::ALTERNATE_OD);   // set to open-drain output mode
        scl.alternateFunction(1);       // AF1 → I2C1_SCL
        scl.mode(Mode::ALTERNATE_OD);   // set to open-drain output mode

        // enable the DMA1 and I2C1 clock (using AHBENR and APB1ENR)
        RCC->AHBENR |= RCC_AHBENR_DMA1EN;
        RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
        RCC_SYNC();
    }
    
    // set the DMA interrupt priority (shared 2/3 channels)
    NVIC_SetPriority(DMA1_Channel2_3_IRQn, 10);
    NVIC_ClearPendingIRQ(DMA1_Channel2_3_IRQn);
    NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

    // set the I2C1 event and error interrupt priority
    NVIC_SetPriority(I2C1_IRQn, 10);
    NVIC_ClearPendingIRQ(I2C1_IRQn);
    NVIC_EnableIRQ(I2C1_IRQn);

    // disable the I2C peripheral, prepare for configuration
    I2C1->CR1 &= ~I2C_CR1_PE;

    // limit the frequency to 10kHz ~ 1MHz
    frequency = std::max(10, std::min(1000, frequency));

    // set the TIMINGR, using the ST official recommended value (48MHz PCLK1 as the basis)
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
        // default setting is a safe value (slower)
        I2C1->TIMINGR = 0x20303E5D; // 50kHz conservative estimate
    }

    // enable the I2C peripheral
    I2C1->CR1=I2C_CR1_PE; 
}


/**
 * ------------------------------
 * receive data
 * @param address the device address
 * @param data the buffer to receive the data
 * @param len the number of bytes to receive
 * @return whether the operation is successful
 * ------------------------------
 */
bool I2C1Master::recv(unsigned char address, void *data, int len)
{
    if(len<=0 || len>0xFFFF) return false;  // check if the length is valid
    // address |= 0x01;                     // set the lowest bit of the address to 1, indicating a read operation 
      // PS: but this board seems to not need this

    // start communication (such as sending the address)
    if (startWorkaround(address, len) == false || (I2C1->ISR & I2C_ISR_DIR)) 
    {
        fastEnableInterrupts();     // end the critical section of the workaround
        stop();                     // send the stop condition
        return false;               // return false
    }

    // clear the error flag
    error=false;
    // set the current thread as the waiting thread
    waiting=Thread::IRQgetCurrentThread();
    
    // enable the I2C receive DMA and error interrupt
    I2C1->CR1 |= I2C_CR1_RXDMAEN | I2C_CR1_ERRIE;

    // configure DMA1_Channel3 (corresponding to I2C1_RX)
    DMA1_Channel3->CCR = 0; // clear the previous configuration
    DMA1_Channel3->CPAR = reinterpret_cast<uint32_t>(&I2C1->RXDR);
    DMA1_Channel3->CMAR = reinterpret_cast<uint32_t>(data);
    DMA1_Channel3->CNDTR = len;

    // enable the DMA interrupt, memory increment, transfer complete interrupt
    DMA1_Channel3->CCR = DMA_CCR_MINC   // memory address increment
                       | DMA_CCR_TCIE   // transfer complete interrupt
                       | DMA_CCR_TEIE   // error interrupt
                       | DMA_CCR_EN;    // start DMA

    // re-enable the interrupt, end the critical section
    fastEnableInterrupts(); // end the critical section

    {
        // critical section starts, disable the interrupts
        FastInterruptDisableLock dLock;
        // wait for the operation to complete
        while (waiting)
        {
            // the current thread waits
            waiting->IRQwait();
            {
                // temporarily enable the interrupt, allow other threads to execute
                FastInterruptEnableLock eLock(dLock);
                // yield the CPU
                Thread::yield();
            }
        }
    }

    // disable the DMA
    DMA1_Channel3->CCR = 0;

    // disable the I2C DMA receive and error interrupt
    I2C1->CR1 &= ~(I2C_CR1_RXDMAEN | I2C_CR1_ERRIE);

    stop(); 
    return !error;
}


/**
 * ------------------------------
 * send data
 * @param address the device address
 * @param data the data to send
 * @param len the data length
 * @param sendStop whether to send the stop condition
 * @return whether the operation is successful
 * ------------------------------
 */
bool I2C1Master::send(unsigned char address, const void *data, int len, bool sendStop)
{
    if(len<=0 || len>0xFFFF) return false;
    address &= 0xFE;    // 清除最低位，写操作

    // start the communication and check the direction
    if (start(address) == false || (I2C1->ISR & I2C_ISR_DIR))
    {
        stop();
        return false;
    }

    // clear the error flag
    error=false;
    // set the current thread as the waiting thread
    waiting=Thread::IRQgetCurrentThread();

    // configure DMA1 Channel 2 (I2C1 TX)
    DMA1_Channel2->CCR = 0; // clear the previous configuration
    DMA1_Channel2->CPAR = reinterpret_cast<uint32_t>(&I2C1->TXDR);
    DMA1_Channel2->CMAR = reinterpret_cast<uint32_t>(data);
    DMA1_Channel2->CNDTR = len;

    // enable the DMA interrupt, memory increment, transfer complete interrupt
    DMA1_Channel2->CCR = DMA_CCR_MINC   // memory address increment
                       | DMA_CCR_TCIE   // transfer complete interrupt
                       | DMA_CCR_TEIE   // error interrupt
                       | DMA_CCR_EN;    // start DMA
    
    // enable the I2C DMA send and error interrupt
    I2C1->CR1 |= I2C_CR1_TXDMAEN | I2C_CR1_ERRIE;

    // re-enable the interrupt, end the critical section
    fastEnableInterrupts(); // end the critical section

    {
        // create a critical section, disable the interrupts
        FastInterruptDisableLock dLock;
        // wait for the operation to complete
        while (waiting)
        {
            // the current thread waits
            waiting->IRQwait();
            {
                // temporarily enable the interrupt, allow other threads to execute
                FastInterruptEnableLock eLock(dLock);
                // yield the CPU
                Thread::yield();
            }
        }
    }

    // disable the DMA channel
    DMA1_Channel2->CCR = 0;

    // disable the I2C interrupt (prevent accidental trigger)
    I2C1->CR1 &= ~(I2C_CR1_TXIE | I2C_CR1_TCIE | I2C_CR1_ERRIE);
    
    // if the stop condition is needed
    if(sendStop) stop();
    // return the operation result, if error is true, the operation failed
    return !error;
}


/**
 * ------------------------------
 * destructor
 * disable the I2C peripheral and release the resources
 * ------------------------------
 */
I2C1Master::~I2C1Master()
{
    // reset the I2C1 peripheral: software reset
    I2C1->CR1 |= I2C_CR1_SWRST;    // some chips have removed this register
    I2C1->CR1 &= ~I2C_CR1_SWRST;   // clear the reset flag

    // disable all related interrupts
    NVIC_DisableIRQ(DMA1_Channel2_3_IRQn); // Channel2 = TX, Channel3 = RX
    NVIC_DisableIRQ(I2C1_IRQn);
    // NVIC_DisableIRQ(I2C1_IRQn);

    {
        // create a critical section, disable the interrupts
        FastInterruptDisableLock dLock;
        // disable the I2C1 peripheral clock
        RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;
        // synchronize the RCC clock configuration
        RCC_SYNC();
    }
    // clear the singleton flag, allow re-creation of the instance
    checkMultipleInstances=false;
}


/**
 * ------------------------------
 * send the start condition and address
 * @param address the device address
 * @return whether the operation is successful
 * ------------------------------
 */
bool I2C1Master::start(unsigned char address)
{
    // wait for the previous transfer to end (ensure the BUSY state is cleared)
    while (I2C1->ISR & I2C_ISR_BUSY);

    // clear the NACKF, STOPCF, BERRCF, ARLOCF old states
    I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF;

    // configure CR2:
    // - device address (7bit left shift 1)
    // - write direction (RD_WRN = 0)
    // - default setting NBYTES = 1 (placeholder, actual user should modify it later)
    // - do not enable AUTOEND, let the upper layer decide whether to STOP
    I2C1->CR2 = ((static_cast<uint32_t>(address) << 1)    // SADD[7:1]
              | (1 << I2C_CR2_NBYTES_Pos))                // NBYTES = 1（占位）
              & ~I2C_CR2_RD_WRN                          // write direction
              & ~I2C_CR2_AUTOEND;                        // manual STOP

    // start START
    I2C1->CR2 |= I2C_CR2_START;

    // wait for the address to be sent (ADDR or TXIS)
    int timeout = 10000;
    while (!(I2C1->ISR & (I2C_ISR_TXIS | I2C_ISR_NACKF)) && --timeout > 0);

    // check if a NACK is received
    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF; // clear the NACK
        return false;
    }

    // check if the master mode
    if ((I2C1->ISR & I2C_ISR_BUSY) == 0)
    {
        return false;
    }

    return true;
}


/**
 * ------------------------------
 * a special start condition sending function, used to handle special cases during reception
 * @param address the device address
 * @param len the number of bytes to receive
 * @return whether the operation is successful
 * ------------------------------
 */
bool I2C1Master::startWorkaround(unsigned char address, int len)
{
    // wait for the bus to be idle
    while (I2C1->ISR & I2C_ISR_BUSY);

    // clear the previous error flags
    I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_STOPCF;

    // build CR2
    uint32_t cr2 = (static_cast<uint32_t>(address) << 1)  // SADD (7-bit)
                 | ((len & 0xFF) << I2C_CR2_NBYTES_Pos)   // NBYTES
                 | I2C_CR2_START;                         // generate START

    // set the direction to read (RD_WRN = 1)
    cr2 |= I2C_CR2_RD_WRN;

    // if the single byte reception, need to set NACK and AUTOEND (read and then STOP)
    if (len == 1)
    {
        cr2 |= I2C_CR2_NACK;     // do not ACK
        cr2 |= I2C_CR2_AUTOEND;  // auto STOP
    }

    // apply the configuration
    I2C1->CR2 = cr2;

    // wait for the event to be triggered
    int timeout = 10000;
    while (!(I2C1->ISR & (I2C_ISR_TXIS | I2C_ISR_NACKF)) && --timeout > 0);

    // enter the critical section, prevent context switching (consistent with the original version)
    fastDisableInterrupts();
    bool result = true;

    // check if a NACK is received
    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF; // clear the NACK
        result = false;
    }

    // check if the master mode
    if ((I2C1->ISR & I2C_ISR_BUSY) == 0)
    {
        result = false;
    }

    // return the operation result
    return result;
}


/**
 * ------------------------------
 * wait for the I2C status update
 * @return whether the operation is successful
 * ------------------------------
 */
bool I2C1Master::waitStatus1()
{
    // clear the error flag
    error=false;
    // set the current thread as the waiting thread
    waiting=Thread::IRQgetCurrentThread();
    
    // enable the error interrupt (for example NACK, BERR, etc.)
    I2C1->CR1 |= I2C_CR1_ERRIE;

    {
        // create a critical section, disable the interrupts
        FastInterruptDisableLock dLock;
        // wait for the operation to complete
        while(waiting)
        {
            // the current thread waits
            waiting->IRQwait();
            {
                // temporarily enable the interrupt, allow other threads to execute
                FastInterruptEnableLock eLock(dLock);
                // yield the CPU
                Thread::yield();
            }
        }
    }

    // disable the error interrupt
    I2C1->CR1 &= ~I2C_CR1_ERRIE;

    // return the operation result, if error is true, the operation failed
    return !error;
}


/**
 * ------------------------------
 * send the I2C stop condition
 * ------------------------------
 */
void I2C1Master::stop()
{
    // send the STOP condition
    I2C1->CR2 |= I2C_CR2_STOP;

    // wait for the STOPF flag (STOP sent)
    int timeout = 10000;
    while ((I2C1->ISR & I2C_ISR_STOPF) == 0 && --timeout > 0);

    // clear the STOPF flag
    I2C1->ICR = I2C_ICR_STOPCF;
}


// static member variable initialization, used to check multiple instances of the class
bool I2C1Master::checkMultipleInstances=false;

} //namespace miosix