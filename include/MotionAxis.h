#ifndef MOTION_AXIS_H
#define MOTION_AXIS_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

// Each offset is the position correction of one digit. Transition corrections
// are derived here, never stored in the UI as compensation on another digit.
template <size_t Digits>
class MotionAxis {
public:
  struct Move { int steps; int destination; int remainder; };
  explicit MotionAxis(int32_t target = 100, int stepLimit = INT_MAX)
    : _target(target), _stepLimit(stepLimit) {}

  // Load settings against a manually established position reference.
  bool configure(int32_t target, const int32_t* offsets) {
    MotionAxis candidate = *this;
    candidate.assign(target, offsets);
    candidate._appliedOffset = offsets[_position];
    if (!candidate.valid()) return false;
    *this = candidate;
    return true;
  }

  // Plan an online edit. Positive correction at the current digit is applied
  // immediately; a negative correction waits for its next arrival. Tracking the
  // actual applied offset also handles edits to several adjacent digits at once.
  bool update(int32_t target, const int32_t* offsets, int& forwardSteps) {
    MotionAxis candidate = *this;
    candidate.assign(target, offsets);
    const int64_t delta = int64_t(offsets[_position]) - _appliedOffset;
    if (delta > INT_MAX) return false;
    if (delta > 0) candidate._appliedOffset = offsets[_position];
    if (!candidate.valid()) return false;
    forwardSteps = delta > 0 ? int(delta) : 0;
    *this = candidate;
    return true;
  }

  bool plan(Move& move, bool referenced = true) const {
    move.destination = (_position + 1) % Digits;
    const int fraction = _remainder + _target % 100;
    const int64_t correction = referenced ? int64_t(_offsets[move.destination]) - _appliedOffset : 0;
    const int64_t steps = int64_t(_target / 100) + correction + fraction / 100;
    if (_target < 100 || steps <= 0 || steps > _stepLimit) return false;
    move.steps = int(steps);
    move.remainder = fraction % 100;
    return true;
  }

  void commit(const Move& move) {
    _position = move.destination;
    _remainder = move.remainder;
    _appliedOffset = _offsets[_position];
  }

  bool calibrate(int delta, int& forwardSteps) {
    int32_t offsets[Digits];
    for (size_t i = 0; i < Digits; ++i) offsets[i] = _offsets[i];
    const int64_t corrected = int64_t(offsets[_position]) + delta;
    if (corrected < INT32_MIN || corrected > INT32_MAX) return false;
    offsets[_position] = int32_t(corrected);
    return update(_target, offsets, forwardSteps);
  }

  bool set_position(int position) {
    if (position < 0 || position >= int(Digits)) return false;
    if (_position == position) return true;
    rebase_position(position);
    return true;
  }
  void rebase_position(int position) {
    _position = position;
    _appliedOffset = _offsets[position];
  }
  void adjust_position(int delta) {
    set_position(int((int64_t(_position) + delta) % int(Digits) + int(Digits)) % int(Digits));
  }
  int position() const { return _position; }
  int remainder() const { return _remainder; }
  int32_t target() const { return _target; }
  const int32_t* offsets() const { return _offsets; }

private:
  int32_t _target;
  int _stepLimit;
  int32_t _offsets[Digits] = {};
  int32_t _appliedOffset = 0;
  int _position = 0;
  int _remainder = 0;

  void assign(int32_t target, const int32_t* offsets) {
    _target = target;
    for (size_t i = 0; i < Digits; ++i) _offsets[i] = offsets[i];
  }
  bool valid() const {
    if (_target < 100) return false;
    const int extra = _target % 100 ? 1 : 0;
    for (size_t i = 0; i < Digits; ++i) {
      const int64_t steps = int64_t(_target / 100) + _offsets[i]
                          - _offsets[(i + Digits - 1) % Digits];
      if (steps <= 0 || steps + extra > _stepLimit) return false;
    }
    const int64_t next = int64_t(_target / 100) + _offsets[(_position + 1) % Digits] - _appliedOffset;
    return next > 0 && next + extra <= _stepLimit;
  }
};

struct MotionSettings {
  int32_t version = 2; // Per-digit positions; v1 stored arrival step deltas.
  int32_t hour_target = 787200;
  int32_t minute_target = 1883200;
  int32_t hours[24] = {};
  int32_t minutes[60] = {};
};

// Preserve zero-sum v1 transition tables as equivalent relative positions.
// Digit zero is the reference; nonzero revolution errors cannot be represented
// by positional offsets and must be calibrated through the average target.
template<size_t N>
inline bool convertArrivalOffsets(int32_t (&offsets)[N]) {
  int64_t total = 0;
  for (size_t i = 0; i < N; ++i) total += offsets[i];
  if (total != 0) return false;
  int32_t positions[N] = {};
  int64_t position = 0;
  for (size_t i = 1; i < N; ++i) {
    position += offsets[i];
    if (position < INT32_MIN || position > INT32_MAX) return false;
    positions[i] = int32_t(position);
  }
  for (size_t i = 0; i < N; ++i) offsets[i] = positions[i];
  return true;
}
inline bool upgradeMotionSettings(MotionSettings& config) {
  if (config.version == 2) return true;
  if (config.version != 1) return false;
  if (!convertArrivalOffsets(config.hours) || !convertArrivalOffsets(config.minutes)) return false;
  config.version = 2;
  return true;
}
#endif
