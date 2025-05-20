#include <miosix.h>
#include "arch/common/drivers/stm32f0_i2c.h" 

using namespace miosix;

int main()
{
    // configure GPIOB: PB6(SCL), PB7(SDA)
    GpioPin sda(GPIOB_BASE, 7); // PB7
    GpioPin scl(GPIOB_BASE, 6); // PB6

    // initialize I2C1Master, frequency 100kHz
    I2C1Master i2c(sda, scl, 100); // 100kHz

    iprintf("[INFO] I2C init success\n");

    // write data to device 0x50
    const int txAddr = 0x56;
    // unsigned char txData[16] = {
    //     0x00, 0x11, 0x22, 0x33,
    //     0x44, 0x55, 0x66, 0x77,
    //     0x88, 0x99, 0xAA, 0xBB,
    //     0xCC, 0xDD, 0xEE, 0xFF
    // };
    unsigned char txData[1] = {0xAB};

    bool txSuccess = i2c.send(txAddr, txData, sizeof(txData));
    iprintf("[INFO] write to 0x%02X: %s\n", txAddr, txSuccess ? "success" : "failed");

    Thread::sleep(100);

    // receive data from device 0x68
    const int rxAddr = 0x68;
    unsigned char rxData[8] = {0};

    bool rxSuccess = i2c.recv(rxAddr, rxData, sizeof(rxData));
    if(rxSuccess)
    {
        iprintf("[INFO] read from 0x%02X:", rxAddr);
        for(int i = 0; i < sizeof(rxData); i++)
            iprintf(" 0x%02X", rxData[i]);
        iprintf("\n");
    }
    else
    {
        iprintf("[WARN] read from 0x%02X failed\n", rxAddr);
    }

    // main loop: LED flashing indicates system active
    for(;;)
    {
        ledOn();
        Thread::sleep(1000);
        ledOff();
        Thread::sleep(1000);
    }
}
