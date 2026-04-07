#pragma once
#include <Arduino.h>
#include "models/TelemetrySnapshot.h"

class TelemetryBuilder {
public:
  String buildJson(const TelemetrySnapshot& snapshot,
                   bool completePayload,
                   bool mcbeamCompatPayload,
                   bool syncOnly = false,
                   const String& syncRequestId = "") const;
};
