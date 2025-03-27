#ifndef FIXER_H
#define FIXER_H

#include <Arduino.h>

#define STANDARD_MINUTE_STEPS 1180
#define STANDARD_HOUR_STEPS   492

class Fixer {
public:
  void begin();

  int get_minute_steps(int currentMinute);
  int get_hour_steps(int currentHour);

private:
  int _minutes[60];
  int _hours[24];
};

#endif