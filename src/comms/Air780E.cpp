#include "comms/Air780E.h"
#include <Update.h>
#include "mbedtls/sha256.h"

namespace {
String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in.charAt(i);
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\r') out += " ";
    else if (c == '\n') out += " ";
    else out += c;
  }
  out.trim();
  return out;
}

String normalizedAt(const String& in) {
  String out = in;
  out.toUpperCase();
  out.replace(" ", "");
  out.replace("\r", "");
  return out;
}

bool hasAnyRegisteredState(const String& response, const char* prefix) {
  const String norm = normalizedAt(response);
  return norm.indexOf(String(prefix) + ":0,1") >= 0 ||
         norm.indexOf(String(prefix) + ":0,5") >= 0 ||
         norm.indexOf(String(prefix) + ":1,1") >= 0 ||
         norm.indexOf(String(prefix) + ":1,5") >= 0 ||
         norm.indexOf(String(prefix) + ":2,1") >= 0 ||
         norm.indexOf(String(prefix) + ":2,5") >= 0;
}

String extractLikelyPhoneNumber(const String& response) {
  // Typical +CNUM format: +CNUM: "","<number>",<type>,...
  const int cnumIdx = response.indexOf("+CNUM:");
  if (cnumIdx < 0) return "";
  int quote = response.indexOf('"', cnumIdx);
  if (quote < 0) return "";
  quote = response.indexOf('"', quote + 1);  // end of alpha field
  if (quote < 0) return "";
  quote = response.indexOf('"', quote + 1);  // start of number
  if (quote < 0) return "";
  const int end = response.indexOf('"', quote + 1);
  if (end < 0) return "";
  String number = response.substring(quote + 1, end);
  number.trim();
  return number;
}

String compactAtSnippet(const String& in, size_t maxLen = 80) {
  String out = in;
  out.replace("\r", " ");
  out.replace("\n", " ");
  out.trim();
  while (out.indexOf("  ") >= 0) out.replace("  ", " ");
  if (out.length() > maxLen) {
    out = out.substring(0, maxLen);
  }
  return out;
}

String quoteAt(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in.charAt(i);
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else out += c;
  }
  return out;
}

String escapeMpubPayload(const String& in) {
  // Air780E MPUB payload is passed inside quotes. Escape control chars and quotes.
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in.charAt(i);
    if (c == '"') out += "\\22";
    else if (c == '\\') out += "\\5C";
    else if (c == '\r') out += "\\0D";
    else if (c == '\n') out += "\\0A";
    else out += c;
  }
  return out;
}

bool extractQuotedField(const String& input, int startAt, String& value, int& nextAt) {
  const int open = input.indexOf('"', startAt);
  if (open < 0) return false;
  const int close = input.indexOf('"', open + 1);
  if (close < 0) return false;
  value = input.substring(open + 1, close);
  nextAt = close + 1;
  return true;
}

bool parseMqttUrcLine(const String& rawLine, String& topic, String& payload) {
  String line = rawLine;
  line.trim();
  if (line.isEmpty()) return false;

  String upper = line;
  upper.toUpperCase();
  if (upper.indexOf("MSUB") < 0 && upper.indexOf("MQTT") < 0 && upper.indexOf("RECV") < 0) {
    return false;
  }

  int pos = 0;
  String q1;
  String q2;
  int next = 0;
  if (extractQuotedField(line, pos, q1, next)) {
    if (extractQuotedField(line, next, q2, next)) {
      topic = q1;
      payload = q2;
      payload.trim();
      return payload.length() > 0;
    }
    if (q1.indexOf('{') >= 0) {
      payload = q1;
      payload.trim();
      return payload.length() > 0;
    }
  }

  const int jsonStart = line.indexOf('{');
  const int jsonEnd = line.lastIndexOf('}');
  if (jsonStart >= 0 && jsonEnd > jsonStart) {
    payload = line.substring(jsonStart, jsonEnd + 1);
    payload.trim();
    return payload.length() > 0;
  }
  return false;
}

bool parseHttpReadLength(const String& response, int& len) {
  const int idx = response.lastIndexOf("+HTTPREAD:");
  if (idx < 0) return false;
  String line = response.substring(idx);
  const int eol = line.indexOf('\n');
  if (eol > 0) line = line.substring(0, eol);
  line.replace("\r", "");
  line.trim();
  const int comma = line.lastIndexOf(',');
  if (comma >= 0) {
    len = line.substring(comma + 1).toInt();
    return len >= 0;
  }
  const int colon = line.indexOf(':');
  if (colon >= 0) {
    len = line.substring(colon + 1).toInt();
    return len >= 0;
  }
  return false;
}
}

Air780E::Air780E(HardwareSerial& serial) : _serial(serial) {}

bool Air780E::begin(int8_t rxPin, int8_t txPin, uint32_t baud, int8_t pwrKeyPin) {
  _pwrKeyPin = pwrKeyPin;
  if (_pwrKeyPin >= 0) {
    // Keep PWRKEY released by default (Hi-Z with pull-up), only pull low when pulsing.
    pinMode(_pwrKeyPin, INPUT_PULLUP);
  }
  _serial.begin(baud, SERIAL_8N1, rxPin, txPin);
  delay(300);
  clearRx();

  if (!ensurePoweredOn() && !probeAtAnyBaud(rxPin, txPin, baud)) {
    _lastError = "modem_no_at";
    return false;
  }

  if (!sendCommand("ATE0", "OK", 1000)) {
    _lastError = "ate0_failed";
    return false;
  }
  if (!sendCommand("AT+CMEE=2", "OK", 1000)) {
    _lastError = "cmee_failed";
    return false;
  }
  if (!sendCommand("AT+CFUN=1", "OK", 3000)) {
    _lastError = "cfun_failed";
    return false;
  }
  sendCommand("AT+CPIN?", "READY", 2500);
  sendCommand("AT+CSQ", "OK", 2000);
  sendCommand("AT+CEREG?", "OK", 2000);
  sendCommand("AT+CGATT?", "OK", 2000);
  return true;
}

bool Air780E::ensurePoweredOn() {
  if (probeAt(4, 1200, 200)) {
    return true;
  }

  if (_pwrKeyPin < 0) {
    _lastError = "pwrkey_not_configured";
    return false;
  }

  pulsePwrKey();
  return probeAt(16, 1200, 500);
}

bool Air780E::probeAtAnyBaud(int8_t rxPin, int8_t txPin, uint32_t preferredBaud) {
  const uint32_t bauds[] = {preferredBaud, 115200, 9600, 57600, 38400, 19200};
  for (uint8_t i = 0; i < (sizeof(bauds) / sizeof(bauds[0])); ++i) {
    _serial.begin(bauds[i], SERIAL_8N1, rxPin, txPin);
    delay(150);
    clearRx();
    if (probeAt(3, 1000, 120)) {
      return true;
    }
  }
  return false;
}

bool Air780E::probeAt(uint8_t attempts, uint32_t timeoutMs, uint32_t retryDelayMs) {
  for (uint8_t i = 0; i < attempts; ++i) {
    if (sendCommand("AT", "OK", timeoutMs)) {
      return true;
    }
    if (i + 1 < attempts) {
      delay(retryDelayMs);
    }
  }
  return false;
}

void Air780E::pulsePwrKey() {
  if (_pwrKeyPin < 0) return;
  // Air780E PWRKEY is active-low: keep low for ~1.2s to power on, then release high.
  pinMode(_pwrKeyPin, OUTPUT);
  digitalWrite(_pwrKeyPin, LOW);
  delay(1200);
  pinMode(_pwrKeyPin, INPUT_PULLUP);
  delay(5000);
}

bool Air780E::isNetworkReady() {
  String out;
  if (!sendCommand("AT", "OK", 1200, &out)) {
    _lastError = "modem_no_at";
    return false;
  }

  out = "";
  if (!sendCommand("AT+CPIN?", "READY", 2000, &out)) {
    _lastError = "sim_not_ready";
    return false;
  }

  bool regOk = false;
  out = "";
  if (sendCommand("AT+CEREG?", "OK", 2000, &out) && hasAnyRegisteredState(out, "+CEREG")) {
    regOk = true;
  }
  if (!regOk) {
    out = "";
    regOk = sendCommand("AT+CREG?", "OK", 2000, &out) && hasAnyRegisteredState(out, "+CREG");
  }
  if (!regOk) {
    _lastError = "not_registered";
    return false;
  }

  // Packet attach may flap during idle; treat it as a soft check once registered.
  out = "";
  if (sendCommand("AT+CGATT?", "OK", 2000, &out)) {
    const String norm = normalizedAt(out);
    if (norm.indexOf("+CGATT:1") < 0) {
      _lastError = "packet_not_attached";
    }
  }

  return true;
}

int Air780E::rssi() {
  String out;
  if (!sendCommand("AT+CSQ", "OK", 2000, &out)) {
    return -113;
  }

  const int idx = out.indexOf("+CSQ:");
  if (idx < 0) {
    return -113;
  }
  const int comma = out.indexOf(',', idx);
  if (comma < 0) {
    return -113;
  }
  const String rawText = out.substring(idx + 5, comma);
  const int rssiRaw = rawText.toInt();
  if (rssiRaw <= 0) return -113;
  if (rssiRaw == 99) return -113;
  return -113 + (2 * rssiRaw);
}

String Air780E::phoneNumber() {
  String out;
  if (!sendCommand("AT+CNUM", "OK", 3000, &out)) {
    return "";
  }
  return extractLikelyPhoneNumber(out);
}

String Air780E::diagnosticsJson() {
  String out;
  String resp;

  auto runExpectOk = [&](const String& cmd, uint32_t timeoutMs, String& response) -> bool {
    response = "";
    return sendCommand(cmd, "OK", timeoutMs, &response);
  };

  auto parseCereg = [](const String& response) -> bool {
    return response.indexOf("+CEREG: 0,1") >= 0 || response.indexOf("+CEREG: 0,5") >= 0 ||
           response.indexOf("+CEREG: 1,1") >= 0 || response.indexOf("+CEREG: 1,5") >= 0;
  };

  auto parseCreg = [](const String& response) -> bool {
    return response.indexOf("+CREG: 0,1") >= 0 || response.indexOf("+CREG: 0,5") >= 0 ||
           response.indexOf("+CREG: 1,1") >= 0 || response.indexOf("+CREG: 1,5") >= 0;
  };

  bool atOk = runExpectOk("AT", 1200, resp);
  const String atResp = jsonEscape(resp);

  bool simOk = sendCommand("AT+CPIN?", "READY", 2000, &resp);
  const String simResp = jsonEscape(resp);

  bool csqOk = runExpectOk("AT+CSQ", 2000, resp);
  const String csqResp = jsonEscape(resp);
  int rssiRaw = -1;
  int rssiDbm = -113;
  const int csqIdx = resp.indexOf("+CSQ:");
  if (csqIdx >= 0) {
    const int comma = resp.indexOf(',', csqIdx);
    if (comma > csqIdx) {
      const int parsed = resp.substring(csqIdx + 5, comma).toInt();
      if (parsed > 0 && parsed < 99) {
        rssiRaw = parsed;
        rssiDbm = -113 + (2 * parsed);
      }
    }
  }

  bool attachOk = sendCommand("AT+CGATT?", "+CGATT: 1", 2000, &resp);
  const String attachResp = jsonEscape(resp);

  String regMode = "none";
  bool regOk = sendCommand("AT+CEREG?", "OK", 2000, &resp) && parseCereg(resp);
  String regResp = resp;
  if (regOk) {
    regMode = "cereg";
  } else {
    regOk = sendCommand("AT+CREG?", "OK", 2000, &resp) && parseCreg(resp);
    regResp = resp;
    if (regOk) {
      regMode = "creg";
    }
  }
  const String regRespEsc = jsonEscape(regResp);

  bool opOk = runExpectOk("AT+COPS?", 3000, resp);
  const String opResp = jsonEscape(resp);

  const bool networkReady = atOk && simOk && attachOk && regOk;
  out.reserve(700);
  out += "{";
  out += "\"network_ready\":" + String(networkReady ? "true" : "false") + ",";
  out += "\"at\":{\"ok\":" + String(atOk ? "true" : "false") + ",\"response\":\"" + atResp + "\"},";
  out += "\"sim\":{\"ok\":" + String(simOk ? "true" : "false") + ",\"response\":\"" + simResp + "\"},";
  out += "\"signal\":{\"ok\":" + String(csqOk ? "true" : "false") + ",\"raw\":" + String(rssiRaw) + ",\"dbm\":" + String(rssiDbm) + ",\"response\":\"" + csqResp + "\"},";
  out += "\"packet_attach\":{\"ok\":" + String(attachOk ? "true" : "false") + ",\"response\":\"" + attachResp + "\"},";
  out += "\"registration\":{\"ok\":" + String(regOk ? "true" : "false") + ",\"mode\":\"" + regMode + "\",\"response\":\"" + regRespEsc + "\"},";
  out += "\"operator\":{\"ok\":" + String(opOk ? "true" : "false") + ",\"response\":\"" + opResp + "\"},";
  out += "\"last_error\":\"" + _lastError + "\",";
  out += "\"timestamp_ms\":" + String(millis());
  out += "}";
  return out;
}

bool Air780E::postJson(const String& baseUrl, const String& path, const String& bearerToken, const String& payload, int& httpCode, String& responseBody) {
  httpCode = 0;
  responseBody = "";
  _lastError = "";

  if (!isNetworkReady()) {
    return false;
  }

  const String url = normalizeUrl(baseUrl, path);
  const bool isHttps = url.startsWith("https://");
  String out;
  {
    const String prevError = _lastError;
    sendCommand("AT+HTTPTERM", "OK", 1500);
    _lastError = prevError;
  }
  if (!sendCommand("AT+HTTPINIT", "OK", 3000)) {
    _lastError = "httpinit_failed";
    return false;
  }

  if (!sendCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000)) {
    _lastError = "http_cid_failed";
    sendCommand("AT+HTTPTERM", "OK", 1500);
    return false;
  }

  if (!sendCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 4000)) {
    _lastError = "http_url_failed";
    sendCommand("AT+HTTPTERM", "OK", 1500);
    return false;
  }

  if (!sendCommand(String("AT+HTTPSSL=") + (isHttps ? "1" : "0"), "OK", 3000)) {
    _lastError = "http_ssl_cfg_failed";
    sendCommand("AT+HTTPTERM", "OK", 1500);
    return false;
  }

  if (!bearerToken.isEmpty()) {
    const String authHeader = "Authorization: Bearer " + bearerToken;
    if (!sendCommand("AT+HTTPPARA=\"USERDATA\",\"" + authHeader + "\"", "OK", 3000)) {
      _lastError = "http_auth_failed";
      sendCommand("AT+HTTPTERM", "OK", 1500);
      return false;
    }
  }

  if (!sendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 2000)) {
    _lastError = "http_content_type_failed";
    sendCommand("AT+HTTPTERM", "OK", 1500);
    return false;
  }

  const size_t payloadLen = payload.length();
  String httpDataCmd = "AT+HTTPDATA=" + String(payloadLen) + ",30000";
  if (!sendCommand(httpDataCmd, "DOWNLOAD", 10000)) {
    // Retry once with a fresh HTTP session in case modem HTTP state is stale.
    sendCommand("AT+HTTPTERM", "OK", 1500);
    if (!sendCommand("AT+HTTPINIT", "OK", 3000) ||
        !sendCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000) ||
        !sendCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 4000) ||
        !sendCommand(String("AT+HTTPSSL=") + (isHttps ? "1" : "0"), "OK", 3000) ||
        !sendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 2000)) {
      _lastError = "httpdata_prep_retry_failed";
      sendCommand("AT+HTTPTERM", "OK", 1500);
      return false;
    }
    if (!bearerToken.isEmpty()) {
      const String authHeader = "Authorization: Bearer " + bearerToken;
      if (!sendCommand("AT+HTTPPARA=\"USERDATA\",\"" + authHeader + "\"", "OK", 3000)) {
        _lastError = "http_auth_retry_failed";
        sendCommand("AT+HTTPTERM", "OK", 1500);
        return false;
      }
    }
    if (!sendCommand(httpDataCmd, "DOWNLOAD", 10000)) {
      _lastError = "httpdata_prompt_failed_len_" + String(payloadLen);
      sendCommand("AT+HTTPTERM", "OK", 1500);
      return false;
    }
  }

  clearRx();
  _serial.print(payload);
  if (!sendCommand("", "OK", 30000, &out)) {
    _lastError = "httpdata_send_failed";
    sendCommand("AT+HTTPTERM", "OK", 1500);
    return false;
  }

  if (!sendCommand("AT+HTTPACTION=1", "+HTTPACTION:", 25000, &out)) {
    _lastError = "httpaction_failed";
    sendCommand("AT+HTTPTERM", "OK", 1500);
    return false;
  }

  int method = 0;
  int bodyLen = 0;
  const unsigned long actionParseStart = millis();
  while (!extractHttpAction(out, method, httpCode, bodyLen) && (millis() - actionParseStart) < 3000) {
    while (_serial.available() > 0) {
      out += static_cast<char>(_serial.read());
    }
    delay(10);
  }
  if (!extractHttpAction(out, method, httpCode, bodyLen)) {
    _lastError = "httpaction_parse_failed";
    sendCommand("AT+HTTPTERM", "OK", 1500);
    return false;
  }

  if (httpCode == 601) {
    // Best-effort bearer/session recovery and one retry for network-error result.
    String tmp;
    sendCommandAny("AT+CGATT=1", "OK", "ERROR", 4000, &tmp);
    sendCommandAny("AT+CGACT=1,1", "OK", "ERROR", 6000, &tmp);
    sendCommandAny("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", "ERROR", 3000, &tmp);
    sendCommandAny("AT+SAPBR=1,1", "OK", "ERROR", 10000, &tmp);

    out = "";
    if (!sendCommand("AT+HTTPACTION=1", "+HTTPACTION:", 25000, &out)) {
      _lastError = "httpaction_retry_failed";
      sendCommand("AT+HTTPTERM", "OK", 1500);
      return false;
    }
    method = 0;
    bodyLen = 0;
    const unsigned long retryParseStart = millis();
    while (!extractHttpAction(out, method, httpCode, bodyLen) && (millis() - retryParseStart) < 3000) {
      while (_serial.available() > 0) {
        out += static_cast<char>(_serial.read());
      }
      delay(10);
    }
    if (!extractHttpAction(out, method, httpCode, bodyLen)) {
      _lastError = "httpaction_retry_parse_failed";
      sendCommand("AT+HTTPTERM", "OK", 1500);
      return false;
    }
  }

  out = "";
  if (bodyLen > 0) {
    if (!sendCommand("AT+HTTPREAD", "OK", 10000, &out)) {
      _lastError = "httpread_failed";
      sendCommand("AT+HTTPTERM", "OK", 1500);
      return false;
    }
    const int payloadStart = out.indexOf("\n");
    if (payloadStart >= 0) {
      responseBody = out.substring(payloadStart + 1);
      responseBody.trim();
      const int okPos = responseBody.lastIndexOf("OK");
      if (okPos > 0) {
        responseBody = responseBody.substring(0, okPos);
        responseBody.trim();
      }
    }
  }

  {
    const String prevError = _lastError;
    sendCommand("AT+HTTPTERM", "OK", 3000);
    _lastError = prevError;
  }
  if (httpCode < 200 || httpCode >= 300) {
    _lastError = "http_status_" + String(httpCode);
    return false;
  }
  return true;
}

bool Air780E::getText(const String& baseUrl, const String& path, const String& bearerToken, int& httpCode, String& responseBody) {
  const String url = normalizeUrl(baseUrl, path);
  return getTextUrl(url, bearerToken, httpCode, responseBody);
}

bool Air780E::getTextUrl(const String& url, const String& bearerToken, int& httpCode, String& responseBody) {
  httpCode = 0;
  responseBody = "";
  _lastError = "";

  if (!httpBegin(url, bearerToken, false)) {
    return false;
  }

  int bodyLen = 0;
  if (!httpAction(0, httpCode, bodyLen)) {
    httpEnd();
    return false;
  }
  if (httpCode < 200 || httpCode >= 300) {
    _lastError = "http_status_" + String(httpCode);
    httpEnd();
    return false;
  }

  String out;
  if (bodyLen > 0) {
    if (!sendCommand("AT+HTTPREAD", "OK", 20000, &out)) {
      _lastError = "httpread_failed";
      httpEnd();
      return false;
    }
    const int payloadStart = out.indexOf("\n");
    if (payloadStart >= 0) {
      responseBody = out.substring(payloadStart + 1);
      responseBody.trim();
      const int okPos = responseBody.lastIndexOf("OK");
      if (okPos > 0) {
        responseBody = responseBody.substring(0, okPos);
        responseBody.trim();
      }
    }
  }
  httpEnd();
  return true;
}

bool Air780E::httpComputeSha256(const String& url, const String& bearerToken, size_t& bytesRead, uint8_t outSha256[32]) {
  bytesRead = 0;
  _lastError = "";
  if (outSha256 == nullptr) {
    _lastError = "sha_out_null";
    return false;
  }
  if (!httpBegin(url, bearerToken, false)) {
    return false;
  }

  int httpCode = 0;
  int bodyLen = 0;
  if (!httpAction(0, httpCode, bodyLen)) {
    httpEnd();
    return false;
  }
  if (httpCode < 200 || httpCode >= 300) {
    _lastError = "http_status_" + String(httpCode);
    httpEnd();
    return false;
  }
  if (bodyLen <= 0) {
    _lastError = "http_empty_body";
    httpEnd();
    return false;
  }

  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, 0);

  uint8_t chunk[1024];
  int offset = 0;
  while (offset < bodyLen) {
    int outLen = 0;
    const int reqLen = (bodyLen - offset) > static_cast<int>(sizeof(chunk))
      ? static_cast<int>(sizeof(chunk))
      : (bodyLen - offset);
    if (!httpReadBinaryChunk(offset, reqLen, chunk, outLen) || outLen <= 0) {
      mbedtls_sha256_free(&shaCtx);
      httpEnd();
      _lastError = "http_chunk_read_failed";
      return false;
    }
    mbedtls_sha256_update(&shaCtx, chunk, outLen);
    offset += outLen;
    bytesRead += static_cast<size_t>(outLen);
  }

  mbedtls_sha256_finish(&shaCtx, outSha256);
  mbedtls_sha256_free(&shaCtx);
  httpEnd();
  return true;
}

bool Air780E::httpDownloadToUpdate(const String& url, const String& bearerToken, size_t& bytesWritten) {
  bytesWritten = 0;
  _lastError = "";
  if (!httpBegin(url, bearerToken, false)) {
    return false;
  }

  int httpCode = 0;
  int bodyLen = 0;
  if (!httpAction(0, httpCode, bodyLen)) {
    httpEnd();
    return false;
  }
  if (httpCode < 200 || httpCode >= 300) {
    _lastError = "http_status_" + String(httpCode);
    httpEnd();
    return false;
  }
  if (bodyLen <= 0) {
    _lastError = "http_empty_body";
    httpEnd();
    return false;
  }

  if (!Update.begin(static_cast<size_t>(bodyLen))) {
    _lastError = "ota_begin_failed_" + String(Update.getError());
    httpEnd();
    return false;
  }

  uint8_t chunk[1024];
  int offset = 0;
  while (offset < bodyLen) {
    int outLen = 0;
    const int reqLen = (bodyLen - offset) > static_cast<int>(sizeof(chunk))
      ? static_cast<int>(sizeof(chunk))
      : (bodyLen - offset);
    if (!httpReadBinaryChunk(offset, reqLen, chunk, outLen) || outLen <= 0) {
      Update.abort();
      httpEnd();
      _lastError = "ota_chunk_read_failed";
      return false;
    }
    const size_t wrote = Update.write(chunk, static_cast<size_t>(outLen));
    if (wrote != static_cast<size_t>(outLen)) {
      _lastError = "ota_write_failed_" + String(Update.getError());
      Update.abort();
      httpEnd();
      return false;
    }
    offset += outLen;
    bytesWritten += static_cast<size_t>(outLen);
  }

  if (!Update.end(true)) {
    _lastError = "ota_finalize_failed_" + String(Update.getError());
    Update.abort();
    httpEnd();
    return false;
  }

  httpEnd();
  return true;
}

bool Air780E::mqttPublish(const String& host,
                          uint16_t port,
                          bool useTls,
                          const String& clientId,
                          const String& username,
                          const String& password,
                          const String& topic,
                          const String& payload,
                          bool useMtls,
                          const String& tlsHostname,
                          const String& caCertPem,
                          const String& clientCertPem,
                          const String& clientKeyPem) {
  _lastError = "";
  if (!ensureMqttConnected(
      host, port, useTls, clientId, username, password,
      useMtls, tlsHostname, caCertPem, clientCertPem, clientKeyPem)) {
    return false;
  }

  if (topic.isEmpty()) {
    _lastError = "mqtt_topic_empty";
    return false;
  }

  String out;
  const String safePayload = escapeMpubPayload(payload);
  const String cmd = "AT+MPUB=\"" + quoteAt(topic) + "\",0,0,\"" + safePayload + "\"";
  if (!sendCommand(cmd, "OK", 12000, &out)) {
    _lastError = "mqtt_pub_failed:" + compactAtSnippet(out);
    _mqttReady = false;
    return false;
  }
  return true;
}

bool Air780E::mqttEnsureConnected(const String& host,
                                  uint16_t port,
                                  bool useTls,
                                  const String& clientId,
                                  const String& username,
                                  const String& password,
                                  bool useMtls,
                                  const String& tlsHostname,
                                  const String& caCertPem,
                                  const String& clientCertPem,
                                  const String& clientKeyPem) {
  return ensureMqttConnected(
    host, port, useTls, clientId, username, password,
    useMtls, tlsHostname, caCertPem, clientCertPem, clientKeyPem);
}

bool Air780E::mqttSubscribe(const String& topic, uint8_t qos) {
  if (!_mqttReady) {
    _lastError = "mqtt_not_connected";
    return false;
  }
  if (topic.isEmpty()) {
    _lastError = "mqtt_sub_topic_empty";
    return false;
  }
  if (qos > 1) qos = 1;

  String out;
  const String cmd = "AT+MSUB=\"" + quoteAt(topic) + "\"," + String(static_cast<int>(qos));
  if (!sendCommand(cmd, "OK", 12000, &out)) {
    _lastError = "mqtt_sub_failed:" + compactAtSnippet(out);
    return false;
  }
  _mqttSubscribedTopic = topic;
  return true;
}

bool Air780E::mqttPollMessage(String& topic, String& payload) {
  topic = "";
  payload = "";

  while (_serial.available() > 0) {
    const char c = static_cast<char>(_serial.read());
    _mqttRxBuffer += c;
    if (_mqttRxBuffer.length() > 4096) {
      _mqttRxBuffer.remove(0, _mqttRxBuffer.length() - 2048);
    }
  }

  int newline = _mqttRxBuffer.indexOf('\n');
  while (newline >= 0) {
    String line = _mqttRxBuffer.substring(0, newline + 1);
    _mqttRxBuffer.remove(0, newline + 1);
    String parsedTopic = "";
    String parsedPayload = "";
    if (parseMqttUrcLine(line, parsedTopic, parsedPayload)) {
      topic = parsedTopic.length() > 0 ? parsedTopic : _mqttSubscribedTopic;
      payload = parsedPayload;
      return true;
    }
    newline = _mqttRxBuffer.indexOf('\n');
  }
  return false;
}

void Air780E::mqttDisconnect() {
  const String prevError = _lastError;
  sendCommand("AT+MDISCONNECT", "OK", 4000);
  sendCommand("AT+MIPCLOSE", "OK", 5000);
  _lastError = prevError;
  _mqttReady = false;
  _mqttEndpoint = "";
  _mqttSubscribedTopic = "";
  _mqttRxBuffer = "";
}

String Air780E::lastError() const {
  return _lastError;
}

bool Air780E::uploadTlsFile(const String& fileName, const String& contents) {
  if (fileName.isEmpty() || contents.isEmpty()) {
    _lastError = "tls_file_empty";
    return false;
  }

  String out;
  sendCommandAny("AT+FSDEL=\"" + quoteAt(fileName) + "\"", "OK", "ERROR", 3000, &out);
  if (!sendCommandAny("AT+FSCREATE=\"" + quoteAt(fileName) + "\"", "OK", "ERROR", 3000, &out)) {
    _lastError = "tls_fscreate_failed:" + compactAtSnippet(out);
    return false;
  }
  if (!sendCommand("AT+FSWRITE=\"" + quoteAt(fileName) + "\",0," + String(contents.length()) + ",15", ">", 12000, &out)) {
    _lastError = "tls_fswrite_prompt_failed:" + compactAtSnippet(out);
    return false;
  }
  clearRx();
  _serial.print(contents);
  if (!sendCommand("", "OK", 30000, &out)) {
    _lastError = "tls_fswrite_failed:" + compactAtSnippet(out);
    return false;
  }
  return true;
}

bool Air780E::mqttConfigureTls(bool useMtls,
                               const String& host,
                               const String& tlsHostname,
                               const String& caCertPem,
                               const String& clientCertPem,
                               const String& clientKeyPem) {
  const String sni = tlsHostname.isEmpty() ? host : tlsHostname;
  const String key = String(useMtls ? "2" : "1") + "|" + sni + "|" +
                     String(caCertPem.length()) + "|" +
                     String(clientCertPem.length()) + "|" +
                     String(clientKeyPem.length());
  if (_mqttTlsConfigKey == key) {
    return true;
  }

  if (!caCertPem.isEmpty()) {
    if (!uploadTlsFile("powereye_ca.crt", caCertPem)) {
      return false;
    }
    if (!sendCommand("AT+SSLCFG=\"cacert\",88,\"powereye_ca.crt\"", "OK", 5000)) {
      _lastError = "tls_cacert_cfg_failed";
      return false;
    }
  }

  if (useMtls) {
    if (clientCertPem.isEmpty() || clientKeyPem.isEmpty()) {
      _lastError = "tls_mtls_missing_cert_or_key";
      return false;
    }
    if (!uploadTlsFile("powereye_client.crt", clientCertPem)) {
      return false;
    }
    if (!uploadTlsFile("powereye_client.key", clientKeyPem)) {
      return false;
    }
    if (!sendCommand("AT+SSLCFG=\"clientcert\",88,\"powereye_client.crt\"", "OK", 5000)) {
      _lastError = "tls_clientcert_cfg_failed";
      return false;
    }
    if (!sendCommand("AT+SSLCFG=\"clientkey\",88,\"powereye_client.key\"", "OK", 5000)) {
      _lastError = "tls_clientkey_cfg_failed";
      return false;
    }
  }

  if (!sendCommand(String("AT+SSLCFG=\"seclevel\",88,") + (useMtls ? "2" : "1"), "OK", 5000)) {
    _lastError = "tls_seclevel_cfg_failed";
    return false;
  }
  if (!sni.isEmpty()) {
    if (!sendCommand("AT+SSLCFG=\"hostname\",88,\"" + quoteAt(sni) + "\"", "OK", 5000)) {
      _lastError = "tls_hostname_cfg_failed";
      return false;
    }
  }
  _mqttTlsConfigKey = key;
  return true;
}

bool Air780E::ensureMqttConnected(const String& host,
                                  uint16_t port,
                                  bool useTls,
                                  const String& clientId,
                                  const String& username,
                                  const String& password,
                                  bool useMtls,
                                  const String& tlsHostname,
                                  const String& caCertPem,
                                  const String& clientCertPem,
                                  const String& clientKeyPem) {
  if (host.isEmpty()) {
    _lastError = "mqtt_host_empty";
    return false;
  }
  if (!isNetworkReady()) {
    return false;
  }

  const String cid = clientId.isEmpty() ? "powereye" : clientId;
  const String endpoint = String(useTls ? "ssl://" : "tcp://") + host + ":" + String(port);
  if (_mqttReady && _mqttEndpoint == endpoint) {
    return true;
  }
  if (_mqttReady && _mqttEndpoint != endpoint) {
    mqttDisconnect();
  }

  String out;
  // Always clear stale socket/session state before opening a new MQTT transport.
  sendCommand("AT+MDISCONNECT", "OK", 4000, &out);
  sendCommand("AT+MIPCLOSE", "OK", 5000, &out);

  String user = username;
  String pass = password;
  if (user.isEmpty()) user = cid;

  // Configure MQTT identity/auth first.
  const String mcfg = "AT+MCONFIG=\"" + quoteAt(cid) + "\",\"" + quoteAt(user) + "\",\"" + quoteAt(pass) + "\"";
  if (!sendCommand(mcfg, "OK", 6000, &out)) {
    _lastError = "mqtt_config_failed:" + compactAtSnippet(out);
    return false;
  }

  if (useTls) {
    if (!mqttConfigureTls(useMtls, host, tlsHostname, caCertPem, clientCertPem, clientKeyPem)) {
      return false;
    }
  }

  const String hostQ = quoteAt(host);
  const String portQ = String(port);
  const String startCmd = useTls
    ? ("AT+SSLMIPSTART=\"" + hostQ + "\",\"" + portQ + "\"")
    : ("AT+MIPSTART=\"" + hostQ + "\",\"" + portQ + "\"");
  bool socketReady = false;

  if (!sendCommand(startCmd, "OK", 12000, &out)) {
    const String upper = normalizedAt(out);
    const bool alreadyConnected = (upper.indexOf("ALREADY") >= 0 && upper.indexOf("CONNECT") >= 0);
    if (!alreadyConnected) {
      // Try one recovery cycle in case socket state is stale.
      sendCommand("AT+MIPCLOSE", "OK", 5000);
      if (!sendCommand(startCmd, "OK", 12000, &out)) {
        const String retryUpper = normalizedAt(out);
        const bool retryAlreadyConnected = (retryUpper.indexOf("ALREADY") >= 0 && retryUpper.indexOf("CONNECT") >= 0);
        if (!retryAlreadyConnected) {
          _lastError = "mqtt_start_failed:" + compactAtSnippet(out);
          return false;
        }
      }
      socketReady = true;
    } else {
      socketReady = true;
    }
  } else {
    socketReady = true;
  }
  // Wait for TCP connect URC only when modem did not already report connected.
  if (socketReady && out.indexOf("CONNECT OK") < 0 && normalizedAt(out).indexOf("ALREADYCONNECT") < 0) {
    String evt;
    if (!sendCommand("", "CONNECT OK", 18000, &evt)) {
      _lastError = "mqtt_start_failed:" + compactAtSnippet(evt);
      return false;
    }
  }

  if (!sendCommand("AT+MCONNECT=1,60", "OK", 6000, &out)) {
    _lastError = "mqtt_connect_failed:" + compactAtSnippet(out);
    _mqttReady = false;
    return false;
  }
  if (out.indexOf("CONNACK OK") < 0) {
    String evt;
    if (!sendCommand("", "CONNACK OK", 15000, &evt)) {
      _lastError = "mqtt_connect_ack_timeout";
      _mqttReady = false;
      return false;
    }
    if (evt.indexOf("CONNACK OK") < 0) {
      _lastError = "mqtt_connect_ack_failed";
      _mqttReady = false;
      return false;
    }
  }

  _mqttReady = true;
  _mqttEndpoint = endpoint;
  _mqttRxBuffer = "";
  return true;
}

bool Air780E::mqttSendData(const String& command, const String& data, const String& expect, uint32_t timeoutMs) {
  if (!sendCommand(command, ">", 8000)) {
    return false;
  }
  clearRx();
  _serial.print(data);
  return sendCommand("", expect, timeoutMs);
}

bool Air780E::sendCommand(const String& command, const String& expect, uint32_t timeoutMs, String* out) {
  return sendCommandAny(command, expect.c_str(), nullptr, timeoutMs, out);
}

bool Air780E::sendCommandAny(const String& command, const char* expect1, const char* expect2, uint32_t timeoutMs, String* out) {
  if (out != nullptr) {
    *out = "";
  }
  if (command.length() > 0) {
    clearRx();
    _serial.print(command);
    _serial.print("\r\n");
  }

  String buffer;
  const unsigned long start = millis();
  while ((millis() - start) < timeoutMs) {
    while (_serial.available() > 0) {
      const char c = static_cast<char>(_serial.read());
      buffer += c;
      if (expect1 != nullptr && buffer.indexOf(expect1) >= 0) {
        if (out != nullptr) {
          *out = buffer;
        }
        return true;
      }
      if (expect2 != nullptr && buffer.indexOf(expect2) >= 0) {
        if (out != nullptr) {
          *out = buffer;
        }
        return true;
      }
      if (buffer.indexOf("ERROR") >= 0) {
        if (out != nullptr) {
          *out = buffer;
        }
        _lastError = "at_error";
        return false;
      }
    }
    delay(2);
  }

  if (out != nullptr) {
    *out = buffer;
  }
  _lastError = "at_timeout";
  return false;
}

bool Air780E::extractHttpAction(const String& line, int& method, int& code, int& len) const {
  const int idx = line.lastIndexOf("+HTTPACTION:");
  if (idx < 0) {
    return false;
  }
  String actionLine = line.substring(idx);
  const int eol = actionLine.indexOf('\n');
  if (eol > 0) {
    actionLine = actionLine.substring(0, eol);
  }
  actionLine.replace("\r", "");
  actionLine.replace("+HTTPACTION:", "");
  actionLine.trim();

  const int firstComma = actionLine.indexOf(',');
  const int secondComma = actionLine.indexOf(',', firstComma + 1);
  if (firstComma < 0 || secondComma < 0) {
    return false;
  }

  method = actionLine.substring(0, firstComma).toInt();
  code = actionLine.substring(firstComma + 1, secondComma).toInt();
  len = actionLine.substring(secondComma + 1).toInt();
  return true;
}

bool Air780E::httpBegin(const String& url, const String& bearerToken, bool setJsonContentType) {
  _lastError = "";
  if (!isNetworkReady()) {
    return false;
  }
  const bool isHttps = url.startsWith("https://");
  {
    const String prevError = _lastError;
    sendCommand("AT+HTTPTERM", "OK", 1500);
    _lastError = prevError;
  }
  if (!sendCommand("AT+HTTPINIT", "OK", 3000)) {
    _lastError = "httpinit_failed";
    return false;
  }
  if (!sendCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000)) {
    _lastError = "http_cid_failed";
    httpEnd();
    return false;
  }
  if (!sendCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 4000)) {
    _lastError = "http_url_failed";
    httpEnd();
    return false;
  }
  if (!sendCommand(String("AT+HTTPSSL=") + (isHttps ? "1" : "0"), "OK", 3000)) {
    _lastError = "http_ssl_cfg_failed";
    httpEnd();
    return false;
  }
  if (!bearerToken.isEmpty()) {
    const String authHeader = "Authorization: Bearer " + bearerToken;
    if (!sendCommand("AT+HTTPPARA=\"USERDATA\",\"" + authHeader + "\"", "OK", 3000)) {
      _lastError = "http_auth_failed";
      httpEnd();
      return false;
    }
  }
  if (setJsonContentType) {
    if (!sendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 2000)) {
      _lastError = "http_content_type_failed";
      httpEnd();
      return false;
    }
  }
  return true;
}

bool Air780E::httpAction(uint8_t method, int& httpCode, int& bodyLen) {
  httpCode = 0;
  bodyLen = 0;
  String out;
  if (!sendCommand("AT+HTTPACTION=" + String(method), "+HTTPACTION:", 35000, &out)) {
    _lastError = "httpaction_failed";
    return false;
  }

  int parsedMethod = 0;
  const unsigned long parseStart = millis();
  while (!extractHttpAction(out, parsedMethod, httpCode, bodyLen) && (millis() - parseStart) < 3000) {
    while (_serial.available() > 0) {
      out += static_cast<char>(_serial.read());
    }
    delay(10);
  }
  if (!extractHttpAction(out, parsedMethod, httpCode, bodyLen)) {
    _lastError = "httpaction_parse_failed";
    return false;
  }
  return true;
}

bool Air780E::httpReadBinaryChunk(int offset, int requestLen, uint8_t* outBuf, int& outLen) {
  outLen = 0;
  if (requestLen <= 0 || outBuf == nullptr) {
    _lastError = "httpread_bad_args";
    return false;
  }

  clearRx();
  _serial.print("AT+HTTPREAD=" + String(offset) + "," + String(requestLen));
  _serial.print("\r\n");

  String out;
  bool seenHeader = false;
  const unsigned long hdrStart = millis();
  while ((millis() - hdrStart) < 15000) {
    while (_serial.available() > 0) {
      const char c = static_cast<char>(_serial.read());
      out += c;
      if (!seenHeader && out.indexOf("+HTTPREAD:") >= 0) {
        seenHeader = true;
      }
      if (seenHeader && c == '\n') {
        break;
      }
    }
    if (seenHeader && out.endsWith("\n")) {
      break;
    }
    delay(1);
  }
  if (!seenHeader) {
    _lastError = "httpread_start_failed";
    return false;
  }
  int dataLen = 0;
  if (!parseHttpReadLength(out, dataLen)) {
    _lastError = "httpread_len_parse_failed";
    return false;
  }
  if (dataLen <= 0 || dataLen > requestLen) {
    _lastError = "httpread_len_invalid_" + String(dataLen);
    return false;
  }

  size_t readTotal = 0;
  const unsigned long start = millis();
  while (readTotal < static_cast<size_t>(dataLen) && (millis() - start) < 20000) {
    if (_serial.available() > 0) {
      outBuf[readTotal++] = static_cast<uint8_t>(_serial.read());
    } else {
      delay(1);
    }
  }
  if (readTotal != static_cast<size_t>(dataLen)) {
    _lastError = "httpread_data_timeout";
    return false;
  }
  outLen = dataLen;

  // Consume trailing CRLF + OK line.
  String tail;
  if (!sendCommand("", "OK", 6000, &tail)) {
    _lastError = "httpread_tail_failed";
    return false;
  }
  return true;
}

bool Air780E::httpEnd() {
  const String prevError = _lastError;
  const bool ok = sendCommand("AT+HTTPTERM", "OK", 3000);
  _lastError = prevError;
  return ok;
}

String Air780E::normalizeUrl(const String& baseUrl, const String& path) const {
  String url = baseUrl;
  url.trim();
  String p = path;
  p.trim();
  if (!p.startsWith("/")) {
    p = "/" + p;
  }
  if (url.endsWith("/")) {
    url.remove(url.length() - 1);
  }
  return url + p;
}

void Air780E::clearRx() {
  while (_serial.available() > 0) {
    const char c = static_cast<char>(_serial.read());
    _mqttRxBuffer += c;
    if (_mqttRxBuffer.length() > 4096) {
      _mqttRxBuffer.remove(0, _mqttRxBuffer.length() - 2048);
    }
  }
}
