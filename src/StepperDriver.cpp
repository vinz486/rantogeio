#include "StepperDriver.h"
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
}

bool StepperDriver::move(int hour_steps, int minute_steps, bool slow) {
  if (hour_steps < 0 || minute_steps < 0) return false;
  if (!hour_steps && !minute_steps) return true;
  const bool hour = hour_steps > 0;
  const bool minute = minute_steps > 0;
  if (hour) digitalWrite(PIN_HOUR_SLEEP, LOW);
  if (minute) digitalWrite(PIN_MINUTE_SLEEP, LOW);
  delayMicroseconds(2000);

  // Preserve normal 12:5 pulse scheduling and slower calibration speed.
  int slot = 0;
  while (hour_steps > 0 || minute_steps > 0) {
    slot = (slot + 1) % 60;
    if (slot % 5 == 0 && minute_steps > 0) {
      digitalWrite(PIN_MINUTE_STEP, HIGH);
      delayMicroseconds(5);
      digitalWrite(PIN_MINUTE_STEP, LOW);
      --minute_steps;
    }
    if (slot % 12 == 0 && hour_steps > 0) {
      digitalWrite(PIN_HOUR_STEP, HIGH);
      delayMicroseconds(5);
      digitalWrite(PIN_HOUR_STEP, LOW);
      --hour_steps;
    }
    delayMicroseconds(STEP_INTERVAL * (slow ? 4 : 1));
  }
  if (hour) digitalWrite(PIN_HOUR_SLEEP, HIGH);
  if (minute) digitalWrite(PIN_MINUTE_SLEEP, HIGH);
  return true;
}
