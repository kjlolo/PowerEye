#include "comms/HttpClient.h"

HttpClient::HttpClient(Air780E& modem) : _modem(modem) {}

bool HttpClient::postJson(const String& baseUrl, const String& path, const String& bearerToken, const String& payload, int& httpCode, String& responseBody) {
  return _modem.postJson(baseUrl, path, bearerToken, payload, httpCode, responseBody);
}
