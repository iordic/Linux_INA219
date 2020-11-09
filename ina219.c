#include <stdlib.h>
#include <stdio.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "ina219.h"

struct INA219 {
    uint32_t i2c_dev;
    uint8_t i2c_address;
    uint16_t cal_value;
    uint16_t current_divider_ma;
    float power_multiplier_mw;
} ina219;

void ina219_begin(uint32_t i2c_dev, uint8_t i2c_address) {
    ina219.i2c_address = i2c_address;
    // Buffer for filename device   
    char filename[20];
    snprintf(filename, 19, "/dev/i2c-%d", i2c_dev);

    // Save the file descriptor to later use
    ina219.i2c_dev = open(filename, O_RDWR);
    if (ina219.i2c_dev < 0) {
        /* ERROR HANDLING; you can check errno to see what went wrong */
        printf("Error: %s\n", strerror(errno));
        exit(1);
    }

    // We must specify with what device address we want to communicate
    if (ioctl(ina219.i2c_dev, I2C_SLAVE, i2c_address)) {
        /* ERROR HANDLING; you can check errno to see what went wrong */
        printf("Error: %s\n", strerror(errno));
        exit(1);
    }
}

void ina219_end() {
    close(ina219.i2c_dev);    
}

/*!
 *  @brief  begin I2C and set up the hardware
 */
void init() {
  // Set chip to large range config values to start
  setCalibration_32V_2A();
}

/*!
 *  @brief  Gets the raw bus voltage (16-bit signed integer, so +-32767)
 *  @return the raw bus voltage reading
 */
int16_t getBusVoltage_raw() {
    uint16_t value;
    value = (uint16_t) i2c_smbus_read_word_data(ina219.i2c_dev, INA219_REG_BUSVOLTAGE);    // 32 bits instead 16 ????
    // Shift to the right 3 to drop CNVR and OVF and multiply by LSB
    //return (int16_t)((value >> 3) * 4);
  value = ((value & 0xff00) >> 8) | ((value & 0xff) << 8);
    return (int16_t)((value >> 3) * 4);

    return value;
}

/*!
 *  @brief  Gets the raw shunt voltage (16-bit signed integer, so +-32767)
 *  @return the raw shunt voltage reading
 */
int16_t getShuntVoltage_raw() {
  uint16_t value;
  value = (uint16_t) i2c_smbus_read_word_data(ina219.i2c_dev, INA219_REG_SHUNTVOLTAGE);    // 32 bits instead 16 ????
  value = ((value & 0xff00) >> 8) | ((value & 0xff) << 8);
  return value;
}

/*!
 *  @brief  Gets the raw current value (16-bit signed integer, so +-32767)
 *  @return the raw current reading
 */
int16_t getCurrent_raw() {
  uint16_t value;

  // Sometimes a sharp load will reset the INA219, which will
  // reset the cal register, meaning CURRENT and POWER will
  // not be available ... avoid this by always setting a cal
  // value even if it's an unfortunate extra step
  i2c_smbus_write_word_data(ina219.i2c_dev, INA219_REG_CALIBRATION, ina219.cal_value);
  // Now we can safely read the CURRENT register!
  value = (uint16_t) i2c_smbus_read_word_data(ina219.i2c_dev, INA219_REG_CURRENT);
  return value;
}

/*!
 *  @brief  Gets the raw power value (16-bit signed integer, so +-32767)
 *  @return raw power reading
 */
int16_t getPower_raw() {
  uint16_t value;

  // Sometimes a sharp load will reset the INA219, which will
  // reset the cal register, meaning CURRENT and POWER will
  // not be available ... avoid this by always setting a cal
  // value even if it's an unfortunate extra step
  i2c_smbus_write_word_data(ina219.i2c_dev, INA219_REG_CALIBRATION, ina219.cal_value);

  // Now we can safely read the POWER register!
  value =  i2c_smbus_read_word_data(ina219.i2c_dev, INA219_REG_POWER);  
  value = ((value & 0xff00) >> 8) | ((value & 0xff) << 8);
  return value;
}

/*!
 *  @brief  Gets the shunt voltage in mV (so +-327mV)
 *  @return the shunt voltage converted to millivolts
 */
float getShuntVoltage_mV() {
  int16_t value;
  value = getShuntVoltage_raw();
  return value * 0.01;
}

/*!
 *  @brief  Gets the shunt voltage in volts
 *  @return the bus voltage converted to volts
 */
float getBusVoltage_V() {
  int16_t value = getBusVoltage_raw();
  return value * 0.001;
}

/*!
 *  @brief  Gets the current value in mA, taking into account the
 *          config settings and current LSB
 *  @return the current reading convereted to milliamps
 */
float getCurrent_mA() {
  float valueDec = getCurrent_raw();
  valueDec /=  ina219.current_divider_ma;
  return valueDec;
}

/*!
 *  @brief  Gets the power value in mW, taking into account the
 *          config settings and current LSB
 *  @return power reading converted to milliwatts
 */
float getPower_mW() {
  float valueDec = getPower_raw();
  valueDec *= ina219.power_multiplier_mw;
  return valueDec;
}

/*!
 *  @brief  Configures to INA219 to be able to measure up to 32V and 2A
 *          of current.  Each unit of current corresponds to 100uA, and
 *          each unit of power corresponds to 2mW. Counter overflow
 *          occurs at 3.2A.
 *  @note   These calculations assume a 0.1 ohm resistor is present
 */
void setCalibration_32V_2A() {
  ina219.cal_value = 4096;
  ina219.current_divider_ma = 10; // Current LSB = 100uA per bit (1000/100 = 10)
  ina219.power_multiplier_mw = 2.0f; // Power LSB = 1mW per bit (2/1)

  // Set Calibration register to 'Cal' calculated above
  //i2c_smbus_write_block_data(ina219.i2c_dev, INA219_REG_CALIBRATION, 2, ina219.cal_value);
  i2c_smbus_write_word_data(ina219.i2c_dev, INA219_REG_CALIBRATION, ina219.cal_value);

  // Set Config register to take into account the settings above
  uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                    INA219_CONFIG_GAIN_8_320MV | INA219_CONFIG_BADCRES_12BIT |
                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
  //i2c_smbus_write_block_data(ina219.i2c_dev, INA219_REG_CONFIG, 2, config);
  i2c_smbus_write_word_data(ina219.i2c_dev, INA219_REG_CONFIG, config);
}

/*!
 *  @brief  Set power save mode according to parameters
 *  @param  on
 *          boolean value
 */
void powerSave(bool on) {
  // To do...
}

/*!
 *  @brief  Configures to INA219 to be able to measure up to 32V and 1A
 *          of current.  Each unit of current corresponds to 40uA, and each
 *          unit of power corresponds to 800uW. Counter overflow occurs at
 *          1.3A.
 *  @note   These calculations assume a 0.1 ohm resistor is present
 */
void setCalibration_32V_1A() {
  ina219.cal_value = 10240;
  ina219.current_divider_ma = 25;    // Current LSB = 40uA per bit (1000/40 = 25)
  ina219.power_multiplier_mw = 0.8f; // Power LSB = 800uW per bit

  // Set Calibration register to 'Cal' calculated above
  //i2c_smbus_write_block_data(ina219.i2c_dev, INA219_REG_CALIBRATION, 2, ina219.cal_value);
  i2c_smbus_write_word_data(ina219.i2c_dev, INA219_REG_CALIBRATION, ina219.cal_value);


  // Set Config register to take into account the settings above
  uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                    INA219_CONFIG_GAIN_8_320MV | INA219_CONFIG_BADCRES_12BIT |
                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;

  i2c_smbus_write_word_data(ina219.i2c_dev, INA219_REG_CONFIG, config);
}

/*!
 *  @brief set device to alibration which uses the highest precision for
 *     current measurement (0.1mA), at the expense of
 *     only supporting 16V at 400mA max.
 */
void setCalibration_16V_400mA() {
  ina219.cal_value = 8192;
  ina219.current_divider_ma = 20;    // Current LSB = 50uA per bit (1000/50 = 20)
  ina219.power_multiplier_mw = 1.0f; // Power LSB = 1mW per bit

  // Set Calibration register to 'Cal' calculated above
  //i2c_smbus_write_block_data(ina219.i2c_dev, INA219_REG_CALIBRATION, 2, ina219.cal_value);
  i2c_smbus_write_word_data(ina219.i2c_dev, INA219_REG_CALIBRATION, ina219.cal_value);
  // Set Config register to take into account the settings above
  uint16_t config = INA219_CONFIG_BVOLTAGERANGE_16V |
                    INA219_CONFIG_GAIN_1_40MV | INA219_CONFIG_BADCRES_12BIT |
                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;

  //i2c_smbus_write_block_data(ina219.i2c_dev, INA219_REG_CONFIG, 2, config);
  i2c_smbus_write_word_data(ina219.i2c_dev, INA219_REG_CONFIG, config);
}
