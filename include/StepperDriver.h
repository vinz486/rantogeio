#ifndef STEPPERDRIVER_H
#define STEPPERDRIVER_H

#include <Arduino.h>
#include "Fixer.h"

class StepperDriver {
public:
  void begin();

  int step(bool hour, bool minute);

  void step_hour(int steps);
  void step_minute(int steps);
  int calibrate_hour(bool &cont);
  int calibrate_minute(bool &cont);

private:
  int get_hour_step_count();
  int get_minute_step_count();
  Fixer _fixer;
};

#endif