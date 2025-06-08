#ifndef _BATTERY_H_
#define _BATTERY_H_

float getBatVoltage()
{
  return M5.Power.getBatteryVoltage();
}

int getBatCapacity(){
  return M5.Power.getBatteryLevel();
}

#endif