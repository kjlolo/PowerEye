#include "comms/MqttClient.h"

MqttClient::MqttClient(Air780E& modem) : _modem(modem) {}

bool MqttClient::publish(const CloudConfig& cloud, const String& payload) {
  String clientId = cloud.mqttClientId;
  if (clientId.isEmpty()) {
    clientId = "powereye";
  }
  return _modem.mqttPublish(
    cloud.mqttHost,
    cloud.mqttPort,
    cloud.mqttTls,
    clientId,
    cloud.mqttUsername,
    cloud.mqttPassword,
    cloud.mqttTelemetryTopic,
    payload
  );
}
