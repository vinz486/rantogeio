# Rantoge ESP32 Clock Controller

This project is an ESP32 controller for an electromechanical clock with independent hour and minute stepper motors.

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
Targets are average **microsteps per digit**, with up to two decimal places.
For each movement the controller adds the integer part, the destination digit's
position offset minus the currently applied position offset, and any whole
microstep accumulated from the fractional part.
The two motors have independent fractional accumulators, which continue across
the 24-hour and 60-minute boundaries. For example, a minute target of `18874.33`
produces exactly `1,887,433` microsteps over 100 minute advances, with zero offsets.

An offset `M10 = +100` means that digit 10 must be positioned 100 microsteps
further forward. Digit 11's UI offset remains zero; the firmware calculates the
shorter departure from digit 10 automatically. Offsets are independent position
corrections, so they do not change the steady-state total per revolution. Every
effective movement must remain positive and fit in a signed 32-bit pulse count.
Invalid settings are rejected before saving or moving either motor.

### Purpose
Compensate for mechanical imprecisions, gear backlash, or initial positioning errors that may occur in different clock assemblies.

### Web Interface Configuration
1. Access the web interface through the ESP32's IP address
2. Navigate to the offset configuration section
3. Adjust the hour and minute offset values as needed
4. **IMPORTANT**: After adding or editing any offset value, you must click "Save All Offsets" to permanently store the changes

### Calibration
On Save All Offsets, an increased position correction for the current digit moves
the motor forward immediately if that position has not already been reached.
Corrections to other digits take effect on arrival. Decreasing the current
digit's position correction takes effect next cycle because the motor cannot
move backward. The actual applied correction is tracked until then, avoiding an
early compensation on the next digit. Re-saving identical values does not move
the motor again.

Live `+` and `-` change the current digit's requested position offset by 100
microsteps through the same planner. Unsaved UI adjustments are preserved and
rebased on the updated saved offsets, even if edited while the request is pending.

Normal operation, fast forward and minute setting use the same movement planner.
Displayed position and fractional carry are committed after emitting the pulses.
There is no encoder feedback: this confirms commanded pulses, not mechanical
motion or motor stalls.

### Command execution and storage

Web and button callbacks enqueue commands; only the Arduino loop drives motors
or changes motion state. HTTP `202` returns a command ID. The UI polls
`/command-status?id=...` and reports success only after execution. A full queue
returns `503`; an execution failure is reported in the command result.

Stop uses a separate priority mailbox and is accepted even when the queue is
full. It cancels commands submitted before Stop and leaves the clock paused,
without automatic catch-up. The current digit movement finishes first to keep
the position reference intact; continuous calibration uses short batches.
Resume clock explicitly restarts time synchronization. Position calibration and
saving offsets are also available while paused.

Targets and offset arrays are stored together in the `motion/config` NVS record.
The record's version 2 stores positional offsets. Earlier zero-sum arrival
tables are converted to equivalent relative positions with digit zero as the
reference; earlier shortened target keys are also supported. Failed writes leave
the active configuration unchanged. Fractional carry and the actual applied
position correction are held in RAM. After an interruption, the displayed-time
reference must still be aligned manually.

### Tests

Run `python3 test/run_host_tests.py` with Python 3, g++ and Node.js installed.
The suite compiles the actual planner, controller and driver against simulated
GPIO, NVS and task queues, with AddressSanitizer and UndefinedBehaviorSanitizer.
It also tests the JavaScript embedded in the firmware. Hardware timing and motor
alignment still require a check on the clock.

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
