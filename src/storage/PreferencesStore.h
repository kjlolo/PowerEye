#pragma once
#include <Preferences.h>
#include "config/DeviceConfig.h"

class PreferencesStore {
public:
  bool begin();
  bool load(DeviceConfig& config);
  bool save(const DeviceConfig& config);

private:
  Preferences _prefs;
};
