#include "app/AlarmManager.h"
#include <cstring>

void AlarmManager::update(const AlarmState& current) {
  _previous = _current;
  _current = current;
  _changed = std::memcmp(&_previous, &_current, sizeof(AlarmState)) != 0;
}

bool AlarmManager::hasStateChanged() const {
  return _changed;
}

const AlarmState& AlarmManager::current() const {
  return _current;
}

const AlarmState& AlarmManager::previous() const {
  return _previous;
}
