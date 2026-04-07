#pragma once
#include <Arduino.h>
#include "comms/Air780E.h"

class HttpClient {
public:
  explicit HttpClient(Air780E& modem);
  bool postJson(const String& baseUrl, const String& path, const String& bearerToken, const String& payload, int& httpCode, String& responseBody);
  bool getText(const String& baseUrl, const String& path, const String& bearerToken, int& httpCode, String& responseBody);
  bool getTextUrl(const String& url, const String& bearerToken, int& httpCode, String& responseBody);
  bool computeSha256(const String& url, const String& bearerToken, size_t& bytesRead, uint8_t outSha256[32]);
  bool downloadToUpdate(const String& url, const String& bearerToken, size_t& bytesWritten);

private:
  Air780E& _modem;
};
