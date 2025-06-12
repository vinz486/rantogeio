#ifndef CASIO_H
#define CASIO_H

#include <Arduino.h>

class CASIO {
public:
  void begin();

  void set_casio(const String& casio);

  void beep(int hour);
  void beep();

private:
  String _casio;
};

#endif
