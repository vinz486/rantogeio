#include "CASIO.h"
#include "settings.h"

void CASIO::begin() {

  pinMode(PIN_CASIO, OUTPUT);
  
  digitalWrite(PIN_CASIO, LOW);
}

void CASIO::set_casio(const String& casio) {

  _casio = casio;
}

void CASIO::beep(int hour) {

  bool beep = false;

  if (_casio == "on") {
    beep = true;
  }

  if (_casio == "day" && (hour >= 8)) {
    beep = true;
  }

  if (beep) {
    tone(PIN_CASIO, CASIO_FREQUENCY, CASIO_DURATION);
    delay(CASIO_DURATION + CASIO_INTERVAL);
    tone(PIN_CASIO, CASIO_FREQUENCY, CASIO_DURATION);
  }
}
