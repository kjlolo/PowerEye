#pragma once
#include "models/AlarmState.h"

class AlarmManager {
public:
  void update(const AlarmState& current);
  bool hasStateChanged() const;
  const AlarmState& current() const;
  const AlarmState& previous() const;

private:
  AlarmState _current;
  AlarmState _previous;
  bool _changed = false;
};
