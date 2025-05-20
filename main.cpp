#include <miosix.h>
#include "arch/common/drivers/stm32f0_i2c.h" // 你的 I2C 驱动头文件

using namespace miosix;

int main()
{
    // 配置 GPIOB 引脚：PB6 (SCL), PB7 (SDA)
    GpioPin sda(GPIOB_BASE, 7); // PB7
    GpioPin scl(GPIOB_BASE, 6); // PB6

    // 初始化 I2C1Master，频率设置为 100kHz
    I2C1Master i2c(sda, scl, 100); // 100kHz

    iprintf("[INFO] I2C init success\n");

    for(int deviceAddr = 0x03; deviceAddr < 0x77; deviceAddr++)
    {
        // 模拟发送数据
        unsigned char txData[2] = {0x00, 0xAA}; // 写寄存器 0x00 值为 0xAA
        bool success = i2c.send(deviceAddr, txData, sizeof(txData));

        iprintf("[INFO] write to device 0x%02X:%s\n", deviceAddr, success ? "success" : "failed");

        Thread::sleep(10);
    }

    for(int deviceAddr = 0x03; deviceAddr < 0x77; deviceAddr++) 
    {
        // 模拟读取数据
        unsigned char rxData[1] = {0};
        bool success = i2c.recv(deviceAddr, rxData, 1);
    
    if(success)
        iprintf("[INFO] read data:0x%02X\n", rxData[0]);
    else
            iprintf("[WARN] read data failed\n");

        Thread::sleep(10);
    }

    // 主循环：LED 闪烁表示系统活着
    for(;;)
    {
        ledOn();
        Thread::sleep(1000);
        ledOff();
        Thread::sleep(1000);
    }
}
