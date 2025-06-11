# Rantoge ESP32 Clock Controller

This project is a clock controller for Rantoge ESP32 that drives stepper motors to control hour and minute hands of an analog clock.

## Features

- WiFi connectivity with access point mode for configuration
- Synchronization with NTP
- CASIO hourly beep function
- Calibration functions with stepper offset
- Web interface for configuration
- Button control support
- Over-the-air (OTA) update capability

## Hardware Requirements

- ESP32 board
- Stepper motors and drivers for clock hands
- Optional buzzer for CASIO beep
- Optional buttons for manual control

## Pin Mapping

See `include/settings.h` for pin configurations.

## Stepper Offset

The system uses offsets to fine-tune the stepper motor positions for precise clock calibration:

### How it works
The offsets are added to base step values to achieve the correct position:
- **Hours**: `(492 + hour_offset) × MICROSTEPPING_MULTIPLIER` steps
- **Minutes**: `(1180 + minute_offset) × MICROSTEPPING_MULTIPLIER` steps

### Purpose
Compensate for mechanical imprecisions, gear backlash, or initial positioning errors that may occur in different clock assemblies.

### Web Interface Configuration
1. Access the web interface through the ESP32's IP address
2. Navigate to the offset configuration section
3. Adjust the hour and minute offset values as needed
4. **IMPORTANT**: After adding or editing any offset value, you must click "Save All Offsets" to permanently store the changes

### Calibration
Use the `calibrate_hour()` and `calibrate_minute()` functions to determine the optimal offset values for your specific clock assembly.

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
