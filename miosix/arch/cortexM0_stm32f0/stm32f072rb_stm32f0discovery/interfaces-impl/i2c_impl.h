#ifndef I2C_IMPL_H
#define I2C_IMPL_H

#include "interfaces/i2c.h"
#include "miosix.h"

namespace miosix {

class I2CImpl : public I2C
{
public:
    I2CImpl();
    ~I2CImpl();

    bool write(uint8_t address, const uint8_t* data, size_t length) override;
    bool read(uint8_t address, uint8_t* data, size_t length) override;
    bool writeThenRead(uint8_t address, const uint8_t* wdata, size_t wlen,
                       uint8_t* rdata, size_t rlen) override;

private:
    void init();
    void deinit();

    bool waitUntilFlagSet(volatile uint32_t& reg, uint32_t flag, uint32_t timeout);
    bool generateStart(uint8_t address, bool read);
    void generateStop();

    // ... 其他私有辅助函数
};

} // namespace miosix

#endif // I2C_IMPL_H
