#pragma once
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <sys/time.h>
class String : public std::string {
public:
  using std::string::string;
  String(const std::string& value) : std::string(value) {}
  void trim() {
    const auto start = find_first_not_of(" \t\r\n");
    if (start == npos) { clear(); return; }
    const auto end = find_last_not_of(" \t\r\n");
    assign(substr(start, end - start + 1));
  }
};
constexpr int HIGH = 1, LOW = 0, OUTPUT = 1;
constexpr int GPIO_NUM_21 = 21, GPIO_NUM_22 = 22, GPIO_NUM_18 = 18, GPIO_NUM_19 = 19;
void pinMode(int, int);
void digitalWrite(int, int);
void delayMicroseconds(unsigned int);
unsigned long millis();
void configTime(long, int, const char*);
bool getLocalTime(struct tm*);
