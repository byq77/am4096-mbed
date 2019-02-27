/** @file AM4096.cpp
 * AM4096 interface library for mbed framework
 * Copyright (C)  Szymon Szantula
 *
 * Distributed under the MIT license.
 * For full terms see the file LICENSE.md.
 */
#include "AM4096.h"

#if AM4096_LOGS
    #define AM_LOG(f_ , ...) printf((f_), ##__VA_ARGS__)
#else
    #define AM_LOG(f_ , ...) do {} while (0)
#endif

#if AM4096_LOGS
static const char CONFIG_STR[] = "*******CONFIG*******\r\n"
                                 "Addr    : 0x%03X\r\n"
                                 "Reg35   : 0x%03X\r\n"
                                 "Pdie    : 0x%03X\r\n"
                                 "Pdtr    : 0x%03X\r\n"
                                 "Slowint : 0x%03X\r\n"
                                 "AGCdis  : 0x%03X\r\n"
                                 "Pdint   : 0x%03X\r\n"
                                 "Zin     : 0x%03X\r\n"
                                 "Sign    : 0x%03X\r\n"
                                 "Bufsel  : 0x%03X\r\n"
                                 "Abridis : 0x%03X\r\n"
                                 "Hist    : 0x%03X\r\n"
                                 "Daa     : 0x%03X\r\n"
                                 "Nfil    : 0x%03X\r\n"
                                 "Res     : 0x%03X\r\n"
                                 "UVW     : 0x%03X\r\n"
                                 "Sth     : 0x%03X\r\n"
                                 "SSIcfg  : 0x%03X\r\n"
                                 "Dac     : 0x%03X\r\n"
                                 "Dact    : 0x%03X\r\n"
                                 "*******************\r\n";

static const char OUTPUT_STR[] = "*******OUTPUT*******\r\n"
                                 "Rpos    : 0x%03X\r\n"
                                 "SRCH    : 0x%03X\r\n"
                                 "Apos    : 0x%03X\r\n"
                                 "SRCH    : 0x%03X\r\n"
                                 "Wel     : 0x%03X\r\n"
                                 "Weh     : 0x%03X\r\n"
                                 "Tho     : 0x%03X\r\n"
                                 "Thof    : 0x%03X\r\n"
                                 "AGCgain : 0x%03X\r\n"
                                 "********************\r\n";
#else
static const char CONFIG_STR[] = "";
static const char OUTPUT_STR[] = "";
#endif

AM4096::AM4096(I2C *i2c_instance, uint8_t hw_addr)
    : _i2c(i2c_instance), _hw_addr(hw_addr), _device_id(0), _initialised(false)
{
    for(int i=0; i<AM4096_CONFIG_DATA_LEN; i++)
        _configuration.data[i]=0;
}

int AM4096::init()
{
    if (_initialised)
        return 0;
    _i2c->frequency(100000);
    if (readReg(AM4096_REGISTER_CONFIG_DATA_ADDR, &_configuration.data[0]))
    {
        if(findAM4096Device())
        {
            AM_LOG("There is no device with this address!\r\nInitialisation failure!\r\n");
            return 1;
        }
    }
    AM_LOG("Device addr: 0x%02X\r\n", _configuration.fields.Addr);
    
    
    uint16_t id_buffer[2];
    for(int i=0;i<AM4096_EEPROM_DEVICE_ID_LEN;i++)
        readReg(AM4096_EEPROM_DEVICE_ID_ADDR+i, id_buffer+i);
    _device_id = (uint32_t)(id_buffer[0] << 16) | id_buffer[1];
    
    AM_LOG("Device id: 0x%08X\r\n");

    for(int i=0; i<AM4096_CONFIG_DATA_LEN; i++)
        readReg(AM4096_EEPROM_CONFIG_DATA_ADDR+i, &_configuration.data[i]);
    
    printAM4096Configuration(&this->_configuration);

    _initialised = true;
    return 0;
}

int AM4096::readReg(uint8_t addr, uint16_t * reg)
{
    char buffer[2];
    int result = 0;
    result += _i2c->write((int)_hw_addr<<1, (const char*)&addr, 1, 1);
    if(addr <= 0x1F)
        wait_us(20); // EEPROM CLK stretching
    result += _i2c->read((int)_hw_addr<<1, buffer,2,0);
    *reg = (uint16_t)((buffer[0] << 8) & 0xff00) | (uint16_t)buffer[1];
    return result; 
}

int AM4096::writeReg(uint8_t addr, uint16_t * reg)
{
    bool flag = !((addr < AM4096_EEPROM_CONFIG_DATA_ADDR + AM4096_CONFIG_DATA_LEN) ||
        ((addr >=AM4096_REGISTER_CONFIG_DATA_ADDR) && 
        (addr < AM4096_REGISTER_CONFIG_DATA_ADDR + AM4096_CONFIG_DATA_LEN) ));
    if(flag)
        return 1;
    
    char buffer[3];
    buffer[0] = addr; 
    buffer[1] = (*reg >> 8) & 0xFF; 
    buffer[2] = *reg & 0xFF;
    return _i2c->write((int)_hw_addr<<1, (const char *)buffer, 3, 0); // wait ~ 20ms after writing to EEPROM 
} 

int AM4096::findAM4096Device()
{
    int found_flag = 1, last_hw_addr = _hw_addr;
    AM_LOG("Starting searching procedure...\r\n");
    _hw_addr = AM4096_ADDR_FIRST;

    while(found_flag && _hw_addr <= AM4096_ADDR_LAST)
    {
        if(found_flag = readReg(AM4096_REGISTER_CONFIG_DATA_ADDR, &_configuration.data[0]))
        {
            _hw_addr++;
            wait_ms(10);
        }
    }
    if(found_flag != AM4096_ERROR_NONE)
    {
        _hw_addr = last_hw_addr;
        AM_LOG("No devices found!\r\n");
        return 1;
    }

    AM_LOG("Device with addr: 0x%02X found!\r\n", _hw_addr);
    return 0;
}

uint32_t AM4096::getDeviceId()
{
    return _device_id;
}

int AM4096::setNewHwAddr(uint8_t hw_addr)
{
    if(hw_addr > 0x7F && _initialised)
    {
        AM_LOG("Can't set new addres!\r\n");
        return 1;
    }
    _configuration.fields.Addr = hw_addr;
    int result = writeReg(AM4096_EEPROM_CONFIG_DATA_ADDR,&_configuration.data[0]);
    wait_ms(20);
    if(result)
    {
        AM_LOG("Can't set new addres!\r\n");
        _configuration.fields.Addr = _hw_addr;
        return 1; 
    }
    _hw_addr = hw_addr;
    AM_LOG("New addr 0x%02X set!\r\n", hw_addr);
    return 0;
}

void AM4096::getConfiguration(AM4096_config_data * conf_ptr)
{
    MBED_ASSERT(conf_ptr);
    *conf_ptr = _configuration;
}

void AM4096::printAM4096Configuration(const AM4096_config_data * conf_ptr)
{
    if(conf_ptr = 0)
        return;
    AM_LOG(
    CONFIG_STR,
    conf_ptr->fields.Addr,
    conf_ptr->fields.Reg35,
    conf_ptr->fields.Pdie,
    conf_ptr->fields.Pdtr,
    conf_ptr->fields.Slowint,
    conf_ptr->fields.AGCdis,
    conf_ptr->fields.Pdint,
    conf_ptr->fields.Zin,
    conf_ptr->fields.Sign,
    conf_ptr->fields.Bufsel,
    conf_ptr->fields.Abridis,
    conf_ptr->fields.Hist,
    conf_ptr->fields.Daa,
    conf_ptr->fields.Nfil,
    conf_ptr->fields.Res,
    conf_ptr->fields.UVW,
    conf_ptr->fields.Sth,
    conf_ptr->fields.SSIcfg,
    conf_ptr->fields.Dac,
    conf_ptr->fields.Dact
    );
}

void AM4096::printAM4096OutputData(const AM4096_output_data * out_ptr)
{
    if(out_ptr = 0)
        return;
    AM_LOG(
        OUTPUT_STR,
        out_ptr->fields.Rpos,
        out_ptr->fields.SRCHRpos,
        out_ptr->fields.Apos,
        out_ptr->fields.SRCHApos,
        out_ptr->fields.Wel,
        out_ptr->fields.Weh,
        out_ptr->fields.Tho,
        out_ptr->fields.Thof,
        out_ptr->fields.AGCgain
    );
}

int AM4096::updateConfiguration(const AM4096_config_data * conf_ptr, bool permament=false)
{
    MBED_ASSERT(conf_ptr);
    _configuration = *conf_ptr;
    int status = 0;
    if(permament)
    {
        for (int i=0; i < AM4096_CONFIG_DATA_LEN; i++)
        {
            status += writeReg(AM4096_EEPROM_CONFIG_DATA_ADDR + 1, &_configuration.data[i]);
            wait_ms(20);
        }
    }
    else
    {
        for (int i = 0; i < AM4096_CONFIG_DATA_LEN; i++)
            status += writeReg(AM4096_REGISTER_CONFIG_DATA_ADDR + i, &_configuration.data[i]);
    }
    return status;
}

int AM4096::readOutputDataRegisters(AM4096_output_data * out_ptr)
{
    MBED_ASSERT(out_ptr);
    int status = 0;
    for(int i = 0; i < AM4096_REGISTER_MEAS_DATA_LEN; i++)
        status = readReg(AM4096_REGISTER_MEAS_DATA_ADDR, &out_ptr->data[i]);
    return status;
}