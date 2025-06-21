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

    // define three addresses
    const int addresses[] = {0x56, 0x58, 0x60};
    const int numAddresses = 3;
    
    // prepare data to send: 0x11, 0x22, 0x33, ..., 0xFF
    unsigned char txData[16] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    // send data to three addresses
    for(int addrIndex = 0; addrIndex < numAddresses; addrIndex++)
    {
        int txAddr = addresses[addrIndex];
        
        // send data byte by byte, ensure even if the write operation "fails", the complete data sequence is sent
        iprintf("[INFO] sending to 0x%02X:", txAddr);
        for(int i = 0; i < sizeof(txData); i++)
        {
            unsigned char singleByte = txData[i];
            bool txSuccess = i2c.send(txAddr, &singleByte, 1);
            iprintf(" 0x%02X", singleByte);
            
            // short delay to ensure signal stability
            Thread::sleep(5);
        }
        iprintf(" (complete sequence sent)\n");
        
        // short delay
        Thread::sleep(50);
    }

    Thread::sleep(100);

    // listen to the reply from three addresses
    unsigned char rxData[8] = {0};
    
    for(int addrIndex = 0; addrIndex < numAddresses; addrIndex++)
    {
        int rxAddr = addresses[addrIndex];
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
        
        // short delay
        Thread::sleep(50);
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
