#pragma once
#include <Arduino.h>
#include "comms/Air780E.h"

class HttpClient {
public:
  explicit HttpClient(Air780E& modem);
  bool postJson(const String& baseUrl, const String& path, const String& bearerToken, const String& payload, int& httpCode, String& responseBody);

private:
  Air780E& _modem;
};
