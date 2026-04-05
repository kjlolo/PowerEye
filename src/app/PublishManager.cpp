#include "app/PublishManager.h"

PublishManager::PublishManager(HttpClient& http, MqttClient& mqtt, QueueStore& queue, DeviceConfig& config)
  : _http(http), _mqtt(mqtt), _queue(queue), _config(config) {}

void PublishManager::enqueue(const String& payload) {
  _queue.enqueue(payload);
}

PublishResult PublishManager::process(bool networkOnline) {
  if (!networkOnline || !_queue.hasPending()) {
    return PublishResult::RETRY;
  }

  const String payload = _queue.peek();

  _lastHttpCode = 0;
  _lastResponse = "";
  _lastTransport = "none";

  if (_config.cloud.mqttEnabled) {
    if (_mqtt.publish(_config.cloud, payload)) {
      _lastTransport = "mqtt";
      _lastResponse = "published";
      _queue.pop();
      return PublishResult::SUCCESS;
    }
    if (!_config.cloud.httpFallbackEnabled) {
      _lastTransport = "mqtt";
      _lastResponse = "mqtt_failed_no_http_fallback";
      return PublishResult::FAILED;
    }
  }

  if (_http.postJson(_config.cloud.baseUrl,
                     _config.cloud.telemetryPath,
                     _config.cloud.authToken,
                     payload,
                     _lastHttpCode,
                     _lastResponse)) {
    _lastTransport = "http";
    _queue.pop();
    return PublishResult::SUCCESS;
  }

  if (_config.cloud.mqttEnabled) {
    _lastTransport = "mqtt_http_fallback";
  } else {
    _lastTransport = "http";
  }
  return PublishResult::FAILED;
}

int PublishManager::lastHttpCode() const {
  return _lastHttpCode;
}

String PublishManager::lastResponse() const {
  return _lastResponse;
}

String PublishManager::lastTransport() const {
  return _lastTransport;
}
