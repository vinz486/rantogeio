#ifndef STEPPERDRIVER_H
#define STEPPERDRIVER_H

#include <Arduino.h>
#include "Fixer.h"

class StepperDriver {
public:
  void begin();

  int step(bool hour, bool minute, int hour_offset = 0, int minute_offset = 0);

  void step_hour(int steps);
  void step_minute(int steps);
  int calibrate_hour(bool &cont);
  int calibrate_minute(bool &cont);

private:
  Fixer _fixer;
};

#endif
