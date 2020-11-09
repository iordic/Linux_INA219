#include <stdio.h>
#include <stdlib.h>

#include "ina219.h"

int main() {
  int i2c_port = 0;
  ina219_begin(i2c_port, INA219_ADDRESS);
  init();
  printf("Current: %f mA\n", getCurrent_mA());
  printf("Bus Voltage: %f V\n", getBusVoltage_V());
  printf("Shunt Voltage: %f mV\n", getShuntVoltage_mV());
  printf("Power: %f mW\n", getPower_mW());
  return 0;
}
