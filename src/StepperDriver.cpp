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

int StepperDriver::step(bool hour, bool minute) {

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

  int hour_steps = hour ? get_hour_step_count() : 0;
  int minute_steps = minute ? get_minute_step_count() : 0;

  int log_minute_steps = minute_steps / MICROSTEPPING_MULTIPLIER;
  int log_hour_steps = hour_steps / MICROSTEPPING_MULTIPLIER;

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

int StepperDriver::get_hour_step_count() {
  static int offset = 0;
  
  int steps = 492;
  offset = (offset + 1) % 20;
  if(offset == 0)
    steps -= 9;

  return steps * MICROSTEPPING_MULTIPLIER;
}

int StepperDriver::get_minute_step_count() {
  static int offset = 0;
  
  int steps = 1180;
  offset = (offset + 1) % 25;
  if(offset == 0)
    steps -= 7;

  return steps * MICROSTEPPING_MULTIPLIER;
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

  return steps / MICROSTEPPING_MULTIPLIER;
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

  return steps / MICROSTEPPING_MULTIPLIER;
}