import { useEffect, useState } from "react";
import api from "../api";

export default function SiteDashboardPage() {
  const [siteId, setSiteId] = useState("");
  const [sites, setSites] = useState([]);
  const [latest, setLatest] = useState(null);
  const [live, setLive] = useState(null);

  const loadSites = async () => {
    const { data } = await api.get("/sites");
    setSites(data.items || []);
    if (!siteId && data.items?.length) setSiteId(data.items[0].site_id);
  };

  const loadData = async (selected) => {
    if (!selected) return;
    const [latestRes, liveRes] = await Promise.all([
      api.get(`/fleet/sites/${selected}/latest`),
      api.get(`/fleet/sites/${selected}/live`),
    ]);
    setLatest(latestRes.data.item);
    setLive(liveRes.data);
  };

  useEffect(() => {
    loadSites();
  }, []);

  useEffect(() => {
    loadData(siteId);
  }, [siteId]);

  useEffect(() => {
    if (!siteId) return undefined;
    const timer = setInterval(() => {
      loadData(siteId);
    }, 15000);
    return () => clearInterval(timer);
  }, [siteId]);

  const values = live?.values || {};
  const gridVoltage = Number(values.grid_voltage ?? 0);
  const gridPower = Number(values.grid_power ?? 0);
  const fuelPercent = Number(values.fuel_percent ?? 0);
  const fuelLiters = Number(values.fuel_liters ?? 0);
  const gensetOnline = Number(values.genset_online_count ?? 0);
  const batteryOnline = Number(values.battery_online_count ?? 0);
  const batteryBanksConfigured = Number(values.battery_bank_count_configured ?? 0);
  const batteryLowSocCount = Number(values.battery_low_soc_count ?? 0);
  const heartbeatOnline = !!live?.network_heartbeat_online;
  const lastTelemetryAt = live?.last_telemetry_at ? new Date(live.last_telemetry_at).toLocaleString() : "-";
  const telemetryAgeSec = live?.last_telemetry_age_sec;
  const telemetryAgeText = Number.isFinite(telemetryAgeSec) ? `${telemetryAgeSec}s ago` : "-";
  const telemetryFresh = Number.isFinite(telemetryAgeSec) ? telemetryAgeSec <= 120 : false;
  const gridOnline = !!values.grid_online || gridVoltage > 150;
  const fuelOnline = !!values.fuel_online || !!values.fuel_sensor_online;
  const gensetAlarm = !!values.genset_alarm || !!values.genset_any_alarm;
  const powerSource = String(values.power_source || "-");

  const rsRows = [
    { name: "RS1", onlineCount: Number(values.rs1_online_count ?? 0), avgSoc: Number(values.rs1_avg_soc ?? 0) },
    { name: "RS2", onlineCount: Number(values.rs2_online_count ?? 0), avgSoc: Number(values.rs2_avg_soc ?? 0) },
    { name: "RS3", onlineCount: Number(values.rs3_online_count ?? 0), avgSoc: Number(values.rs3_avg_soc ?? 0) },
    { name: "RS4", onlineCount: Number(values.rs4_online_count ?? 0), avgSoc: Number(values.rs4_avg_soc ?? 0) },
  ];

  return (
    <div>
      <div className="topbar">
        <h2>Per-Site Dashboard</h2>
        <select value={siteId} onChange={(e) => setSiteId(e.target.value)}>
          {sites.map((s) => (
            <option key={s.site_id} value={s.site_id}>
              {s.site_id} - {s.site_name}
            </option>
          ))}
        </select>
      </div>

      <div className="grid">
        <div className="card"><h3>Device</h3><p>{latest?.device_id || "-"}</p></div>
        <div className="card"><h3>Firmware</h3><p>{latest?.fw_version || "-"}</p></div>
        <div className="card"><h3>Transport</h3><p>{latest?.transport_status || "-"}</p></div>
        <div className="card"><h3>Network Heartbeat</h3><p className={heartbeatOnline ? "status-online" : "status-offline"}>{heartbeatOnline ? "ONLINE" : "OFFLINE"}</p></div>
        <div className="card"><h3>Last Telemetry</h3><p>{lastTelemetryAt}</p><p>{telemetryAgeText} ({telemetryFresh ? "FRESH" : "STALE"})</p></div>
      </div>

      <div className="subsystem-grid">
        <div className="card">
          <h3>Grid</h3>
          <p>Status: <span className={gridOnline ? "status-online" : "status-offline"}>{gridOnline ? "ONLINE" : "OFFLINE"}</span></p>
          <p>Voltage: {`${gridVoltage.toFixed(1)} V`}</p>
          <p>Current: {`${Number(values.grid_current ?? 0).toFixed(2)} A`}</p>
          <p>Power: {`${gridPower.toFixed(1)} W`}</p>
          <p>Frequency: {`${Number(values.grid_frequency ?? 0).toFixed(2)} Hz`}</p>
          <p>Power Factor: {`${Number(values.grid_power_factor ?? 0).toFixed(2)}`}</p>
          <p>Energy: {`${Number(values.grid_energy_kwh ?? 0).toFixed(3)} kWh`}</p>
        </div>
        <div className="card">
          <h3>Fuel</h3>
          <p>Status: <span className={fuelOnline ? "status-online" : "status-offline"}>{fuelOnline ? "ONLINE" : "OFFLINE"}</span></p>
          <p>Level: {`${fuelPercent.toFixed(1)} %`}</p>
          <p>Volume: {`${fuelLiters.toFixed(1)} L`}</p>
          <p>Raw: {`${Number(values.fuel_raw ?? 0)}`}</p>
        </div>
        <div className="card">
          <h3>Generator</h3>
          <p>Status: <span className={gensetOnline > 0 ? "status-online" : "status-offline"}>{gensetOnline > 0 ? "ONLINE" : "OFFLINE"}</span></p>
          <p>Online Count: {gensetOnline} / {Number(values.genset_count_configured ?? 0)}</p>
          <p>Mode: {String(values.genset_mode || "-").toUpperCase()}</p>
          <p>Alarm: <span className={gensetAlarm ? "status-offline" : "status-online"}>{gensetAlarm ? "YES" : "NO"}</span></p>
          <p>Voltage: {`${Number(values.genset_voltage ?? 0).toFixed(1)} V`}</p>
          <p>Battery Voltage: {`${Number(values.genset_battery_voltage ?? 0).toFixed(2)} V`}</p>
          <p>Current: {`${Number(values.genset_current ?? 0).toFixed(2)} A`}</p>
          <p>Run Hours: {`${Number(values.genset_run_hours ?? 0).toFixed(0)}`}</p>
        </div>
        <div className="card">
          <h3>Battery Banks</h3>
          <p>Status: <span className={batteryOnline > 0 ? "status-online" : "status-offline"}>{batteryOnline > 0 ? "ONLINE" : "OFFLINE"}</span></p>
          <p>Online Banks: {batteryOnline} / {batteryBanksConfigured}</p>
          <p>Low SOC Count: {batteryLowSocCount}</p>
          <p>Power Source: {powerSource.toUpperCase()}</p>
          <table>
            <thead>
              <tr>
                <th>Rectifier</th>
                <th>Online Banks</th>
                <th>Avg SOC</th>
              </tr>
            </thead>
            <tbody>
              {rsRows.map((r) => (
                <tr key={r.name}>
                  <td>{r.name}</td>
                  <td>{r.onlineCount}</td>
                  <td>{`${r.avgSoc.toFixed(1)} %`}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
