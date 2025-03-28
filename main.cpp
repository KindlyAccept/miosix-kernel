#include <miosix.h>
#include "miosix/arch/cortexM0_stm32f0/stm32f072rb_stm32f0discovery/interfaces-impl/i2c_impl.h"

using namespace miosix;

int main()
{
    I2CImpl i2c;
    uint8_t whoami_reg = 0x0F;
    uint8_t result = 0;
    bool ok = i2c.writeThenRead(0x68, &whoami_reg, 1, &result, 1); // 0x68 是 I2C 地址

    for(;;)
    {
        ledOn();
        Thread::sleep(2000);
        ledOff();
        Thread::sleep(2000);
    }
}
