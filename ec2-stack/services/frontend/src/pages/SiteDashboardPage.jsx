import { useEffect, useMemo, useState } from "react";
import { Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis, Legend } from "recharts";
import api from "../api";
import { useAuth } from "../state/AuthContext";

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
  const { user } = useAuth();
  const isAdmin = useMemo(() => (user?.roles || []).includes("admin"), [user]);
  const [siteId, setSiteId] = useState("");
  const [sites, setSites] = useState([]);
  const [latest, setLatest] = useState(null);
  const [live, setLive] = useState(null);
  const [series, setSeries] = useState([]);
  const [tab, setTab] = useState("overview");
  const [hours, setHours] = useState(24);
  const [events, setEvents] = useState({ active_alarms: [], history: [] });
  const [configText, setConfigText] = useState("{}");
  const [configMeta, setConfigMeta] = useState({ updated_by: null, updated_at: null });
  const [configMsg, setConfigMsg] = useState("");

  const loadSites = async () => {
    const { data } = await api.get("/sites");
    setSites(data.items || []);
    if (!siteId && data.items?.length) setSiteId(data.items[0].site_id);
  };

  const loadOverview = async (selected) => {
    if (!selected) return;
    const [latestRes, liveRes] = await Promise.all([
      api.get(`/fleet/sites/${selected}/latest`),
      api.get(`/fleet/sites/${selected}/live`),
    ]);
    setLatest(latestRes.data.item);
    setLive(liveRes.data);
  };

  const loadSeries = async (selected) => {
    if (!selected) return;
    const tsRes = await api.get(`/fleet/sites/${selected}/timeseries`, { params: { hours } });
    const rows = tsRes.data.items || [];
    const byTime = new Map();
    rows.forEach((r) => {
      const key = r.time;
      const row = byTime.get(key) || { time: key };
      row[r.field] = Number(r.value);
      byTime.set(key, row);
    });
    setSeries(Array.from(byTime.values()).sort((a, b) => (a.time > b.time ? 1 : -1)));
  };

  const loadEvents = async (selected) => {
    if (!selected) return;
    const { data } = await api.get(`/fleet/sites/${selected}/events`, { params: { hours } });
    setEvents({ active_alarms: data.active_alarms || [], history: data.history || [] });
  };

  const loadConfig = async (selected) => {
    if (!selected) return;
    const { data } = await api.get(`/fleet/sites/${selected}/subsystem-config`);
    setConfigText(JSON.stringify(data.config || {}, null, 2));
    setConfigMeta({ updated_by: data.updated_by || null, updated_at: data.updated_at || null });
  };

  const saveConfig = async () => {
    try {
      const parsed = JSON.parse(configText || "{}");
      const { data } = await api.put(`/fleet/sites/${siteId}/subsystem-config`, { config: parsed });
      setConfigText(JSON.stringify(data.config || {}, null, 2));
      setConfigMeta({ updated_by: data.updated_by || null, updated_at: data.updated_at || null });
      setConfigMsg("Configuration saved.");
    } catch (err) {
      setConfigMsg(err?.response?.data?.detail || err?.message || "Save failed.");
    }
  };

  useEffect(() => {
    loadSites();
  }, []);

  useEffect(() => {
    loadOverview(siteId);
  }, [siteId]);

  useEffect(() => {
    if (!siteId) return undefined;
    const timer = setInterval(() => {
      loadOverview(siteId);
    }, 15000);
    return () => clearInterval(timer);
  }, [siteId]);

  useEffect(() => {
    if (tab === "trends") loadSeries(siteId);
    if (tab === "events") loadEvents(siteId);
    if (tab === "configuration") loadConfig(siteId);
  }, [tab, siteId, hours]);

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
        <div className="row">
          <select value={siteId} onChange={(e) => setSiteId(e.target.value)}>
            {sites.map((s) => (
              <option key={s.site_id} value={s.site_id}>
                {s.site_id} - {s.site_name}
              </option>
            ))}
          </select>
          <select value={hours} onChange={(e) => setHours(Number(e.target.value))}>
            <option value={6}>Last 6h</option>
            <option value={24}>Last 24h</option>
            <option value={72}>Last 72h</option>
            <option value={168}>Last 7d</option>
          </select>
        </div>
      </div>
      <div className="tab-row">
        <button type="button" className={`secondary ${tab === "overview" ? "tab-active" : ""}`} onClick={() => setTab("overview")}>Overview</button>
        <button type="button" className={`secondary ${tab === "trends" ? "tab-active" : ""}`} onClick={() => setTab("trends")}>Trends</button>
        <button type="button" className={`secondary ${tab === "configuration" ? "tab-active" : ""}`} onClick={() => setTab("configuration")}>Configuration</button>
        <button type="button" className={`secondary ${tab === "events" ? "tab-active" : ""}`} onClick={() => setTab("events")}>Events</button>
      </div>

      {tab === "overview" && (
        <>
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
        </>
      )}

      {tab === "trends" && (
        <>
          <div className="section-title">Subsystem Trends</div>
          <div className="chart-grid">
            <div className="card">
              <h3>Grid Trends</h3>
              <div className="chart-wrap">
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={series}>
                    <XAxis dataKey="time" hide />
                    <YAxis />
                    <Tooltip />
                    <Legend />
                    <Line type="monotone" dataKey="grid_voltage" stroke="#ffd447" dot={false} name="Voltage (V)" />
                    <Line type="monotone" dataKey="grid_power" stroke="#38bdf8" dot={false} name="Power (W)" />
                    <Line type="monotone" dataKey="grid_current" stroke="#43d68c" dot={false} name="Current (A)" />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </div>
            <div className="card">
              <h3>Fuel Trends</h3>
              <div className="chart-wrap">
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={series}>
                    <XAxis dataKey="time" hide />
                    <YAxis />
                    <Tooltip />
                    <Legend />
                    <Line type="monotone" dataKey="fuel_percent" stroke="#f97316" dot={false} name="Fuel (%)" />
                    <Line type="monotone" dataKey="fuel_liters" stroke="#22c55e" dot={false} name="Fuel (L)" />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </div>
            <div className="card">
              <h3>Generator/Battery Online</h3>
              <div className="chart-wrap">
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={series}>
                    <XAxis dataKey="time" hide />
                    <YAxis />
                    <Tooltip />
                    <Legend />
                    <Line type="monotone" dataKey="genset_online_count" stroke="#a78bfa" dot={false} name="Generator Online" />
                    <Line type="monotone" dataKey="battery_online_count" stroke="#60a5fa" dot={false} name="Battery Online" />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </div>
            <div className="card">
              <h3>Network/Power Availability</h3>
              <div className="chart-wrap">
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={series}>
                    <XAxis dataKey="time" hide />
                    <YAxis />
                    <Tooltip />
                    <Legend />
                    <Line type="monotone" dataKey="network_online" stroke="#43d68c" dot={false} name="Network Online" />
                    <Line type="monotone" dataKey="site_power_available" stroke="#facc15" dot={false} name="Site Power Available" />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </div>
          </div>
        </>
      )}

      {tab === "configuration" && (
        <>
          <div className="section-title">Remote Subsystem Configuration</div>
          <div className="card">
            <div className="meta-line">Allowed keys: `rs485`, `fuel`, `alarms`</div>
            <div className="meta-line">Blocked keys: `identity`, `cloud`</div>
            <textarea
              className="json-editor"
              value={configText}
              onChange={(e) => setConfigText(e.target.value)}
              disabled={!isAdmin}
            />
            <div className="row form-actions">
              <button type="button" onClick={saveConfig} disabled={!isAdmin || !siteId}>Save Configuration</button>
            </div>
            <div className="meta-line">Updated by: {configMeta.updated_by || "-"}</div>
            <div className="meta-line">Updated at: {configMeta.updated_at ? new Date(configMeta.updated_at).toLocaleString() : "-"}</div>
            {configMsg ? <div className="meta-line">{configMsg}</div> : null}
          </div>
        </>
      )}

      {tab === "events" && (
        <>
          <div className="section-title">Active Alarms</div>
          <div className="card">
            <div className="alarm-chip-row">
              {(events.active_alarms || []).map((a) => (
                <span key={a.alarm_key} className={`status-chip ${a.active ? "warn" : "online"}`}>
                  {a.alarm_label}: {a.active ? "ACTIVE" : "CLEAR"}
                </span>
              ))}
            </div>
          </div>

          <div className="section-title">Historical Alarms</div>
          <div className="card">
            <table>
              <thead>
                <tr>
                  <th>Timestamp</th>
                  <th>Alarm</th>
                  <th>State</th>
                </tr>
              </thead>
              <tbody>
                {(events.history || []).length === 0 ? (
                  <tr>
                    <td colSpan={3}>No alarm events in selected time range.</td>
                  </tr>
                ) : (
                  (events.history || []).map((evt, idx) => (
                    <tr key={`${evt.alarm_key}-${evt.time}-${idx}`}>
                      <td>{new Date(evt.time).toLocaleString()}</td>
                      <td>{evt.alarm_label}</td>
                      <td>{evt.state.toUpperCase()}</td>
                    </tr>
                  ))
                )}
              </tbody>
            </table>
          </div>
        </>
      )}
    </div>
  );
}
