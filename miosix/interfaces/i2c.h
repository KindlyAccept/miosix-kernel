#ifndef MIOSIX_I2C_H
#define MIOSIX_I2C_H

#include <cstddef>
#include <cstdint>

namespace miosix {

/**
 * I2C 接口抽象类（可多平台实现）
 */
class I2C
{
public:
    virtual ~I2C() {}

    /**
     * 写数据到 I2C 从设备
     * @param address 从设备 7bit 地址（左移后内部添加 R/W）
     * @param data 数据缓冲区指针
     * @param length 要写入的字节数
     * @return 成功返回 true，失败返回 false
     */
    virtual bool write(uint8_t address, const uint8_t* data, size_t length) = 0;

    /**
     * 从 I2C 从设备读取数据
     * @param address 从设备地址
     * @param data 用于存储读取数据的缓冲区
     * @param length 要读取的字节数
     * @return 成功返回 true，失败返回 false
     */
    virtual bool read(uint8_t address, uint8_t* data, size_t length) = 0;

    /**
     * 先写入后读取，常用于访问寄存器类设备
     * @param address 从设备地址
     * @param wdata 要写入的数据（通常是寄存器地址）
     * @param wlen 写入长度
     * @param rdata 存储读取数据的缓冲区
     * @param rlen 读取长度
     * @return 成功返回 true，失败返回 false
     */
    virtual bool writeThenRead(uint8_t address, const uint8_t* wdata, size_t wlen,
                               uint8_t* rdata, size_t rlen) = 0;
};

} // namespace miosix

#endif // MIOSIX_I2C_H
