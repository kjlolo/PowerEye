#include "comms/MqttClient.h"
#include "comms/mqtt_root_ca.h"

MqttClient::MqttClient(Air780E& modem) : _modem(modem) {}

namespace {
String effectiveCaPem(const CloudConfig& cloud) {
  if (!cloud.mqttCaCertPem.isEmpty()) {
    return cloud.mqttCaCertPem;
  }
  return String(MQTT_ROOT_CA);
}
}

String MqttClient::resolveClientId(const CloudConfig& cloud) const {
  String clientId = cloud.mqttClientId;
  if (clientId.isEmpty()) {
    clientId = "powereye";
  }
  return clientId;
}

bool MqttClient::ensureControlChannel(const CloudConfig& cloud) {
  const String clientId = resolveClientId(cloud);
  if (!_modem.mqttEnsureConnected(
      cloud.mqttHost,
      cloud.mqttPort,
      cloud.mqttTls,
      clientId,
      cloud.mqttUsername,
      cloud.mqttPassword,
      cloud.mqttMtlsEnabled,
      cloud.mqttTlsHostname,
      effectiveCaPem(cloud),
      cloud.mqttClientCertPem,
      cloud.mqttClientKeyPem)) {
    _controlReady = false;
    _statusPublished = false;
    return false;
  }

  const String newKey = cloud.mqttHost + ":" + String(cloud.mqttPort) + "|" + clientId + "|" + cloud.mqttCmdTopic;
  if (newKey != _controlKey) {
    _controlKey = newKey;
    _statusPublished = false;
  }

  if (!cloud.mqttCmdTopic.isEmpty()) {
    if (!_modem.mqttSubscribe(cloud.mqttCmdTopic, 1)) {
      _controlReady = false;
      return false;
    }
  }

  _controlReady = true;
  return true;
}

bool MqttClient::publish(const CloudConfig& cloud, const String& payload) {
  const String clientId = resolveClientId(cloud);
  return _modem.mqttPublish(
    cloud.mqttHost,
    cloud.mqttPort,
    cloud.mqttTls,
    clientId,
    cloud.mqttUsername,
    cloud.mqttPassword,
    cloud.mqttTelemetryTopic,
    payload,
    cloud.mqttMtlsEnabled,
    cloud.mqttTlsHostname,
    effectiveCaPem(cloud),
    cloud.mqttClientCertPem,
    cloud.mqttClientKeyPem
  );
}

bool MqttClient::publishStatus(const CloudConfig& cloud, const String& statusPayload) {
  if (cloud.mqttStatusTopic.isEmpty()) {
    return false;
  }
  const String clientId = resolveClientId(cloud);
  const bool ok = _modem.mqttPublish(
    cloud.mqttHost,
    cloud.mqttPort,
    cloud.mqttTls,
    clientId,
    cloud.mqttUsername,
    cloud.mqttPassword,
    cloud.mqttStatusTopic,
    statusPayload,
    cloud.mqttMtlsEnabled,
    cloud.mqttTlsHostname,
    effectiveCaPem(cloud),
    cloud.mqttClientCertPem,
    cloud.mqttClientKeyPem
  );
  if (ok) {
    _statusPublished = true;
  }
  return ok;
}

bool MqttClient::pollCommand(String& topic, String& payload) {
  if (!_controlReady) {
    return false;
  }
  return _modem.mqttPollMessage(topic, payload);
}
