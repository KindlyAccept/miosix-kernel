#include "i2c_impl.h"                  // 自己的实现声明
#include "interfaces/i2c.h"            // 抽象接口定义
#include "interfaces/delays.h"         // 延时函数
#include "interfaces/portability.h"    // 端口相关
#include "interfaces/arch_registers.h" // 访问 STM32 寄存器
#include "kernel/kernel.h"             // 线程操作（如 sleep）
#include "kernel/sync.h"               // 互斥锁等（可选）
#include "kernel/logging.h"            // 调试输出（可选）

// using namespace miosix;

namespace miosix {

I2CImpl::I2CImpl()
{
    init();
}

I2CImpl::~I2CImpl()
{
    deinit();
}

void I2CImpl::init()
{
    // 1. 打开 GPIOB 和 I2C1 时钟
    RCC->AHBENR  |= RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    // 2. 配置 PB6 和 PB7 为复用功能（AF1）
    GPIOB->MODER   &= ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOB->MODER   |=  (GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1);                 // AF 模式

    GPIOB->AFR[0]  &= ~(GPIO_AFRL_AFRL6 | GPIO_AFRL_AFRL7);
    GPIOB->AFR[0]  |=  (1 << GPIO_AFRL_AFRL6_Pos) | (1 << GPIO_AFRL_AFRL7_Pos);     // AF1

    GPIOB->OTYPER  |=  GPIO_OTYPER_OT_6 | GPIO_OTYPER_OT_7;                         // 开漏输出
    GPIOB->OSPEEDR |=  GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7;             // 高速
    GPIOB->PUPDR   &= ~(GPIO_PUPDR_PUPDR6 | GPIO_PUPDR_PUPDR7);
    GPIOB->PUPDR   |=  (1 << GPIO_PUPDR_PUPDR6_Pos) | (1 << GPIO_PUPDR_PUPDR7_Pos); // 上拉

    // 3. 复位 I2C1
    RCC->APB1RSTR |=  RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    // 4. 禁用 I2C 外设（修改配置前必须清除 PE 位）
    I2C1->CR1 &= ~I2C_CR1_PE;

    // 5. 配置时序寄存器（TIMINGR）
    // 值 0x00B01A4B 对应：48MHz 时钟下的 100kHz 标准模式
    I2C1->TIMINGR = 0x00B01A4B;

    // 6. 启用 I2C 外设
    I2C1->CR1 |= I2C_CR1_PE;

}

void I2CImpl::deinit()
{
    // 禁用 I2C，释放资源（如有必要）
}

bool I2CImpl::write(uint8_t address, const uint8_t* data, size_t length)
{
    if (length == 0) return false;

    // 1. 等待 I2C 空闲
    while (I2C1->ISR & I2C_ISR_BUSY);

    // 2. 配置 CR2：目标地址、传输字节数、写方向、自动 STOP
    I2C1->CR2 = (address << 1)               | // 左移1 = 写模式
                (length << I2C_CR2_NBYTES_Pos) |
                I2C_CR2_START                 | // 开始传输
                I2C_CR2_AUTOEND;               // 自动 STOP（不需手动设 STOP 位）

    // 3. 逐字节发送数据
    for (size_t i = 0; i < length; ++i)
    {
        // 等待 TXIS = 1 → 可以写数据
        uint32_t timeout = 10000;
        while (!(I2C1->ISR & I2C_ISR_TXIS))
        {
            if (I2C1->ISR & I2C_ISR_NACKF)  // NACK: 设备没响应
            {
                I2C1->ICR |= I2C_ICR_NACKCF; // 清除标志
                return false;
            }
            if (--timeout == 0) return false;
        }

        // 写入数据
        I2C1->TXDR = data[i];
    }

    // 4. 等待 STOP 传输完成（由 AUTOEND 自动生成）
    uint32_t timeout = 10000;
    while (!(I2C1->ISR & I2C_ISR_STOPF))
    {
        if (--timeout == 0) return false;
    }

    // 5. 清除 STOPF 标志
    I2C1->ICR |= I2C_ICR_STOPCF;

    return true;
}


bool I2CImpl::read(uint8_t address, uint8_t* data, size_t length)
{
    if (length == 0) return false;

    // 1. 等待 I2C 空闲
    while (I2C1->ISR & I2C_ISR_BUSY);

    // 2. 配置 CR2：地址、长度、读方向、自动 STOP
    I2C1->CR2 = (address << 1)                  | // 左移1 → 7-bit 地址 + 读位
                (length << I2C_CR2_NBYTES_Pos)  |
                I2C_CR2_RD_WRN                  | // 设置读方向
                I2C_CR2_START                   | // 发送起始信号
                I2C_CR2_AUTOEND;                  // 自动生成 STOP

    for (size_t i = 0; i < length; ++i)
    {
        uint32_t timeout = 10000;
        while (!(I2C1->ISR & I2C_ISR_RXNE)) // 等待接收数据准备好
        {
            if (I2C1->ISR & I2C_ISR_NACKF)
            {
                I2C1->ICR |= I2C_ICR_NACKCF;
                return false;
            }
            if (--timeout == 0) return false;
        }

        data[i] = I2C1->RXDR; // 读取数据
    }

    // 3. 等待 STOPF
    uint32_t timeout = 10000;
    while (!(I2C1->ISR & I2C_ISR_STOPF))
    {
        if (--timeout == 0) return false;
    }

    // 4. 清除 STOPF
    I2C1->ICR |= I2C_ICR_STOPCF;

    return true;
}


bool I2CImpl::writeThenRead(uint8_t address, const uint8_t* wdata, size_t wlen,
                            uint8_t* rdata, size_t rlen)
{
    if (wlen == 0 || rlen == 0) return false;

    // 1. 等待 BUSY 清除
    while (I2C1->ISR & I2C_ISR_BUSY);

    // 2. 设置写模式，不使用 AUTOEND，因为要发 RESTART（不是 STOP）
    I2C1->CR2 = (address << 1)                  |
                (wlen << I2C_CR2_NBYTES_Pos)    |
                I2C_CR2_START;                  // 不加 AUTOEND → 手动控制

    // 3. 写入 wdata 内容（通常是寄存器地址）
    for (size_t i = 0; i < wlen; ++i)
    {
        uint32_t timeout = 10000;
        while (!(I2C1->ISR & I2C_ISR_TXIS))
        {
            if (I2C1->ISR & I2C_ISR_NACKF)
            {
                I2C1->ICR |= I2C_ICR_NACKCF;
                return false;
            }
            if (--timeout == 0) return false;
        }

        I2C1->TXDR = wdata[i];
    }

    // 4. 等待 TC（Transfer Complete）而不是 STOPF
    uint32_t timeout = 10000;
    while (!(I2C1->ISR & I2C_ISR_TC))
    {
        if (--timeout == 0) return false;
    }

    // 5. 配置为读模式，设置 NBYTES 和 READ，启用 AUTOEND
    I2C1->CR2 = (address << 1)                   |
                (rlen << I2C_CR2_NBYTES_Pos)    |
                I2C_CR2_RD_WRN                  | // 读方向
                I2C_CR2_START                   | // 再次发送 START
                I2C_CR2_AUTOEND;

    // 6. 接收 rlen 字节
    for (size_t i = 0; i < rlen; ++i)
    {
        timeout = 10000;
        while (!(I2C1->ISR & I2C_ISR_RXNE))
        {
            if (I2C1->ISR & I2C_ISR_NACKF)
            {
                I2C1->ICR |= I2C_ICR_NACKCF;
                return false;
            }
            if (--timeout == 0) return false;
        }

        rdata[i] = I2C1->RXDR;
    }

    // 7. 等待 STOPF
    timeout = 10000;
    while (!(I2C1->ISR & I2C_ISR_STOPF))
    {
        if (--timeout == 0) return false;
    }

    // 8. 清除 STOPF
    I2C1->ICR |= I2C_ICR_STOPCF;

    return true;
}


bool I2CImpl::waitUntilFlagSet(volatile uint32_t& reg, uint32_t flag, uint32_t timeout)
{
    // 简化版超时等待机制
    for (uint32_t i = 0; i < timeout; ++i)
    {
        if (reg & flag) return true;
        delayMs(1);
    }
    return false;
}

void I2CImpl::generateStop()
{
    // 设置 STOP 位
}

} // namespace miosix
