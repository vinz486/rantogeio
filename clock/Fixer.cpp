#include "Fixer.h"
#include "settings.h"

void Fixer::begin() {

  _minutes[5] = 300;
  _minutes[10] = 300;

}

int Fixer::get_minute_steps(int currentMinute) {

  int steps = _minutes[currentMinute];
  
  if (steps == 0) {

    return STANDARD_MINUTE_STEPS;
  }

  return steps;
}

int Fixer::get_hour_steps(int currentHour) {

  int steps = _hours[currentHour];

  if (steps == 0) {

    return STANDARD_HOUR_STEPS;
  }

  return steps;
}
