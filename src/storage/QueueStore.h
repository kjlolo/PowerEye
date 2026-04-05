#pragma once
#include <Arduino.h>
#include <vector>
#include <Preferences.h>

class QueueStore {
public:
  QueueStore();
  ~QueueStore();

  bool enqueue(const String& payload);
  bool hasPending() const;
  String peek() const;
  void pop();
  size_t size() const;

private:
  void ensureLoaded() const;
  void persist() const;

  mutable Preferences _prefs;
  mutable bool _loaded = false;
  mutable std::vector<String> _records;
};
