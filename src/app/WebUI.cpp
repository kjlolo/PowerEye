#include "app/WebUI.h"
#include <math.h>
#include <WiFi.h>
#include <Update.h>

namespace {
String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("\"", "&quot;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  return value;
}

const char* statusClass(bool online) {
  return online ? "badge-ok" : "badge-off";
}

const char* contactModeValue(bool activeHigh) {
  return activeHigh ? "no" : "nc";
}

bool parseContactMode(const String& value) {
  return value.equalsIgnoreCase("no");
}

String contactModeSelectHtml(const String& name, const InputConfig& cfg) {
  String html;
  html += "<select name='" + name + "'>";
  html += "<option value='nc'";
  html += (!cfg.activeHigh ? " selected" : "");
  html += ">NC (active low)</option>";
  html += "<option value='no'";
  html += (cfg.activeHigh ? " selected" : "");
  html += ">NO (active high)</option></select>";
  return html;
}

String batteryModelSelectHtml(const String& name, BatteryModel model) {
  String html;
  html += "<select name='" + name + "'>";
  html += "<option value='none'";
  html += (model == BatteryModel::NONE ? " selected" : "");
  html += ">None</option>";
  html += "<option value='changhong'";
  html += (model == BatteryModel::CHANGHONG ? " selected" : "");
  html += ">Changhong</option>";
  html += "<option value='wolong'";
  html += (model == BatteryModel::WOLONG ? " selected" : "");
  html += ">Wolong</option></select>";
  return html;
}

String generatorModelSelectHtml(const String& name, GeneratorModel model) {
  String html;
  html += "<select name='" + name + "'>";
  html += "<option value='none'";
  html += (model == GeneratorModel::NONE ? " selected" : "");
  html += ">None</option>";
  html += "<option value='hgm6100nc'";
  html += (model == GeneratorModel::HGM6100NC ? " selected" : "");
  html += ">HGM6100NC</option></select>";
  return html;
}

String generatorRowsHtml(const Rs485Config& cfg) {
  String html;
  const uint8_t total = cfg.generatorCount > Rs485Config::MAX_GENERATORS
    ? Rs485Config::MAX_GENERATORS
    : cfg.generatorCount;
  for (uint8_t i = 0; i < total; ++i) {
    html += "<tr><td>Generator ";
    html += String(i + 1);
    html += "</td><td><input name='gen_sid_";
    html += String(i);
    html += "' type='number' min='1' max='247' value='";
    html += String(cfg.generatorSlaveIds[i]);
    html += "'></td><td>";
    html += generatorModelSelectHtml("gen_model_" + String(i), cfg.generatorModels[i]);
    html += "</td></tr>";
  }
  return html;
}

String batteryBankRowsHtml(const Rs485Config& cfg) {
  String html;
  const uint8_t totalBanks = cfg.batteryBankCount > Rs485Config::MAX_BATTERY_BANKS
    ? Rs485Config::MAX_BATTERY_BANKS
    : cfg.batteryBankCount;
  for (uint8_t i = 0; i < totalBanks; ++i) {
    const uint8_t rectifier = static_cast<uint8_t>((i / Rs485Config::BANKS_PER_RECTIFIER) + 1);
    const uint8_t bank = static_cast<uint8_t>((i % Rs485Config::BANKS_PER_RECTIFIER) + 1);
    html += "<tr><td>Rectifier ";
    html += String(rectifier);
    html += "</td><td>Bank ";
    html += String(bank);
    html += "</td><td><input name='batt_bank_sid_";
    html += String(i);
    html += "' type='number' min='1' max='247' value='";
    html += String(cfg.batteryBankSlaveIds[i]);
    html += "'></td><td>";
    html += batteryModelSelectHtml("batt_bank_model_" + String(i), cfg.batteryBankModels[i]);
    html += "</td></tr>";
  }
  return html;
}

const char* gensetModeLabel(const GensetData& g) {
  if (g.autoMode) return "AUTO";
  if (g.manualMode) return "MANUAL";
  if (g.stopMode) return "STOP";
  if (g.online) return "TEST/OTHER";
  return "UNKNOWN";
}

bool gensetHasAlarm(const GensetData& g) {
  return g.commonAlarm || g.commonWarn || g.commonShutdown;
}

const GensetData* primaryGenset(const TelemetrySnapshot& s) {
  const uint8_t count = s.gensetCountConfigured > Rs485Config::MAX_GENERATORS
    ? Rs485Config::MAX_GENERATORS
    : s.gensetCountConfigured;
  for (uint8_t i = 0; i < count; ++i) {
    if (s.gensets[i].online) return &s.gensets[i];
  }
  if (count > 0) return &s.gensets[0];
  return nullptr;
}

String dashboardBatteryGroupsHtml(const TelemetrySnapshot& s) {
  String html;
  const uint8_t count = s.batteryBankCountConfigured > Rs485Config::MAX_BATTERY_BANKS
    ? Rs485Config::MAX_BATTERY_BANKS
    : s.batteryBankCountConfigured;
  if (count == 0) {
    html += "<div class='meta'>No configured banks</div>";
    return html;
  }
  const uint8_t groups = static_cast<uint8_t>((count + Rs485Config::BANKS_PER_RECTIFIER - 1) / Rs485Config::BANKS_PER_RECTIFIER);
  for (uint8_t g = 0; g < groups; ++g) {
    html += "<div class='meta' style='margin-top:8px'>RS";
    html += String(g + 1);
    html += "</div>";
    html += "<div class='table-wrap'><table><thead><tr><th>Bank</th><th>Status</th><th>V</th><th>I</th><th>SOC</th><th>SOH</th></tr></thead><tbody>";
    const uint8_t start = g * Rs485Config::BANKS_PER_RECTIFIER;
    const uint8_t end = static_cast<uint8_t>(start + Rs485Config::BANKS_PER_RECTIFIER);
    for (uint8_t i = start; i < end && i < count; ++i) {
      const BatteryData& b = s.batteryBanks[i];
      const uint8_t bankNo = static_cast<uint8_t>((i % Rs485Config::BANKS_PER_RECTIFIER) + 1);
      html += "<tr>";
      html += "<td>" + String(bankNo) + "</td>";
      html += "<td><span class='badge ";
      html += (b.online ? "badge-ok" : "badge-off");
      html += "'>";
      html += (b.online ? "ONLINE" : "OFFLINE");
      html += "</span></td>";
      html += "<td>" + String(b.packVoltage, 2) + "</td>";
      html += "<td>" + String(b.packCurrent, 2) + "</td>";
      html += "<td>" + String(b.soc, 1) + "</td>";
      html += "<td>" + String(b.soh, 1) + "</td>";
      html += "</tr>";
    }
    html += "</tbody></table></div>";
  }
  return html;
}

float computedTankLiters(const FuelConfig& cfg) {
  if (cfg.tankLengthCm <= 0.0f || cfg.tankDiameterCm <= 0.0f) {
    return 0.0f;
  }
  const float r = cfg.tankDiameterCm / 2.0f;
  const float cm3 = static_cast<float>(M_PI) * r * r * cfg.tankLengthCm;
  return cm3 / 1000.0f;
}

String uiHead(const char* title) {
  String html;
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>";
  html += title;
  html += "</title>";
  html += R"HTML(<style>
:root{
  --bg:#07152f;
  --bg-soft:#10254a;
  --card:#102146;
  --text:#eef4ff;
  --muted:#99abc9;
  --line:#2a3f66;
  --accent:#ffd447;
  --accent-strong:#f9be00;
  --ok:#43d68c;
  --off:#f26e7d;
}
*{box-sizing:border-box}
body{
  margin:0;
  font-family:Segoe UI,Helvetica,Arial,sans-serif;
  background:
    radial-gradient(circle at 12% 10%, rgba(255,212,71,.18), transparent 40%),
    radial-gradient(circle at 88% 6%, rgba(255,212,71,.08), transparent 36%),
    linear-gradient(180deg, var(--bg-soft), var(--bg));
  color:var(--text);
}
.wrap{max-width:1100px;margin:0 auto;padding:18px 14px 40px}
.top{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:14px}
.title{margin:0;font-size:22px;letter-spacing:.2px}
.subtitle{margin:3px 0 0;color:var(--muted);font-size:12px}
.btn{
  display:inline-block;text-decoration:none;background:var(--accent);color:#09142c;font-weight:700;
  padding:10px 14px;border-radius:10px;border:1px solid #f0c62f
}
.btn-secondary{background:transparent;color:var(--accent);border-color:#e7c547}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:12px}
.grid-2{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
.card{
  background:linear-gradient(180deg, rgba(255,255,255,.03), rgba(255,255,255,.01));
  border:1px solid var(--line);border-radius:14px;padding:14px;backdrop-filter:blur(1px)
}
.card-compact{
  max-width:760px;
  margin:0 auto;
}
.label{color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:.8px}
.value{font-size:26px;font-weight:700;margin-top:6px;line-height:1.1}
.value-sm{font-size:16px}
.meta{margin-top:6px;color:var(--muted);font-size:12px}
.badge{
  display:inline-block;padding:4px 10px;border-radius:999px;font-size:11px;font-weight:700;letter-spacing:.6px
}
.badge-ok{background:rgba(67,214,140,.16);border:1px solid rgba(67,214,140,.5);color:var(--ok)}
.badge-off{background:rgba(242,110,125,.16);border:1px solid rgba(242,110,125,.55);color:var(--off)}
.section-title{margin:20px 0 10px;font-size:13px;color:var(--accent);letter-spacing:.8px;text-transform:uppercase}
.section-title-center{text-align:center}
form{margin:0}
.form-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:12px}
.field{display:flex;flex-direction:column;gap:6px}
.field label{font-size:12px;color:var(--muted)}
input,select{
  width:100%;padding:10px 11px;background:#0a1939;color:var(--text);
  border:1px solid var(--line);border-radius:10px;outline:none
}
input:focus,select:focus{border-color:var(--accent)}
.check{display:flex;align-items:center;gap:8px;padding-top:4px}
.check input{width:auto}
.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:16px}
.tip{font-size:12px;color:var(--muted);margin-top:10px}
.table-wrap{overflow:auto}
table{
  width:100%;
  border-collapse:collapse;
  border:1px solid var(--line);
  border-radius:10px;
  overflow:hidden;
}
th,td{
  padding:9px 10px;
  border-bottom:1px solid var(--line);
  text-align:left;
  font-size:13px;
}
th{color:var(--accent);font-weight:700;background:#0b1c3f}
#alarm-mode-table{
  table-layout:fixed;
}
#alarm-mode-table th:first-child,
#alarm-mode-table td:first-child{
  width:58%;
}
#alarm-mode-table th:last-child,
#alarm-mode-table td:last-child{
  width:42%;
}
#alarm-mode-table select{
  max-width:260px;
}
td input[readonly]{
  border:1px solid var(--line);
  background:#0b1c3f;
  color:var(--text);
  font-weight:700;
}
.modal-backdrop{
  position:fixed;inset:0;background:rgba(2,9,22,.76);display:none;align-items:center;justify-content:center;padding:14px;z-index:50
}
.modal{
  width:min(560px,100%);background:linear-gradient(180deg,#0f2147,#0a1939);
  border:1px solid var(--line);border-radius:14px;padding:14px
}
.modal h3{margin:0 0 6px;font-size:18px}
.modal p{margin:0 0 10px;color:var(--muted);font-size:12px}
.level-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(86px,1fr));gap:8px;margin-top:10px}
.level-btn{
  border:1px solid #f0c62f;background:#ffd447;color:#0a1631;font-weight:700;border-radius:10px;padding:10px
}
.level-btn:hover{background:#ffdf69}
.raw-box{margin-top:8px;padding:10px;border:1px solid var(--line);border-radius:10px;background:#0a1939}
.diag{margin-top:10px}
.diag pre{
  margin:8px 0 0;
  padding:10px;
  border:1px solid var(--line);
  border-radius:10px;
  background:#0a1939;
  color:var(--text);
  min-height:90px;
  max-height:220px;
  overflow:auto;
  font-size:12px;
  white-space:pre-wrap;
  word-break:break-word;
}
.kv{display:grid;grid-template-columns:1fr 1fr;gap:6px 10px;margin-top:8px}
.kv-row{display:flex;justify-content:space-between;gap:8px;font-size:12px;color:var(--muted)}
.kv-row b{color:var(--text);font-weight:700}
@media (max-width: 780px){
  .grid-2{grid-template-columns:1fr}
}
</style></head><body>)HTML";
  return html;
}
}

WebUI::WebUI(DeviceConfig& config, PreferencesStore& prefs, TelemetrySnapshot& snapshot, QueueStore& queue)
  : _server(80), _config(config), _prefs(prefs), _snapshot(snapshot), _queue(queue) {
  _diagMutex = xSemaphoreCreateMutex();
}

bool WebUI::isAuthorized(AsyncWebServerRequest* request) const {
  return request->authenticate(_config.ui.adminUser.c_str(), _config.ui.adminPassword.c_str());
}

void WebUI::begin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(_config.ui.apSsid.c_str(), _config.ui.apPassword.c_str(), 6);

  _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "text/html", dashboardHtml());
  });

  _server.on("/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!isAuthorized(request)) {
      return request->requestAuthentication();
    }
    request->send(200, "text/html", settingsHtml());
  });

  _server.on("/firmware", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!isAuthorized(request)) {
      return request->requestAuthentication();
    }
    request->send(200, "text/html", firmwareHtml());
  });

  _server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!isAuthorized(request)) {
      return request->requestAuthentication();
    }

    if (request->hasParam("site_name", true)) {
      _config.identity.siteName = request->getParam("site_name", true)->value();
    }
    if (request->hasParam("site_id", true)) {
      _config.identity.siteId = request->getParam("site_id", true)->value();
    }
    _config.ui.apSsid = buildApSsidFromSiteId(_config.identity.siteId);
    if (request->hasParam("device_id", true)) {
      _config.identity.deviceId = request->getParam("device_id", true)->value();
    }
    if (request->hasParam("base_url", true)) {
      _config.cloud.baseUrl = request->getParam("base_url", true)->value();
    }
    if (request->hasParam("telemetry_path", true)) {
      _config.cloud.telemetryPath = request->getParam("telemetry_path", true)->value();
    }
    if (request->hasParam("token", true)) {
      _config.cloud.authToken = request->getParam("token", true)->value();
    }
    if (request->hasParam("report_ms", true)) {
      _config.cloud.reportIntervalMs = request->getParam("report_ms", true)->value().toInt();
    }
    if (request->hasParam("payload_mode", true)) {
      const String mode = request->getParam("payload_mode", true)->value();
      _config.cloud.completePayload = mode.equalsIgnoreCase("complete");
    }
    _config.cloud.mcbeamCompatPayload = request->hasParam("payload_mcbeam", true);
    _config.cloud.mqttEnabled = request->hasParam("mqtt_en", true);
    if (request->hasParam("mqtt_host", true)) {
      _config.cloud.mqttHost = request->getParam("mqtt_host", true)->value();
      _config.cloud.mqttHost.trim();
    }
    if (request->hasParam("mqtt_port", true)) {
      const int port = request->getParam("mqtt_port", true)->value().toInt();
      if (port >= 1 && port <= 65535) {
        _config.cloud.mqttPort = static_cast<uint16_t>(port);
      }
    }
    _config.cloud.mqttTls = request->hasParam("mqtt_tls", true);
    if (request->hasParam("mqtt_client_id", true)) {
      _config.cloud.mqttClientId = request->getParam("mqtt_client_id", true)->value();
    }
    if (request->hasParam("mqtt_user", true)) {
      _config.cloud.mqttUsername = request->getParam("mqtt_user", true)->value();
    }
    if (request->hasParam("mqtt_pass", true)) {
      _config.cloud.mqttPassword = request->getParam("mqtt_pass", true)->value();
    }
    if (request->hasParam("mqtt_topic", true)) {
      _config.cloud.mqttTelemetryTopic = request->getParam("mqtt_topic", true)->value();
      _config.cloud.mqttTelemetryTopic.trim();
      if (_config.cloud.mqttTelemetryTopic.isEmpty()) {
        _config.cloud.mqttTelemetryTopic = "powereye/telemetry";
      }
    }
    if (request->hasParam("mqtt_cmd_topic", true)) {
      _config.cloud.mqttCmdTopic = request->getParam("mqtt_cmd_topic", true)->value();
      _config.cloud.mqttCmdTopic.trim();
      if (_config.cloud.mqttCmdTopic.isEmpty()) {
        _config.cloud.mqttCmdTopic = "powereye/cmd";
      }
    }
    if (request->hasParam("mqtt_status_topic", true)) {
      _config.cloud.mqttStatusTopic = request->getParam("mqtt_status_topic", true)->value();
      _config.cloud.mqttStatusTopic.trim();
      if (_config.cloud.mqttStatusTopic.isEmpty()) {
        _config.cloud.mqttStatusTopic = "powereye/status";
      }
    }
    _config.cloud.mqttMtlsEnabled = request->hasParam("mqtt_mtls", true);
    if (request->hasParam("mqtt_tls_sni", true)) {
      _config.cloud.mqttTlsHostname = request->getParam("mqtt_tls_sni", true)->value();
      _config.cloud.mqttTlsHostname.trim();
    }
    if (request->hasParam("mqtt_ca_pem", true)) {
      _config.cloud.mqttCaCertPem = request->getParam("mqtt_ca_pem", true)->value();
      _config.cloud.mqttCaCertPem.trim();
    }
    if (request->hasParam("mqtt_client_cert_pem", true)) {
      _config.cloud.mqttClientCertPem = request->getParam("mqtt_client_cert_pem", true)->value();
      _config.cloud.mqttClientCertPem.trim();
    }
    if (request->hasParam("mqtt_client_key_pem", true)) {
      _config.cloud.mqttClientKeyPem = request->getParam("mqtt_client_key_pem", true)->value();
      _config.cloud.mqttClientKeyPem.trim();
    }
    _config.cloud.httpFallbackEnabled = request->hasParam("http_fb", true);
    _config.rs485.pzemEnabled = request->hasParam("pzem_en", true);
    if (request->hasParam("pzem_sid", true)) {
      _config.rs485.pzemSlaveId = static_cast<uint8_t>(request->getParam("pzem_sid", true)->value().toInt());
    }
    _config.rs485.generatorEnabled = request->hasParam("gen_en", true);
    if (request->hasParam("gen_count", true)) {
      uint8_t requested = static_cast<uint8_t>(request->getParam("gen_count", true)->value().toInt());
      if (requested < 1) requested = 1;
      if (requested > Rs485Config::MAX_GENERATORS) requested = Rs485Config::MAX_GENERATORS;
      _config.rs485.generatorCount = requested;
    }
    for (uint8_t i = 0; i < Rs485Config::MAX_GENERATORS; ++i) {
      const String keySid = "gen_sid_" + String(i);
      if (request->hasParam(keySid, true)) {
        _config.rs485.generatorSlaveIds[i] = static_cast<uint8_t>(request->getParam(keySid, true)->value().toInt());
      }
      const String keyModel = "gen_model_" + String(i);
      if (request->hasParam(keyModel, true)) {
        _config.rs485.generatorModels[i] = generatorModelFromString(request->getParam(keyModel, true)->value());
      }
    }
    _config.rs485.batteryEnabled = request->hasParam("batt_en", true);
    if (request->hasParam("batt_rect_count", true)) {
      _config.rs485.rectifierCount = static_cast<uint8_t>(request->getParam("batt_rect_count", true)->value().toInt());
    }
    if (request->hasParam("batt_bank_count", true)) {
      uint8_t requested = static_cast<uint8_t>(request->getParam("batt_bank_count", true)->value().toInt());
      if (requested > Rs485Config::MAX_BATTERY_BANKS) requested = Rs485Config::MAX_BATTERY_BANKS;
      _config.rs485.batteryBankCount = requested;
    }
    const uint8_t rectLimit = static_cast<uint8_t>(_config.rs485.rectifierCount * Rs485Config::BANKS_PER_RECTIFIER);
    if (_config.rs485.batteryBankCount > rectLimit) {
      _config.rs485.batteryBankCount = rectLimit;
    }
    for (uint8_t i = 0; i < Rs485Config::MAX_BATTERY_BANKS; ++i) {
      const String key = "batt_bank_sid_" + String(i);
      if (request->hasParam(key, true)) {
        _config.rs485.batteryBankSlaveIds[i] = static_cast<uint8_t>(request->getParam(key, true)->value().toInt());
      }
      const String keyModel = "batt_bank_model_" + String(i);
      if (request->hasParam(keyModel, true)) {
        _config.rs485.batteryBankModels[i] = batteryModelFromString(request->getParam(keyModel, true)->value());
      }
    }
    bool used[248] = {};
    auto markUsed = [&](uint8_t sid) -> bool {
      if (sid < 1 || sid > 247) return false;
      if (used[sid]) return false;
      used[sid] = true;
      return true;
    };
    bool duplicateFound = false;
    if (_config.rs485.pzemEnabled && !markUsed(_config.rs485.pzemSlaveId)) duplicateFound = true;
    if (_config.rs485.generatorEnabled) {
      const uint8_t activeGenerators = _config.rs485.generatorCount;
      for (uint8_t i = 0; i < activeGenerators; ++i) {
        if (_config.rs485.generatorModels[i] == GeneratorModel::NONE) continue;
        if (!markUsed(_config.rs485.generatorSlaveIds[i])) {
          duplicateFound = true;
          break;
        }
      }
    }
    if (_config.rs485.batteryEnabled) {
      const uint8_t activeBanks = _config.rs485.batteryBankCount;
      for (uint8_t i = 0; i < activeBanks; ++i) {
        if (_config.rs485.batteryBankModels[i] == BatteryModel::NONE) continue;
        if (!markUsed(_config.rs485.batteryBankSlaveIds[i])) {
          duplicateFound = true;
          break;
        }
      }
    }
    if (duplicateFound) {
      return request->send(400, "text/plain", "Duplicate or invalid slave ID found. Ensure all enabled devices/banks use unique slave IDs.");
    }
    if (request->hasParam("rs485_baud", true)) {
      _config.rs485.baudRate = request->getParam("rs485_baud", true)->value().toInt();
    }
    _config.fuel.enabled = request->hasParam("fuel_en", true);
    if (request->hasParam("fuel_r0", true)) {
      _config.fuel.raw0 = request->getParam("fuel_r0", true)->value().toInt();
    }
    if (request->hasParam("fuel_r25", true)) {
      _config.fuel.raw25 = request->getParam("fuel_r25", true)->value().toInt();
    }
    if (request->hasParam("fuel_r50", true)) {
      _config.fuel.raw50 = request->getParam("fuel_r50", true)->value().toInt();
    }
    if (request->hasParam("fuel_r75", true)) {
      _config.fuel.raw75 = request->getParam("fuel_r75", true)->value().toInt();
    }
    if (request->hasParam("fuel_r100", true)) {
      _config.fuel.raw100 = request->getParam("fuel_r100", true)->value().toInt();
    }
    if (request->hasParam("fuel_tank_l", true)) {
      _config.fuel.tankLengthCm = request->getParam("fuel_tank_l", true)->value().toFloat();
    }
    if (request->hasParam("fuel_tank_d", true)) {
      _config.fuel.tankDiameterCm = request->getParam("fuel_tank_d", true)->value().toFloat();
    }
    if (request->hasParam("fuel_sensor_h", true)) {
      _config.fuel.sensorReachHeightCm = request->getParam("fuel_sensor_h", true)->value().toFloat();
    }
    if (request->hasParam("fuel_unreached_h", true)) {
      _config.fuel.sensorUnreachedHeightCm = request->getParam("fuel_unreached_h", true)->value().toFloat();
    }
    if (request->hasParam("fuel_dead", true)) {
      _config.fuel.deadSpaceLiters = request->getParam("fuel_dead", true)->value().toFloat();
    }
    if (request->hasParam("alarm_ac_mode", true)) {
      _config.alarms.acMains.activeHigh = parseContactMode(request->getParam("alarm_ac_mode", true)->value());
    }
    if (request->hasParam("alarm_run_mode", true)) {
      _config.alarms.gensetRun.activeHigh = parseContactMode(request->getParam("alarm_run_mode", true)->value());
    }
    if (request->hasParam("alarm_fail_mode", true)) {
      _config.alarms.gensetFail.activeHigh = parseContactMode(request->getParam("alarm_fail_mode", true)->value());
    }
    if (request->hasParam("alarm_batt_mode", true)) {
      _config.alarms.batteryTheft.activeHigh = parseContactMode(request->getParam("alarm_batt_mode", true)->value());
    }
    if (request->hasParam("alarm_cable_mode", true)) {
      _config.alarms.powerCableTheft.activeHigh = parseContactMode(request->getParam("alarm_cable_mode", true)->value());
    }
    if (request->hasParam("alarm_door_mode", true)) {
      _config.alarms.doorOpen.activeHigh = parseContactMode(request->getParam("alarm_door_mode", true)->value());
    }
    _prefs.save(_config);
    request->redirect("/");
  });

  _server.on("/fwupdate", HTTP_POST,
    [this](AsyncWebServerRequest* request) {
      if (!isAuthorized(request)) {
        return request->requestAuthentication();
      }
      if (_otaUploadOk) {
        request->send(200, "text/html",
          "<!doctype html><html><body style='font-family:Segoe UI,Arial;padding:20px;background:#07152f;color:#eef4ff'>"
          "<h3>Firmware updated successfully.</h3><p>Device is rebooting...</p>"
          "<p><a href='/firmware' style='color:#ffd447'>Return to firmware page</a></p></body></html>");
        request->onDisconnect([]() {
          delay(300);
          ESP.restart();
        });
      } else {
        String msg = _otaUploadError.isEmpty() ? "Firmware update failed." : _otaUploadError;
        request->send(500, "text/html",
          "<!doctype html><html><body style='font-family:Segoe UI,Arial;padding:20px;background:#07152f;color:#eef4ff'>"
          "<h3>Firmware update failed.</h3><pre style='white-space:pre-wrap'>" + htmlEscape(msg) + "</pre>"
          "<p><a href='/firmware' style='color:#ffd447'>Back to firmware page</a></p></body></html>");
      }
    },
    [this](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (!isAuthorized(request)) {
        return;
      }
      if (index == 0) {
        _otaUploadOk = false;
        _otaUploadError = "";
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          _otaUploadError = String("Update begin failed. Error ") + String(Update.getError());
          return;
        }
      }
      if (_otaUploadError.isEmpty() && len > 0) {
        const size_t written = Update.write(data, len);
        if (written != len) {
          _otaUploadError = String("Write failed. Error ") + String(Update.getError());
          Update.abort();
          return;
        }
      }
      if (final) {
        if (_otaUploadError.isEmpty() && Update.end(true)) {
          _otaUploadOk = true;
        } else {
          if (_otaUploadError.isEmpty()) {
            _otaUploadError = String("Finalize failed. Error ") + String(Update.getError());
          }
          Update.abort();
        }
      }
      (void)filename;
    });

  _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    String json = "{";
    json += "\"device_id\":\"" + _snapshot.deviceId + "\",";
    json += "\"site_id\":\"" + _snapshot.siteId + "\",";
    json += "\"site_name\":\"" + _snapshot.siteName + "\",";
    json += "\"queue\":" + String(_queue.size()) + ",";
    json += "\"network_online\":" + String(_snapshot.networkOnline ? "true" : "false") + ",";
    json += "\"transport_status\":\"" + _snapshot.transportStatus + "\",";
    json += "\"last_error\":\"" + _snapshot.lastError + "\",";
    json += "\"phone_number\":\"" + _snapshot.phoneNumber + "\",";
    json += "\"rssi\":" + String(_snapshot.rssi) + ",";
    json += "\"uptime_s\":" + String(_snapshot.uptimeMs / 1000UL) + ",";
    json += "\"energy_online\":" + String(_snapshot.energy.online ? "true" : "false") + ",";
    json += "\"energy_voltage\":" + String(_snapshot.energy.voltage, 2) + ",";
    json += "\"energy_power\":" + String(_snapshot.energy.power, 2) + ",";
    json += "\"energy_pf\":" + String(_snapshot.energy.powerFactor, 2) + ",";
    json += "\"energy_current\":" + String(_snapshot.energy.current, 2) + ",";
    json += "\"energy_frequency\":" + String(_snapshot.energy.frequency, 2) + ",";
    json += "\"energy_kwh\":" + String(_snapshot.energy.energyKwh, 3) + ",";
    json += "\"gensets\":[";
    const uint8_t gensetCount = _snapshot.gensetCountConfigured > Rs485Config::MAX_GENERATORS
      ? Rs485Config::MAX_GENERATORS
      : _snapshot.gensetCountConfigured;
    for (uint8_t i = 0; i < gensetCount; ++i) {
      const GensetData& g = _snapshot.gensets[i];
      if (i > 0) json += ",";
      json += "{";
      json += "\"index\":" + String(i + 1) + ",";
      json += "\"online\":" + String(g.online ? "true" : "false") + ",";
      json += "\"mode\":\"" + String(gensetModeLabel(g)) + "\",";
      json += "\"alarm\":" + String(gensetHasAlarm(g) ? "true" : "false") + ",";
      json += "\"voltage\":" + String(g.voltageA, 1) + ",";
      json += "\"current\":" + String(g.currentA, 2) + ",";
      json += "\"batt_voltage\":" + String(g.batteryVoltage, 2) + ",";
      json += "\"run_hours\":" + String(g.runHours);
      json += "}";
    }
    json += "],";
    json += "\"fuel_raw\":" + String(_snapshot.fuel.raw) + ",";
    json += "\"fuel_percent\":" + String(_snapshot.fuel.percent, 2) + ",";
    json += "\"fuel_liters\":" + String(_snapshot.fuel.liters, 2) + ",";
    json += "\"fuel_online\":" + String(_snapshot.fuel.online ? "true" : "false") + ",";
    json += "\"fuel_status\":\"" + String(_snapshot.fuel.online ? "ONLINE" : "OFFLINE") + "\",";
    json += "\"battery_banks\":[";
    const uint8_t banks = _snapshot.batteryBankCountConfigured > Rs485Config::MAX_BATTERY_BANKS
      ? Rs485Config::MAX_BATTERY_BANKS
      : _snapshot.batteryBankCountConfigured;
    for (uint8_t i = 0; i < banks; ++i) {
      const BatteryData& b = _snapshot.batteryBanks[i];
      if (i > 0) json += ",";
      json += "{";
      json += "\"index\":" + String(i + 1) + ",";
      json += "\"slave_id\":" + String(_snapshot.batteryBankSlaveIds[i]) + ",";
      json += "\"online\":" + String(b.online ? "true" : "false") + ",";
      json += "\"voltage\":" + String(b.packVoltage, 2) + ",";
      json += "\"current\":" + String(b.packCurrent, 2) + ",";
      json += "\"soc\":" + String(b.soc, 2) + ",";
      json += "\"soh\":" + String(b.soh, 2);
      json += "}";
    }
    json += "]";
    json += "}";
    request->send(200, "application/json", json);
  });

  _server.on("/api/network_diag", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (request->hasParam("action") && request->getParam("action")->value() == "start") {
      bool accepted = false;
      bool running = false;
      if (_diagMutex != nullptr && xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!_networkDiagRunning) {
          _networkDiagRequested = true;
          _networkDiagRunning = true;
          accepted = true;
        }
        running = _networkDiagRunning;
        xSemaphoreGive(_diagMutex);
      }
      String json = "{";
      json += "\"accepted\":" + String(accepted ? "true" : "false") + ",";
      json += "\"running\":" + String(running ? "true" : "false");
      json += "}";
      return request->send(200, "application/json", json);
    }
    request->send(200, "application/json", networkDiagStatusJson());
  });

  _server.on("/api/aws_test", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (request->hasParam("action") && request->getParam("action")->value() == "start") {
      bool accepted = false;
      bool running = false;
      if (_diagMutex != nullptr && xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!_awsTestRunning) {
          _awsTestRequested = true;
          _awsTestRunning = true;
          accepted = true;
        }
        running = _awsTestRunning;
        xSemaphoreGive(_diagMutex);
      }
      String json = "{";
      json += "\"accepted\":" + String(accepted ? "true" : "false") + ",";
      json += "\"running\":" + String(running ? "true" : "false");
      json += "}";
      return request->send(200, "application/json", json);
    }
    request->send(200, "application/json", awsTestStatusJson());
  });

  _server.on("/api/mqtt_test", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (request->hasParam("action") && request->getParam("action")->value() == "start") {
      bool accepted = false;
      bool running = false;
      if (_diagMutex != nullptr && xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!_mqttTestRunning) {
          _mqttTestRequested = true;
          _mqttTestRunning = true;
          accepted = true;
        }
        running = _mqttTestRunning;
        xSemaphoreGive(_diagMutex);
      }
      String json = "{";
      json += "\"accepted\":" + String(accepted ? "true" : "false") + ",";
      json += "\"running\":" + String(running ? "true" : "false");
      json += "}";
      return request->send(200, "application/json", json);
    }
    request->send(200, "application/json", mqttTestStatusJson());
  });

  _server.begin();
}

bool WebUI::consumeNetworkDiagRequest() {
  if (_diagMutex == nullptr) return false;
  if (xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  const bool requested = _networkDiagRequested;
  _networkDiagRequested = false;
  xSemaphoreGive(_diagMutex);
  return requested;
}

bool WebUI::consumeAwsTestRequest() {
  if (_diagMutex == nullptr) return false;
  if (xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  const bool requested = _awsTestRequested;
  _awsTestRequested = false;
  xSemaphoreGive(_diagMutex);
  return requested;
}

bool WebUI::consumeMqttTestRequest() {
  if (_diagMutex == nullptr) return false;
  if (xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  const bool requested = _mqttTestRequested;
  _mqttTestRequested = false;
  xSemaphoreGive(_diagMutex);
  return requested;
}

void WebUI::setNetworkDiagResult(const String& resultJson) {
  if (_diagMutex == nullptr) return;
  if (xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(200)) != pdTRUE) return;
  _networkDiagLastResult = resultJson;
  _networkDiagUpdatedMs = millis();
  _networkDiagRunning = false;
  xSemaphoreGive(_diagMutex);
}

void WebUI::setAwsTestResult(const String& resultJson) {
  if (_diagMutex == nullptr) return;
  if (xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(200)) != pdTRUE) return;
  _awsTestLastResult = resultJson;
  _awsTestUpdatedMs = millis();
  _awsTestRunning = false;
  xSemaphoreGive(_diagMutex);
}

void WebUI::setMqttTestResult(const String& resultJson) {
  if (_diagMutex == nullptr) return;
  if (xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(200)) != pdTRUE) return;
  _mqttTestLastResult = resultJson;
  _mqttTestUpdatedMs = millis();
  _mqttTestRunning = false;
  xSemaphoreGive(_diagMutex);
}

String WebUI::networkDiagStatusJson() {
  bool running = false;
  unsigned long updatedMs = 0;
  String result = "{}";
  if (_diagMutex != nullptr && xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    running = _networkDiagRunning;
    updatedMs = _networkDiagUpdatedMs;
    result = _networkDiagLastResult;
    xSemaphoreGive(_diagMutex);
  }
  String json = "{";
  json += "\"running\":" + String(running ? "true" : "false") + ",";
  json += "\"updated_ms\":" + String(updatedMs) + ",";
  json += "\"result\":" + result;
  json += "}";
  return json;
}

String WebUI::awsTestStatusJson() {
  bool running = false;
  unsigned long updatedMs = 0;
  String result = "{}";
  if (_diagMutex != nullptr && xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    running = _awsTestRunning;
    updatedMs = _awsTestUpdatedMs;
    result = _awsTestLastResult;
    xSemaphoreGive(_diagMutex);
  }
  String json = "{";
  json += "\"running\":" + String(running ? "true" : "false") + ",";
  json += "\"updated_ms\":" + String(updatedMs) + ",";
  json += "\"result\":" + result;
  json += "}";
  return json;
}

String WebUI::mqttTestStatusJson() {
  bool running = false;
  unsigned long updatedMs = 0;
  String result = "{}";
  if (_diagMutex != nullptr && xSemaphoreTake(_diagMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    running = _mqttTestRunning;
    updatedMs = _mqttTestUpdatedMs;
    result = _mqttTestLastResult;
    xSemaphoreGive(_diagMutex);
  }
  String json = "{";
  json += "\"running\":" + String(running ? "true" : "false") + ",";
  json += "\"updated_ms\":" + String(updatedMs) + ",";
  json += "\"result\":" + result;
  json += "}";
  return json;
}

String WebUI::dashboardHtml() const {
  String html = uiHead("Power Eye Dashboard");
  const GensetData* primaryGen = primaryGenset(_snapshot);
  const bool genOnline = primaryGen != nullptr && primaryGen->online;
  const String genMode = primaryGen != nullptr ? String(gensetModeLabel(*primaryGen)) : String("UNKNOWN");
  const bool genAlarm = primaryGen != nullptr && gensetHasAlarm(*primaryGen);
  const float genVoltage = primaryGen != nullptr ? primaryGen->voltageA : 0.0f;
  const float genBattVoltage = primaryGen != nullptr ? primaryGen->batteryVoltage : 0.0f;
  const float genCurrent = primaryGen != nullptr ? primaryGen->currentA : 0.0f;
  const uint32_t genRunHours = primaryGen != nullptr ? primaryGen->runHours : 0;
  html += "<div class='wrap'>";
  html += "<div class='top'>";
  html += "<div><h1 class='title'>Power Eye Dashboard</h1><p class='subtitle' id='site-line'>";
  html += htmlEscape(_snapshot.siteId) + " | " + htmlEscape(_snapshot.siteName);
  html += "</p></div>";
  html += "<a class='btn' href='/settings'>Configuration</a>";
  html += "</div>";

  html += "<div class='grid'>";
  html += "<div class='card'><div class='label'>Network</div><div class='value value-sm'><span id='network-badge' class='badge ";
  html += statusClass(_snapshot.networkOnline);
  html += "'>";
  html += (_snapshot.networkOnline ? "ONLINE" : "OFFLINE");
  html += "</span></div><div class='meta'>RSSI <span id='rssi-value'>";
  html += String(_snapshot.rssi);
  html += "</span> dBm</div><div class='meta'>Phone <span id='phone-number'>";
  html += htmlEscape(_snapshot.phoneNumber.length() > 0 ? _snapshot.phoneNumber : "N/A");
  html += "</span></div></div>";

  html += "<div class='card'><div class='label'>Queue Pending</div><div class='value' id='queue-value'>";
  html += String(_queue.size());
  html += "</div><div class='meta'>Records waiting for upload</div></div>";

  html += "<div class='card'><div class='label'>Device</div><div class='value value-sm' id='device-id'>";
  html += htmlEscape(_snapshot.deviceId);
  html += "</div><div class='meta'>Uptime <span id='uptime-s'>";
  html += String(_snapshot.uptimeMs / 1000UL);
  html += "</span> s</div></div>";

  html += "</div>";

  html += "<div class='section-title'>Subsystems</div><div class='grid-2'>";

  html += "<div class='card'><div class='label'>GRID</div><div class='value value-sm'><span id='energy-badge' class='badge ";
  html += statusClass(_snapshot.energy.online);
  html += "'>";
  html += (_snapshot.energy.online ? "ONLINE" : "OFFLINE");
  html += "</span></div><div class='kv'>";
  html += "<div class='kv-row'><span>Voltage</span><b><span id='energy-voltage-sub'>" + String(_snapshot.energy.voltage, 1) + "</span> V</b></div>";
  html += "<div class='kv-row'><span>Current</span><b><span id='energy-current'>" + String(_snapshot.energy.current, 2) + "</span> A</b></div>";
  html += "<div class='kv-row'><span>Power</span><b><span id='energy-power-sub'>" + String(_snapshot.energy.power, 1) + "</span> W</b></div>";
  html += "<div class='kv-row'><span>Frequency</span><b><span id='energy-frequency'>" + String(_snapshot.energy.frequency, 1) + "</span> Hz</b></div>";
  html += "<div class='kv-row'><span>Power Factor</span><b><span id='energy-pf-sub'>" + String(_snapshot.energy.powerFactor, 2) + "</span></b></div>";
  html += "<div class='kv-row'><span>Energy</span><b><span id='energy-kwh-sub'>" + String(_snapshot.energy.energyKwh, 3) + "</span> kWh</b></div>";
  html += "</div></div>";

  html += "<div class='card'><div class='label'>Generator</div><div class='value value-sm'><span id='genset-badge' class='badge ";
  html += statusClass(genOnline);
  html += "'>";
  html += (genOnline ? "ONLINE" : "OFFLINE");
  html += "</span></div><div class='kv'>";
  html += "<div class='kv-row'><span>Mode</span><b><span id='genset-mode'>" + genMode + "</span></b></div>";
  html += "<div class='kv-row'><span>Alarm</span><b><span id='genset-alarm'>" + String(genAlarm ? "YES" : "NO") + "</span></b></div>";
  html += "<div class='kv-row'><span>Generator Voltage</span><b><span id='genset-voltage'>" + String(genVoltage, 1) + "</span> V</b></div>";
  html += "<div class='kv-row'><span>Battery Voltage</span><b><span id='genset-batt-voltage'>" + String(genBattVoltage, 2) + "</span> V</b></div>";
  html += "<div class='kv-row'><span>Current</span><b><span id='genset-current'>" + String(genCurrent, 2) + "</span> A</b></div>";
  html += "<div class='kv-row'><span>Running Hours</span><b><span id='genset-run-hours'>" + String(genRunHours) + "</span></b></div>";
  html += "</div></div>";

  html += "<div class='card'><div class='label'>RECTIFIER</div>";
  html += "<div id='battery-banks-body'>";
  html += dashboardBatteryGroupsHtml(_snapshot);
  html += "</div></div>";

  html += "<div class='card'><div class='label'>Fuel</div><div class='value value-sm'><span id='fuel-badge' class='badge ";
  html += statusClass(_snapshot.fuel.online);
  html += "'>";
  html += (_snapshot.fuel.online ? "ONLINE" : "OFFLINE");
  html += "</span></div><div class='value'><span id='fuel-percent'>";
  html += String(_snapshot.fuel.percent, 1);
  html += "</span>%</div><div class='kv'>";
  html += "<div class='kv-row'><span>Liters</span><b><span id='fuel-liters'>" + String(_snapshot.fuel.liters, 1) + "</span> L</b></div>";
  html += "<div class='kv-row'><span>Raw</span><b><span id='fuel-raw-sub'>" + String(_snapshot.fuel.raw) + "</span></b></div>";
  html += "</div></div>";

  html += "</div>";
  html += "<div class='section-title'>Diagnostics</div>";
  html += "<div class='card diag'>";
  html += "<div class='label'>Network & AWS Tests</div>";
  html += "<div class='actions'><button class='btn btn-secondary' type='button' onclick='runNetworkDiag()'>Test Network</button>";
  html += "<button class='btn btn-secondary' type='button' onclick='runAwsTest()'>Test AWS Server</button>";
  html += "<button class='btn btn-secondary' type='button' onclick='runMqttTest()'>Test MQTT</button></div>";
  html += "<div class='meta'>Network Diagnostics</div><pre id='network-diag-output'>Press Test Network to run AT diagnostics.</pre>";
  html += "<div class='meta'>AWS Test Result</div><pre id='aws-test-output'>Press Test AWS Server to validate API reachability.</pre>";
  html += "<div class='meta'>MQTT Test Result</div><pre id='mqtt-test-output'>Press Test MQTT to validate broker connectivity and publish.</pre>";
  html += "</div>";
  html += R"HTML(<script>
const BADGE_OK = "badge badge-ok";
const BADGE_OFF = "badge badge-off";

function setText(id, value) {
  const el = document.getElementById(id);
  if (el) el.textContent = value;
}

function setBadge(id, online) {
  const el = document.getElementById(id);
  if (!el) return;
  el.className = online ? BADGE_OK : BADGE_OFF;
  el.textContent = online ? "ONLINE" : "OFFLINE";
}

function safeFormatDiag(obj) {
  try {
    return JSON.stringify(obj, null, 2);
  } catch (_) {
    return String(obj);
  }
}

function renderBatteryBanks(banks) {
  const body = document.getElementById('battery-banks-body');
  if (!body) return;
  if (!Array.isArray(banks) || banks.length === 0) {
    body.innerHTML = "<div class='meta'>No configured banks</div>";
    return;
  }
  const perRs = 4;
  const groups = Math.ceil(banks.length / perRs);
  let html = "";
  for (let g = 0; g < groups; g += 1) {
    html += `<div class='meta' style='margin-top:8px'>RS${g + 1}</div>`;
    html += "<div class='table-wrap'><table><thead><tr><th>Bank</th><th>Status</th><th>V</th><th>I</th><th>SOC</th><th>SOH</th></tr></thead><tbody>";
    for (let i = g * perRs; i < Math.min((g + 1) * perRs, banks.length); i += 1) {
      const b = banks[i] || {};
      const online = !!b.online;
      const statusClass = online ? "badge badge-ok" : "badge badge-off";
      const statusLabel = online ? "ONLINE" : "OFFLINE";
      const bankNo = (i % perRs) + 1;
      html += `<tr>
        <td>${bankNo}</td>
        <td><span class="${statusClass}">${statusLabel}</span></td>
        <td>${Number(b.voltage ?? 0).toFixed(2)}</td>
        <td>${Number(b.current ?? 0).toFixed(2)}</td>
        <td>${Number(b.soc ?? 0).toFixed(1)}</td>
        <td>${Number(b.soh ?? 0).toFixed(1)}</td>
      </tr>`;
    }
    html += "</tbody></table></div>";
  }
  body.innerHTML = html;
}

function pickPrimaryGenset(gensets) {
  if (!Array.isArray(gensets) || gensets.length === 0) return null;
  const online = gensets.find((g) => !!g.online);
  return online || gensets[0] || null;
}

async function runNetworkDiag() {
  const out = document.getElementById('network-diag-output');
  if (out) out.textContent = 'Running diagnostics...';
  try {
    const startRes = await fetch('/api/network_diag?action=start', { cache: 'no-store' });
    if (!startRes.ok) {
      if (out) out.textContent = `Network diagnostics start failed: HTTP ${startRes.status}`;
      return;
    }
    let attempts = 0;
    while (attempts < 40) {
      await new Promise((resolve) => setTimeout(resolve, 500));
      const res = await fetch('/api/network_diag', { cache: 'no-store' });
      if (!res.ok) {
        if (out) out.textContent = `Network diagnostics failed: HTTP ${res.status}`;
        return;
      }
      const diagState = await res.json();
      if (diagState.running) {
        attempts += 1;
        continue;
      }
      if (diagState.result) {
        if (out) out.textContent = safeFormatDiag(diagState.result);
      } else {
        if (out) out.textContent = safeFormatDiag(diagState);
      }
      return;
    }
    if (out) out.textContent = 'Network diagnostics timed out';
  } catch (e) {
    if (out) out.textContent = `Network diagnostics error: ${e}`;
  }
}

async function runAwsTest() {
  const out = document.getElementById('aws-test-output');
  if (out) out.textContent = 'Running AWS server test...';
  try {
    const startRes = await fetch('/api/aws_test?action=start', { cache: 'no-store' });
    if (!startRes.ok) {
      if (out) out.textContent = `AWS test start failed: HTTP ${startRes.status}`;
      return;
    }
    let attempts = 0;
    while (attempts < 60) {
      await new Promise((resolve) => setTimeout(resolve, 500));
      const res = await fetch('/api/aws_test', { cache: 'no-store' });
      if (!res.ok) {
        if (out) out.textContent = `AWS test failed: HTTP ${res.status}`;
        return;
      }
      const awsState = await res.json();
      if (awsState.running) {
        attempts += 1;
        continue;
      }
      if (awsState.result) {
        if (out) out.textContent = safeFormatDiag(awsState.result);
      } else {
        if (out) out.textContent = safeFormatDiag(awsState);
      }
      return;
    }
    if (out) out.textContent = 'AWS test timed out';
  } catch (e) {
    if (out) out.textContent = `AWS test error: ${e}`;
  }
}

async function runMqttTest() {
  const out = document.getElementById('mqtt-test-output');
  if (out) out.textContent = 'Running MQTT test...';
  try {
    const startRes = await fetch('/api/mqtt_test?action=start', { cache: 'no-store' });
    if (!startRes.ok) {
      if (out) out.textContent = `MQTT test start failed: HTTP ${startRes.status}`;
      return;
    }
    let attempts = 0;
    while (attempts < 60) {
      await new Promise((resolve) => setTimeout(resolve, 500));
      const res = await fetch('/api/mqtt_test', { cache: 'no-store' });
      if (!res.ok) {
        if (out) out.textContent = `MQTT test failed: HTTP ${res.status}`;
        return;
      }
      const mqttState = await res.json();
      if (mqttState.running) {
        attempts += 1;
        continue;
      }
      if (mqttState.result) {
        if (out) out.textContent = safeFormatDiag(mqttState.result);
      } else {
        if (out) out.textContent = safeFormatDiag(mqttState);
      }
      return;
    }
    if (out) out.textContent = 'MQTT test timed out';
  } catch (e) {
    if (out) out.textContent = `MQTT test error: ${e}`;
  }
}

async function refreshStatus() {
  try {
    const res = await fetch('/api/status', { cache: 'no-store' });
    if (!res.ok) return;
    const s = await res.json();
    setText('site-line', `${s.site_id} | ${s.site_name}`);
    setText('device-id', s.device_id);
    setText('rssi-value', s.rssi);
    setText('phone-number', s.phone_number || 'N/A');
    setText('queue-value', s.queue);
    setText('energy-voltage-sub', Number(s.energy_voltage).toFixed(1));
    setText('energy-power-sub', Number(s.energy_power).toFixed(1));
    setText('energy-pf-sub', Number(s.energy_pf).toFixed(2));
    setText('energy-current', Number(s.energy_current).toFixed(2));
    setText('energy-frequency', Number(s.energy_frequency).toFixed(1));
    setText('energy-kwh-sub', Number(s.energy_kwh).toFixed(3));
    setText('fuel-percent', Number(s.fuel_percent).toFixed(1));
    setText('fuel-liters', Number(s.fuel_liters).toFixed(1));
    setText('fuel-raw-sub', Number(s.fuel_raw).toFixed(0));
    setBadge('fuel-badge', !!s.fuel_online);
    const gen = pickPrimaryGenset(s.gensets);
    setText('genset-mode', (gen && gen.mode) ? gen.mode : 'UNKNOWN');
    setText('genset-voltage', Number(gen?.voltage ?? 0).toFixed(1));
    setText('genset-batt-voltage', Number(gen?.batt_voltage ?? 0).toFixed(2));
    setText('genset-current', Number(gen?.current ?? 0).toFixed(2));
    setText('genset-run-hours', Number(gen?.run_hours ?? 0).toFixed(0));
    setText('genset-alarm', (gen && gen.alarm) ? 'YES' : 'NO');
    setText('uptime-s', s.uptime_s);
    renderBatteryBanks(s.battery_banks);
    setBadge('network-badge', !!s.network_online);
    setBadge('energy-badge', !!s.energy_online);
    setBadge('genset-badge', !!gen?.online);
  } catch (_) {}
}

setInterval(refreshStatus, 5000);
refreshStatus();
</script>)HTML";
  html += "</div></body></html>";
  return html;
}

String WebUI::settingsHtml() const {
  String html = uiHead("Power Eye Settings");
  const float tankLiters = computedTankLiters(_config.fuel);
  html += "<div class='wrap'>";
  html += "<div class='top'>";
  html += "<div><h1 class='title'>Power Eye Configuration</h1><p class='subtitle'>Commissioning and telemetry setup</p></div>";
  html += "<div class='actions'><a class='btn btn-secondary' href='/firmware'>Firmware</a><a class='btn btn-secondary' href='/'>Back to Dashboard</a></div>";
  html += "</div>";

  html += "<form method='POST' action='/save'>";

  html += "<div class='section-title'>Identity</div><div class='card'><div class='form-grid'>";
  html += "<div class='field'><label>Device ID</label><input name='device_id' value='" + htmlEscape(_config.identity.deviceId) + "'></div>";
  html += "<div class='field'><label>Site ID</label><input name='site_id' value='" + htmlEscape(_config.identity.siteId) + "'></div>";
  html += "<div class='field'><label>Site Name</label><input name='site_name' value='" + htmlEscape(_config.identity.siteName) + "'></div>";
  html += "</div></div>";

  html += "<div class='section-title'>Cloud</div><div class='card'><div class='form-grid'>";
  html += "<div style='grid-column:1/-1'><div class='label'>HTTP Transport</div></div>";
  html += "<div class='field'><label>Base URL</label><input name='base_url' value='" + htmlEscape(_config.cloud.baseUrl) + "'></div>";
  html += "<div class='field'><label>Telemetry Path</label><input name='telemetry_path' value='" + htmlEscape(_config.cloud.telemetryPath) + "'></div>";
  html += "<div class='field'><label>Bearer Token</label><input name='token' value='" + htmlEscape(_config.cloud.authToken) + "'></div>";
  html += "<div class='field'><label>Report Interval (ms)</label><input name='report_ms' type='number' min='1000' value='" + String(_config.cloud.reportIntervalMs) + "'></div>";
  html += "<div class='field'><label>Telemetry Payload Mode</label><select name='payload_mode'>";
  html += "<option value='compact'";
  html += (_config.cloud.completePayload ? "" : " selected");
  html += ">Compact (default)</option>";
  html += "<option value='complete'";
  html += (_config.cloud.completePayload ? " selected" : "");
  html += ">Complete (full site dump)</option>";
  html += "</select></div>";
  html += "<div class='field'><label>Schema Compatibility</label><label class='check'><input type='checkbox' name='payload_mcbeam' ";
  html += (_config.cloud.mcbeamCompatPayload ? "checked" : "");
  html += ">Use McBeam-compatible JSON fields</label></div>";
  html += "<div style='grid-column:1/-1;height:1px;background:var(--line);margin:4px 0 6px'></div>";
  html += "<div style='grid-column:1/-1'><div class='label'>MQTT Transport</div></div>";
  html += "<div class='field'><label>MQTT Transport</label><label class='check'><input type='checkbox' name='mqtt_en' ";
  html += (_config.cloud.mqttEnabled ? "checked" : "");
  html += ">Enable MQTT Publish</label></div>";
  html += "<div class='field'><label>MQTT Broker Host/IP</label><input name='mqtt_host' value='" + htmlEscape(_config.cloud.mqttHost) + "'></div>";
  html += "<div class='field'><label>MQTT Broker Port</label><input name='mqtt_port' type='number' min='1' max='65535' value='" + String(_config.cloud.mqttPort) + "'></div>";
  html += "<div class='field'><label>MQTT Topic</label><input name='mqtt_topic' value='" + htmlEscape(_config.cloud.mqttTelemetryTopic) + "'></div>";
  html += "<div class='field'><label>MQTT CMD Topic (Step 2)</label><input name='mqtt_cmd_topic' value='" + htmlEscape(_config.cloud.mqttCmdTopic) + "'></div>";
  html += "<div class='field'><label>MQTT Status Topic (Step 2)</label><input name='mqtt_status_topic' value='" + htmlEscape(_config.cloud.mqttStatusTopic) + "'></div>";
  html += "<div class='field'><label>MQTT Client ID (optional)</label><input name='mqtt_client_id' value='" + htmlEscape(_config.cloud.mqttClientId) + "'></div>";
  html += "<div class='field'><label>MQTT Username (optional)</label><input name='mqtt_user' value='" + htmlEscape(_config.cloud.mqttUsername) + "'></div>";
  html += "<div class='field'><label>MQTT Password (optional)</label><input name='mqtt_pass' value='" + htmlEscape(_config.cloud.mqttPassword) + "'></div>";
  html += "<div class='field'><label>MQTT TLS</label><label class='check'><input type='checkbox' name='mqtt_tls' ";
  html += (_config.cloud.mqttTls ? "checked" : "");
  html += ">Use TLS (recommended)</label></div>";
  html += "<div class='field'><label>MQTT mTLS (Client Certificate)</label><label class='check'><input type='checkbox' name='mqtt_mtls' ";
  html += (_config.cloud.mqttMtlsEnabled ? "checked" : "");
  html += ">Enable client certificate authentication</label></div>";
  html += "<div class='field'><label>TLS Hostname / SNI (optional)</label><input name='mqtt_tls_sni' value='" + htmlEscape(_config.cloud.mqttTlsHostname) + "'></div>";
  html += "<div class='field' style='grid-column:1/-1'><label>CA Certificate PEM (optional)</label><textarea name='mqtt_ca_pem' rows='5' placeholder='-----BEGIN CERTIFICATE-----'>" + htmlEscape(_config.cloud.mqttCaCertPem) + "</textarea></div>";
  html += "<div class='field' style='grid-column:1/-1'><label>Client Certificate PEM (for mTLS)</label><textarea name='mqtt_client_cert_pem' rows='5' placeholder='-----BEGIN CERTIFICATE-----'>" + htmlEscape(_config.cloud.mqttClientCertPem) + "</textarea></div>";
  html += "<div class='field' style='grid-column:1/-1'><label>Client Private Key PEM (for mTLS)</label><textarea name='mqtt_client_key_pem' rows='6' placeholder='-----BEGIN PRIVATE KEY-----'>" + htmlEscape(_config.cloud.mqttClientKeyPem) + "</textarea></div>";
  html += "<div class='field'><label>HTTP Fallback</label><label class='check'><input type='checkbox' name='http_fb' ";
  html += (_config.cloud.httpFallbackEnabled ? "checked" : "");
  html += ">Use HTTP when MQTT publish fails</label></div>";
  html += "</div></div>";

  html += "<div class='section-title'>Import Device Secrets</div><div class='card card-compact'>";
  html += "<p class='tip'>Paste the generated device_secrets.h text, then click Import. Review values and click Save Configuration.</p>";
  html += "<div class='field'><label>device_secrets.h content</label><textarea id='device-secrets-input' rows='8' placeholder='#define MQTT_USERNAME \"...\"'></textarea></div>";
  html += "<div class='actions'><button class='btn btn-secondary' type='button' onclick='importDeviceSecrets()'>Import From Text</button>";
  html += "<span id='device-secrets-status' class='tip' style='margin-left:10px'>No import yet.</span></div>";
  html += "</div>";

  html += "<div class='section-title'>RS485 Network</div><div class='card'><div class='form-grid'>";
  html += "<div class='field'><label>RS485 Baud</label><input name='rs485_baud' type='number' min='1200' value='" + String(_config.rs485.baudRate) + "'></div>";
  html += "</div><p class='tip'>Shared bus settings used by PZEM, generator modules and rectifier battery modules.</p></div>";

  html += "<div class='section-title'>PZEM Meter</div><div class='card'><div class='form-grid'>";
  html += "<div class='field'><label>PZEM Slave ID</label><input name='pzem_sid' type='number' min='1' max='247' value='" + String(_config.rs485.pzemSlaveId) + "'>";
  html += "<label class='check'><input type='checkbox' name='pzem_en' ";
  html += (_config.rs485.pzemEnabled ? "checked" : "");
  html += ">Enable PZEM</label></div>";
  html += "</div></div>";

  html += "<div class='section-title'>Generator Modules</div><div class='card'><div class='form-grid'>";
  html += "<div class='field'><label>Generator Monitor</label><label class='check'><input type='checkbox' name='gen_en' ";
  html += (_config.rs485.generatorEnabled ? "checked" : "");
  html += ">Enable Generator</label></div>";
  html += "<div class='field'><label>Generator Count</label><select name='gen_count'>";
  for (int i = 1; i <= Rs485Config::MAX_GENERATORS; ++i) {
    html += "<option value='" + String(i) + "'";
    html += (_config.rs485.generatorCount == i ? " selected" : "");
    html += ">" + String(i) + "</option>";
  }
  html += "</select></div>";
  html += "</div>";
  html += "<div class='table-wrap'><table><thead><tr><th>Generator</th><th>Slave ID</th><th>Model</th></tr></thead><tbody>";
  html += generatorRowsHtml(_config.rs485);
  html += "</tbody></table></div><p class='tip'>Configure slave ID and model per generator module.</p></div>";

  html += "<div class='section-title'>Rectifier Battery Modules</div><div class='card'><div class='form-grid'>";
  html += "<div class='field'><label>Battery Monitor</label><label class='check'><input type='checkbox' name='batt_en' ";
  html += (_config.rs485.batteryEnabled ? "checked" : "");
  html += ">Enable Battery Monitor</label></div>";
  html += "<div class='field'><label>Rectifier Count</label><select name='batt_rect_count'>";
  for (int i = 1; i <= 4; ++i) {
    html += "<option value='" + String(i) + "'";
    html += (_config.rs485.rectifierCount == i ? " selected" : "");
    html += ">" + String(i) + "</option>";
  }
  html += "</select></div>";
  html += "<div class='field'><label>Battery Bank Count</label><select name='batt_bank_count'>";
  for (int i = 1; i <= Rs485Config::MAX_BATTERY_BANKS; ++i) {
    html += "<option value='" + String(i) + "'";
    html += (_config.rs485.batteryBankCount == i ? " selected" : "");
    html += ">" + String(i) + "</option>";
  }
  html += "</select></div>";
  html += "</div>";
  html += "<div class='table-wrap'><table><thead><tr><th>Group</th><th>Bank</th><th>Slave ID</th><th>Battery Model</th></tr></thead><tbody>";
  html += batteryBankRowsHtml(_config.rs485);
  html += "</tbody></table></div><p class='tip'>Battery banks are grouped 4 banks per rectifier.</p></div>";

  html += "<div class='section-title'>Fuel Sensor</div><div class='card'><div class='form-grid'>";
  html += "<div class='field'><label>Tank Length (cm)</label><input name='fuel_tank_l' type='number' step='0.1' min='0' value='" + String(_config.fuel.tankLengthCm, 1) + "'></div>";
  html += "<div class='field'><label>Tank Diameter (cm) - horizontal cylinder</label><input name='fuel_tank_d' type='number' step='0.1' min='0' value='" + String(_config.fuel.tankDiameterCm, 1) + "'></div>";
  html += "<div class='field'><label>Sensor Reach / Float Travel (cm)</label><input name='fuel_sensor_h' type='number' step='0.1' min='0' value='" + String(_config.fuel.sensorReachHeightCm, 1) + "'></div>";
  html += "<div class='field'><label>Sensor Unreached Height (cm, fallback)</label><input name='fuel_unreached_h' type='number' step='0.1' min='0' value='" + String(_config.fuel.sensorUnreachedHeightCm, 1) + "'></div>";
  html += "<div class='field'><label>Tank Liters @ 100% (computed)</label><input type='number' readonly value='" + String(tankLiters, 1) + "'></div>";
  html += "<div class='field'><label>Dead Space Liters (manual fallback)</label><input name='fuel_dead' type='number' step='0.1' min='0' value='" + String(_config.fuel.deadSpaceLiters, 1) + "'>";
  html += "<label class='check'><input type='checkbox' name='fuel_en' ";
  html += (_config.fuel.enabled ? "checked" : "");
  html += ">Enable Fuel Sensor</label></div>";
  html += "<div class='field'><label>Fuel Calibration Tool</label><button class='btn btn-secondary' type='button' onclick='openFuelCalModal()'>Calibrate from Live Raw</button>";
  html += "<div class='tip'>Capture points at 0/25/50/75/100% of sensor stroke height. Hold level steady 12-20s before capture.</div></div>";
  html += "</div>";
  html += "<div class='table-wrap'><table><thead><tr><th>Sensor Stroke Level</th><th>Raw Value</th></tr></thead><tbody>";
  html += "<tr><td>100% stroke</td><td><input name='fuel_r100' type='number' readonly value='" + String(_config.fuel.raw100) + "'></td></tr>";
  html += "<tr><td>75% stroke</td><td><input name='fuel_r75' type='number' readonly value='" + String(_config.fuel.raw75) + "'></td></tr>";
  html += "<tr><td>50% stroke</td><td><input name='fuel_r50' type='number' readonly value='" + String(_config.fuel.raw50) + "'></td></tr>";
  html += "<tr><td>25% stroke</td><td><input name='fuel_r25' type='number' readonly value='" + String(_config.fuel.raw25) + "'></td></tr>";
  html += "<tr><td>0% stroke</td><td><input name='fuel_r0' type='number' readonly value='" + String(_config.fuel.raw0) + "'></td></tr>";
  html += "</tbody></table></div>";
  html += "<p class='tip'>Liters are derived from horizontal-cylinder geometry using calibrated sensor height plus dead-space compensation. Use fuel-drop checks (e.g., 20L) to validate field accuracy.</p>";
  html += "</div></div>";

  html += "<div class='section-title section-title-center'>Digital Alarm Contact Mode</div><div class='card card-compact'>";
  html += "<div class='table-wrap'><table id='alarm-mode-table'><thead><tr><th>Alarm Input</th><th>Contact Mode</th></tr></thead><tbody>";
  html += "<tr><td>AC Mains Rectifier</td><td>";
  html += contactModeSelectHtml("alarm_ac_mode", _config.alarms.acMains);
  html += "</td></tr>";
  html += "<tr><td>Genset Operation</td><td>";
  html += contactModeSelectHtml("alarm_run_mode", _config.alarms.gensetRun);
  html += "</td></tr>";
  html += "<tr><td>Genset Failed</td><td>";
  html += contactModeSelectHtml("alarm_fail_mode", _config.alarms.gensetFail);
  html += "</td></tr>";
  html += "<tr><td>Battery Theft</td><td>";
  html += contactModeSelectHtml("alarm_batt_mode", _config.alarms.batteryTheft);
  html += "</td></tr>";
  html += "<tr><td>Power Cable Theft</td><td>";
  html += contactModeSelectHtml("alarm_cable_mode", _config.alarms.powerCableTheft);
  html += "</td></tr>";
  html += "<tr><td>RS Door Open</td><td>";
  html += contactModeSelectHtml("alarm_door_mode", _config.alarms.doorOpen);
  html += "</td></tr>";
  html += "</tbody></table></div>";
  html += "<p class='tip'>NC uses active low input logic. NO uses active high input logic.</p>";
  html += "<p class='tip'>Changes are saved to local preferences immediately. Polling behavior updates without reflashing.</p>";
  html += "</div>";

  html += R"HTML(
<div id='fuel-cal-modal' class='modal-backdrop' onclick='closeFuelCalModal(event)'>
  <div class='modal'>
    <h3>Fuel Calibration</h3>
    <p>Tap a level button to sample live raw value from GPIO and assign it to that calibration point.</p>
    <div class='raw-box'>Current sampled raw: <b id='fuel-raw-live'>-</b></div>
    <div class='level-grid'>
      <button type='button' class='level-btn' onclick='captureFuelRaw(100)'>Set 100%</button>
      <button type='button' class='level-btn' onclick='captureFuelRaw(75)'>Set 75%</button>
      <button type='button' class='level-btn' onclick='captureFuelRaw(50)'>Set 50%</button>
      <button type='button' class='level-btn' onclick='captureFuelRaw(25)'>Set 25%</button>
      <button type='button' class='level-btn' onclick='captureFuelRaw(0)'>Set 0%</button>
    </div>
    <div class='actions'>
      <button type='button' class='btn btn-secondary' onclick='captureFuelRaw()'>Read Raw Only</button>
      <button type='button' class='btn' onclick='closeFuelCalModal()'>Done</button>
    </div>
  </div>
</div>
)HTML";

  html += "<div class='actions'><button class='btn' type='submit'>Save Configuration</button><a class='btn btn-secondary' href='/'>Cancel</a></div>";
  html += R"HTML(
</form>
<script>
function fuelInputByLevel(level) {
  if (level === 100) return document.querySelector("input[name='fuel_r100']");
  if (level === 75) return document.querySelector("input[name='fuel_r75']");
  if (level === 50) return document.querySelector("input[name='fuel_r50']");
  if (level === 25) return document.querySelector("input[name='fuel_r25']");
  if (level === 0) return document.querySelector("input[name='fuel_r0']");
  return null;
}

function openFuelCalModal() {
  const modal = document.getElementById('fuel-cal-modal');
  if (modal) modal.style.display = 'flex';
  captureFuelRaw();
}

function closeFuelCalModal(ev) {
  if (ev && ev.target && ev.target.id !== 'fuel-cal-modal') return;
  const modal = document.getElementById('fuel-cal-modal');
  if (modal) modal.style.display = 'none';
}

async function captureFuelRaw(level) {
  try {
    const res = await fetch('/api/status', { cache: 'no-store' });
    if (!res.ok) return;
    const s = await res.json();
    const raw = Number(s.fuel_raw);
    const rawBox = document.getElementById('fuel-raw-live');
    if (rawBox) rawBox.textContent = Number.isFinite(raw) ? String(raw) : '-';
    if (typeof level === 'number') {
      const input = fuelInputByLevel(level);
      if (input && Number.isFinite(raw)) {
        input.value = String(raw);
      }
    }
  } catch (_) {}
}

function setInputValueByName(name, value) {
  if (!value) return false;
  const el = document.querySelector(`input[name='${name}']`);
  if (!el) return false;
  el.value = value;
  return true;
}

function parseDeviceSecretsDefines(text) {
  const defs = {};
  const pattern = /#define\s+([A-Za-z0-9_]+)\s+"([^"]*)"/g;
  let m;
  while ((m = pattern.exec(text)) !== null) {
    defs[m[1]] = m[2];
  }
  return defs;
}

function parseRawPemBlock(text, symbolName) {
  const escaped = symbolName.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const re = new RegExp(`static\\s+const\\s+char\\s+${escaped}\\s*\\[\\]\\s*PROGMEM\\s*=\\s*R\"EOF\\(([\\s\\S]*?)\\)EOF\";`, 'm');
  const m = text.match(re);
  if (!m || !m[1]) return '';
  return m[1].trim();
}

function importDeviceSecrets() {
  const input = document.getElementById('device-secrets-input');
  const status = document.getElementById('device-secrets-status');
  if (!input) return;
  const rawText = input.value || '';
  const defs = parseDeviceSecretsDefines(rawText);
  let applied = 0;

  const mappings = [
    ['DEVICE_SITE_ID', 'site_id'],
    ['MQTT_CLIENT_ID', 'mqtt_client_id'],
    ['MQTT_USERNAME', 'mqtt_user'],
    ['MQTT_PASSWORD', 'mqtt_pass'],
    ['MQTT_TOPIC', 'mqtt_topic'],
    ['MQTT_CMD_TOPIC', 'mqtt_cmd_topic'],
    ['MQTT_STATUS_TOPIC', 'mqtt_status_topic']
  ];

  for (const [fromKey, toInput] of mappings) {
    if (Object.prototype.hasOwnProperty.call(defs, fromKey)) {
      if (setInputValueByName(toInput, defs[fromKey])) {
        applied += 1;
      }
    }
  }

  const clientCertPem = parseRawPemBlock(rawText, 'DEVICE_CLIENT_CERT');
  const clientKeyPem = parseRawPemBlock(rawText, 'DEVICE_CLIENT_KEY');
  const caPem = parseRawPemBlock(rawText, 'MQTT_ROOT_CA');

  const certArea = document.querySelector("textarea[name='mqtt_client_cert_pem']");
  if (certArea && clientCertPem) {
    certArea.value = clientCertPem;
    applied += 1;
  }
  const keyArea = document.querySelector("textarea[name='mqtt_client_key_pem']");
  if (keyArea && clientKeyPem) {
    keyArea.value = clientKeyPem;
    applied += 1;
  }
  const caArea = document.querySelector("textarea[name='mqtt_ca_pem']");
  if (caArea && caPem) {
    caArea.value = caPem;
    applied += 1;
  }

  const mtlsEnable = document.querySelector("input[name='mqtt_mtls']");
  if (mtlsEnable && clientCertPem && clientKeyPem) {
    mtlsEnable.checked = true;
  }

  const mqttEnable = document.querySelector("input[name='mqtt_en']");
  if (mqttEnable && applied > 0) {
    mqttEnable.checked = true;
  }

  if (status) {
    if (applied > 0) {
      status.textContent = `Imported ${applied} field(s). Click Save Configuration.`;
    } else {
      status.textContent = 'No supported #define entries found.';
    }
  }
}
</script>
</div></body></html>)HTML";
  return html;
}

String WebUI::firmwareHtml() const {
  String html = uiHead("Power Eye Firmware Update");
  html += "<div class='wrap'>";
  html += "<div class='top'>";
  html += "<div><h1 class='title'>Firmware Update</h1><p class='subtitle'>Local OTA upload over Wi-Fi</p></div>";
  html += "<div class='actions'><a class='btn btn-secondary' href='/settings'>Back to Settings</a><a class='btn btn-secondary' href='/'>Dashboard</a></div>";
  html += "</div>";
  html += "<div class='card card-compact'>";
  html += "<form method='POST' action='/fwupdate' enctype='multipart/form-data'>";
  html += "<div class='form-grid'>";
  html += "<div class='field'><label>Firmware Binary (.bin)</label><input type='file' name='firmware' accept='.bin' required></div>";
  html += "</div>";
  html += "<div class='actions'><button class='btn' type='submit'>Upload and Flash</button></div>";
  html += "<p class='tip'>Use a firmware .bin built for this exact board and partition layout. Device reboots automatically after successful update.</p>";
  html += "</form>";
  html += "</div>";
  html += "</div></body></html>";
  return html;
}
