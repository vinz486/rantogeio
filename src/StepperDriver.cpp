#include "esp32-hal-gpio.h"
#include "StepperDriver.h"
#include "Fixer.h"
#include "settings.h"

void StepperDriver::begin() {
  pinMode(PIN_HOUR_STEP, OUTPUT);
  pinMode(PIN_HOUR_SLEEP, OUTPUT);
  pinMode(PIN_MINUTE_STEP, OUTPUT);
  pinMode(PIN_MINUTE_SLEEP, OUTPUT);

  digitalWrite(PIN_HOUR_STEP, LOW);
  digitalWrite(PIN_HOUR_SLEEP, HIGH);
  digitalWrite(PIN_MINUTE_STEP, LOW);
  digitalWrite(PIN_MINUTE_SLEEP, HIGH);

  _fixer.begin();
}

int StepperDriver::step(bool hour, bool minute, int hour_offset, int minute_offset) {

  if (!hour && !minute) {
    return 0;
  }


  if (hour) {
    digitalWrite(PIN_HOUR_SLEEP, LOW);
  }

  if (minute) {
    digitalWrite(PIN_MINUTE_SLEEP, LOW);
  }


  delayMicroseconds(2000);

  // Steps are now already in microsteps (no multiplier needed)
  int hour_steps = hour ? (HOUR_STEPS_BASE + hour_offset) : 0;
  int minute_steps = minute ? (MINUTE_STEPS_BASE + minute_offset) : 0;

  int log_minute_steps = minute_steps;
  int log_hour_steps = hour_steps;

  int offset = 0;
  while(hour_steps > 0 || minute_steps > 0) {
    offset = (offset + 1) % 60;

    if(offset % 5 == 0 && minute_steps > 0) {
      digitalWrite(PIN_MINUTE_STEP, HIGH);
      delayMicroseconds(5);
      digitalWrite(PIN_MINUTE_STEP, LOW);
      minute_steps--;
    }

    if(offset % 12 == 0 && hour_steps > 0) {
      digitalWrite(PIN_HOUR_STEP, HIGH);
      delayMicroseconds(5);
      digitalWrite(PIN_HOUR_STEP, LOW);
      hour_steps--;
    }

    delayMicroseconds(STEP_INTERVAL);
  }  

  digitalWrite(PIN_HOUR_SLEEP, HIGH);
  digitalWrite(PIN_MINUTE_SLEEP, HIGH);  

  if (hour) {

    return log_hour_steps;
  }

  if (minute) {

    return log_minute_steps;
  }

  return 0;
}

void StepperDriver::step_hour(int steps) {
  if (steps == 0) return;
  
  digitalWrite(PIN_HOUR_SLEEP, LOW);
  delayMicroseconds(2000);
  
  for (int i = 0; i < steps; i++) {
    digitalWrite(PIN_HOUR_STEP, HIGH);
    delayMicroseconds(5);
    digitalWrite(PIN_HOUR_STEP, LOW);
    delayMicroseconds(4 * 12 * STEP_INTERVAL);  // Match calibrate_hour timing
  }
  
  digitalWrite(PIN_HOUR_SLEEP, HIGH);
}

void StepperDriver::step_minute(int steps) {
  if (steps == 0) return;
  
  digitalWrite(PIN_MINUTE_SLEEP, LOW);
  delayMicroseconds(2000);
  
  for (int i = 0; i < steps; i++) {
    digitalWrite(PIN_MINUTE_STEP, HIGH);
    delayMicroseconds(5);
    digitalWrite(PIN_MINUTE_STEP, LOW);
    delayMicroseconds(4 * 5 * STEP_INTERVAL);  // Match calibrate_minute timing
  }
  
  digitalWrite(PIN_MINUTE_SLEEP, HIGH);
}

int StepperDriver::calibrate_hour(bool &cont) {
  digitalWrite(PIN_HOUR_SLEEP, LOW);
  delayMicroseconds(2000);

  int steps = 0;

  while(cont) {
      steps++;
      digitalWrite(PIN_HOUR_STEP, HIGH);
      delayMicroseconds(5);
      digitalWrite(PIN_HOUR_STEP, LOW);
      delayMicroseconds(4 * 12 * STEP_INTERVAL);
  }

  digitalWrite(PIN_HOUR_SLEEP, HIGH);

  return steps;  // Return actual microsteps
}

int StepperDriver::calibrate_minute(bool &cont) {
  digitalWrite(PIN_MINUTE_SLEEP, LOW);
  delayMicroseconds(2000);

  int steps = 0;

  while(cont) {
      steps++;
      digitalWrite(PIN_MINUTE_STEP, HIGH);
      delayMicroseconds(5);
      digitalWrite(PIN_MINUTE_STEP, LOW);
      delayMicroseconds(4 * 5 * STEP_INTERVAL);
  }

  digitalWrite(PIN_MINUTE_SLEEP, HIGH);

  return steps;  // Return actual microsteps
}
