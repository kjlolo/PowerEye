#include "comms/HttpClient.h"

HttpClient::HttpClient(Air780E& modem) : _modem(modem) {}

bool HttpClient::postJson(const String& baseUrl, const String& path, const String& bearerToken, const String& payload, int& httpCode, String& responseBody) {
  return _modem.postJson(baseUrl, path, bearerToken, payload, httpCode, responseBody);
}

bool HttpClient::getText(const String& baseUrl, const String& path, const String& bearerToken, int& httpCode, String& responseBody) {
  return _modem.getText(baseUrl, path, bearerToken, httpCode, responseBody);
}

bool HttpClient::getTextUrl(const String& url, const String& bearerToken, int& httpCode, String& responseBody) {
  return _modem.getTextUrl(url, bearerToken, httpCode, responseBody);
}

bool HttpClient::computeSha256(const String& url, const String& bearerToken, size_t& bytesRead, uint8_t outSha256[32]) {
  return _modem.httpComputeSha256(url, bearerToken, bytesRead, outSha256);
}

bool HttpClient::downloadToUpdate(const String& url, const String& bearerToken, size_t& bytesWritten) {
  return _modem.httpDownloadToUpdate(url, bearerToken, bytesWritten);
}
