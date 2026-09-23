#ifndef STEPPERDRIVER_H
#define STEPPERDRIVER_H
#include <Arduino.h>

class StepperDriver {
public:
  void begin();
  // Exact pulse counts: zero disables an axis; negative counts are rejected.
  // Only ClockManager::tick() in the Arduino loop task may call this.
  bool move(int hour_steps, int minute_steps, bool slow = false);
};
#endif
