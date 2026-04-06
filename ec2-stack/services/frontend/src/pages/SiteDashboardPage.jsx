import { useEffect, useState } from "react";
import api from "../api";

function StatusChip({ online, onlineText = "ONLINE", offlineText = "OFFLINE" }) {
  return <span className={`status-chip ${online ? "online" : "offline"}`}>{online ? onlineText : offlineText}</span>;
}

function WarnChip({ active, activeText = "YES", clearText = "NO" }) {
  if (active) return <span className="status-chip warn">{activeText}</span>;
  return <span className="status-chip online">{clearText}</span>;
}

function MetricRow({ label, value }) {
  return (
    <div className="metric-row">
      <span>{label}</span>
      <b>{value}</b>
    </div>
  );
}

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

      <div className="section-title">Site Status</div>
      <div className="grid">
        <div className="card">
          <h3>Device</h3>
          <div className="value-line">{latest?.device_id || "-"}</div>
          <div className="meta-line">Firmware: {latest?.fw_version || "-"}</div>
        </div>
        <div className="card">
          <h3>Transport</h3>
          <div className="value-line">{String(latest?.transport_status || "-").toUpperCase()}</div>
          <div className="meta-line">Source: MQTT/HTTP runtime</div>
        </div>
        <div className="card">
          <h3>Network Heartbeat</h3>
          <div className="value-line"><StatusChip online={heartbeatOnline} /></div>
          <div className="meta-line">Telemetry link state</div>
        </div>
        <div className="card">
          <h3>Last Telemetry</h3>
          <div className="meta-line">{lastTelemetryAt}</div>
          <div className="value-line">{telemetryAgeText}</div>
          <div className="meta-line">
            <span className={`status-chip ${telemetryFresh ? "online" : "warn"}`}>{telemetryFresh ? "FRESH" : "STALE"}</span>
          </div>
        </div>
      </div>

      <div className="section-title">Subsystems</div>
      <div className="subsystem-grid">
        <div className="card">
          <h3>Grid</h3>
          <div className="metric-list">
            <MetricRow label="Status" value={<StatusChip online={gridOnline} />} />
            <MetricRow label="Voltage" value={`${gridVoltage.toFixed(1)} V`} />
            <MetricRow label="Current" value={`${Number(values.grid_current ?? 0).toFixed(2)} A`} />
            <MetricRow label="Power" value={`${gridPower.toFixed(1)} W`} />
            <MetricRow label="Frequency" value={`${Number(values.grid_frequency ?? 0).toFixed(2)} Hz`} />
            <MetricRow label="Power Factor" value={`${Number(values.grid_power_factor ?? 0).toFixed(2)}`} />
            <MetricRow label="Energy" value={`${Number(values.grid_energy_kwh ?? 0).toFixed(3)} kWh`} />
          </div>
        </div>
        <div className="card">
          <h3>Fuel</h3>
          <div className="metric-list">
            <MetricRow label="Status" value={<StatusChip online={fuelOnline} />} />
            <MetricRow label="Level" value={`${fuelPercent.toFixed(1)} %`} />
            <MetricRow label="Volume" value={`${fuelLiters.toFixed(1)} L`} />
            <MetricRow label="Raw" value={`${Number(values.fuel_raw ?? 0)}`} />
          </div>
        </div>
        <div className="card">
          <h3>Generator</h3>
          <div className="metric-list">
            <MetricRow label="Status" value={<StatusChip online={gensetOnline > 0} />} />
            <MetricRow label="Online Count" value={`${gensetOnline} / ${Number(values.genset_count_configured ?? 0)}`} />
            <MetricRow label="Mode" value={String(values.genset_mode || "-").toUpperCase()} />
            <MetricRow label="Alarm" value={<WarnChip active={gensetAlarm} />} />
            <MetricRow label="Voltage" value={`${Number(values.genset_voltage ?? 0).toFixed(1)} V`} />
            <MetricRow label="Battery Voltage" value={`${Number(values.genset_battery_voltage ?? 0).toFixed(2)} V`} />
            <MetricRow label="Current" value={`${Number(values.genset_current ?? 0).toFixed(2)} A`} />
            <MetricRow label="Run Hours" value={`${Number(values.genset_run_hours ?? 0).toFixed(0)}`} />
          </div>
        </div>
        <div className="card">
          <h3>Battery Banks</h3>
          <div className="metric-list">
            <MetricRow label="Status" value={<StatusChip online={batteryOnline > 0} />} />
            <MetricRow label="Online Banks" value={`${batteryOnline} / ${batteryBanksConfigured}`} />
            <MetricRow label="Low SOC Count" value={`${batteryLowSocCount}`} />
            <MetricRow label="Power Source" value={powerSource.toUpperCase()} />
          </div>
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
