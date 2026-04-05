#include "storage/QueueStore.h"
#include "AppConfig.h"

QueueStore::QueueStore() = default;

QueueStore::~QueueStore() {
  if (_loaded) {
    _prefs.end();
  }
}

void QueueStore::ensureLoaded() const {
  if (_loaded) {
    return;
  }
  if (!_prefs.begin("telemetryq", false)) {
    _loaded = true;
    return;
  }
  const uint16_t count = _prefs.getUShort("count", 0);
  const uint16_t boundedCount = count > AppConfig::MAX_QUEUE_RECORDS ? AppConfig::MAX_QUEUE_RECORDS : count;
  _records.clear();
  _records.reserve(boundedCount);
  for (uint16_t i = 0; i < boundedCount; ++i) {
    const String key = "q" + String(i);
    _records.push_back(_prefs.getString(key.c_str(), ""));
  }
  _loaded = true;
}

void QueueStore::persist() const {
  if (!_loaded) {
    return;
  }
  const uint16_t count = static_cast<uint16_t>(_records.size());
  _prefs.putUShort("count", count);
  for (size_t i = 0; i < _records.size(); ++i) {
    const String key = "q" + String(i);
    _prefs.putString(key.c_str(), _records[i]);
  }
  for (size_t i = _records.size(); i < AppConfig::MAX_QUEUE_RECORDS; ++i) {
    const String key = "q" + String(i);
    if (_prefs.isKey(key.c_str())) {
      _prefs.remove(key.c_str());
    }
  }
}

bool QueueStore::enqueue(const String& payload) {
  ensureLoaded();
  if (_records.size() >= AppConfig::MAX_QUEUE_RECORDS) {
    _records.erase(_records.begin());
  }
  _records.push_back(payload);
  persist();
  return true;
}

bool QueueStore::hasPending() const {
  ensureLoaded();
  return !_records.empty();
}

String QueueStore::peek() const {
  ensureLoaded();
  if (_records.empty()) {
    return String();
  }
  return _records.front();
}

void QueueStore::pop() {
  ensureLoaded();
  if (!_records.empty()) {
    _records.erase(_records.begin());
    persist();
  }
}

size_t QueueStore::size() const {
  ensureLoaded();
  return _records.size();
}
