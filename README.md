# Rantoge ESP32 Clock Controller

This project is a clock controller for Rantoge ESP32 that drives stepper motors to control hour and minute hands of an analog clock.

## Features

- Wifi connectivity with access point mode for configuration
- Synchronization with NTP
- CASIO hourly beep function
- Calibration functions
- Web interface for configuration
- Button control support
- Over-the-air (OTA) update capability

## Hardware Requirements

- ESP32 board
- Stepper motors and drivers for clock hands
- Optional buzzer for CASIO beep
- Optional GPS module
- Optional buttons for manual control

## Pin Mapping

See `include/settings.h` for pin configurations.

## Development

This project is developed using PlatformIO. To build and upload:

```
pio run -t upload
```

To monitor serial output:

```
pio device monitor
```

## License

This code is open source.
