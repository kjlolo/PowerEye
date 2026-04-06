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

const FIELD_LABELS = {
  grid_voltage: "Voltage (V)",
  grid_current: "Current (A)",
  grid_power: "Power (W)",
  grid_frequency: "Frequency (Hz)",
  grid_power_factor: "Power Factor",
  grid_energy_kwh: "Energy (kWh)",
  fuel_percent: "Fuel (%)",
  fuel_liters: "Fuel (L)",
  genset_online_count: "Genset Online",
  battery_online_count: "Battery Online",
  battery_discharging_count: "Battery Discharging",
  battery_charging_count: "Battery Charging",
  power_supply_grid: "Grid Supplying",
  power_supply_genset: "Genset Supplying",
  power_supply_battery: "Battery Supplying",
  site_power_available: "Power Available",
};

const FIELD_COLORS = {
  grid_voltage: "#ffd447",
  grid_current: "#43d68c",
  grid_power: "#38bdf8",
  grid_frequency: "#a78bfa",
  grid_power_factor: "#f97316",
  grid_energy_kwh: "#22c55e",
  fuel_percent: "#f97316",
  fuel_liters: "#22c55e",
  genset_online_count: "#a78bfa",
  battery_online_count: "#60a5fa",
  battery_discharging_count: "#f97316",
  battery_charging_count: "#22c55e",
  power_supply_grid: "#43d68c",
  power_supply_genset: "#a78bfa",
  power_supply_battery: "#f97316",
  site_power_available: "#facc15",
};

const TREND_GROUPS = {
  grid: ["grid_voltage", "grid_current", "grid_power", "grid_frequency", "grid_power_factor", "grid_energy_kwh"],
  fuel: ["fuel_percent", "fuel_liters"],
  generator: ["genset_online_count"],
  batteries: ["battery_online_count"],
  availability: ["power_supply_grid", "power_supply_genset", "power_supply_battery", "site_power_available"],
};

function defaultConfigForm() {
  return {
    rs485: {
      baud_rate: 9600,
      pzem_enabled: true,
      pzem_slave_id: 1,
      generator_enabled: false,
      generator_count: 1,
      battery_enabled: false,
      battery_bank_count: 4,
    },
    fuel: {
      enabled: true,
      tank_length_cm: 100,
      tank_diameter_cm: 40,
      sensor_reach_height_cm: 38,
      sensor_unreached_height_cm: 2,
    },
    alarms: {
      ac_mains_active_high: false,
      genset_run_active_high: false,
      genset_fail_active_high: false,
    },
    power_availability: {
      source_priority: "grid,genset,battery",
      grid_voltage_threshold: 150,
      min_battery_online_count: 1,
    },
  };
}

function toDatetimeLocalInput(date) {
  const d = date || new Date();
  const pad = (n) => String(n).padStart(2, "0");
  const yyyy = d.getFullYear();
  const mm = pad(d.getMonth() + 1);
  const dd = pad(d.getDate());
  const hh = pad(d.getHours());
  const mi = pad(d.getMinutes());
  return `${yyyy}-${mm}-${dd}T${hh}:${mi}`;
}

function localToIso(v) {
  if (!v) return "";
  return new Date(v).toISOString();
}

function formatBrowserDateTime(value) {
  if (!value) return "-";
  const dt = new Date(value);
  if (Number.isNaN(dt.getTime())) return "-";
  return dt.toLocaleString();
}

function formatAxisDateTime(value) {
  const dt = new Date(value);
  if (Number.isNaN(dt.getTime())) return "";
  const pad = (n) => String(n).padStart(2, "0");
  return `${pad(dt.getMonth() + 1)}/${pad(dt.getDate())} ${pad(dt.getHours())}:${pad(dt.getMinutes())}`;
}

function TrendCard({ title, data, fields, onToggleField }) {
  return (
    <div className="card">
      <h3>{title}</h3>
      <div className="field-selector">
        {fields.map((f) => (
          <label key={f} className="field-check">
            <input type="checkbox" checked={f.enabled} onChange={() => onToggleField(f.key)} />
            <span>{FIELD_LABELS[f.key]}</span>
          </label>
        ))}
      </div>
      <div className="chart-wrap">
        <ResponsiveContainer width="100%" height="100%">
          <LineChart data={data}>
            <XAxis dataKey="ts" type="number" domain={["dataMin", "dataMax"]} tickFormatter={formatAxisDateTime} />
            <YAxis />
            <Tooltip labelFormatter={(v) => formatBrowserDateTime(v)} />
            <Legend />
            {fields
              .filter((f) => f.enabled)
              .map((f) => (
                <Line key={f.key} type="monotone" dataKey={f.key} stroke={FIELD_COLORS[f.key]} dot={false} name={FIELD_LABELS[f.key]} />
              ))}
          </LineChart>
        </ResponsiveContainer>
      </div>
    </div>
  );
}

export default function SiteDashboardPageV2() {
  const { user } = useAuth();
  const isAdmin = useMemo(() => (user?.roles || []).includes("admin"), [user]);
  const [siteId, setSiteId] = useState("");
  const [sites, setSites] = useState([]);
  const [latest, setLatest] = useState(null);
  const [live, setLive] = useState(null);
  const [series, setSeries] = useState([]);
  const [tab, setTab] = useState("overview");
  const [events, setEvents] = useState({ active_alarms: [], history: [] });
  const [configForm, setConfigForm] = useState(defaultConfigForm());
  const [configMeta, setConfigMeta] = useState({ updated_by: null, updated_at: null });
  const [configMsg, setConfigMsg] = useState("");
  const now = new Date();
  const [trendStart, setTrendStart] = useState(toDatetimeLocalInput(new Date(now.getTime() - 24 * 60 * 60 * 1000)));
  const [trendEnd, setTrendEnd] = useState(toDatetimeLocalInput(now));
  const [trendApplied, setTrendApplied] = useState({ start: "", end: "", seq: 0 });
  const [trendFields, setTrendFields] = useState({
    grid: Object.fromEntries(TREND_GROUPS.grid.map((k) => [k, true])),
    fuel: Object.fromEntries(TREND_GROUPS.fuel.map((k) => [k, true])),
    generator: Object.fromEntries(TREND_GROUPS.generator.map((k) => [k, true])),
    batteries: Object.fromEntries(TREND_GROUPS.batteries.map((k) => [k, true])),
    availability: Object.fromEntries(TREND_GROUPS.availability.map((k) => [k, true])),
  });

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

  const loadSeries = async (selected, rangeOverride = null) => {
    if (!selected) return;
    const params = {};
    const requestedStart = rangeOverride?.start ?? trendStart;
    const requestedEnd = rangeOverride?.end ?? trendEnd;
    const startIso = localToIso(requestedStart);
    const endIso = localToIso(requestedEnd);
    if (startIso) params.start = startIso;
    if (endIso) params.end = endIso;
    const tsRes = await api.get(`/fleet/sites/${selected}/timeseries`, { params });
    const rows = tsRes.data.items || [];
    const byTime = new Map();
    rows.forEach((r) => {
      const ts = new Date(r.time).getTime();
      if (!Number.isFinite(ts)) return;
      const key = String(ts);
      const row = byTime.get(key) || { ts };
      row[r.field] = Number(r.value);
      byTime.set(key, row);
    });
    const sorted = Array.from(byTime.values()).sort((a, b) => a.ts - b.ts);
    setSeries(sorted);
  };

  const applyTrendRange = async () => {
    const next = { start: trendStart, end: trendEnd, seq: Date.now() };
    setTrendApplied(next);
    await loadSeries(siteId, next);
  };

  const loadEvents = async (selected) => {
    if (!selected) return;
    const params = {};
    const startIso = localToIso(trendStart);
    const endIso = localToIso(trendEnd);
    if (startIso) params.start = startIso;
    if (endIso) params.end = endIso;
    const { data } = await api.get(`/fleet/sites/${selected}/events`, { params });
    setEvents({ active_alarms: data.active_alarms || [], history: data.history || [] });
  };

  const loadConfig = async (selected) => {
    if (!selected) return;
    const { data } = await api.get(`/fleet/sites/${selected}/subsystem-config`);
    const cfg = data.config || {};
    const merged = defaultConfigForm();
    if (cfg.rs485) merged.rs485 = { ...merged.rs485, ...cfg.rs485 };
    if (cfg.fuel) merged.fuel = { ...merged.fuel, ...cfg.fuel };
    if (cfg.alarms) merged.alarms = { ...merged.alarms, ...cfg.alarms };
    if (cfg.power_availability) merged.power_availability = { ...merged.power_availability, ...cfg.power_availability };
    setConfigForm(merged);
    setConfigMeta({ updated_by: data.updated_by || null, updated_at: data.updated_at || null });
  };

  const saveConfig = async () => {
    try {
      const payload = {
        config: {
          rs485: configForm.rs485,
          fuel: configForm.fuel,
          alarms: configForm.alarms,
          power_availability: configForm.power_availability,
        },
      };
      const { data } = await api.put(`/fleet/sites/${siteId}/subsystem-config`, payload);
      setConfigMeta({ updated_by: data.updated_by || null, updated_at: data.updated_at || null });
      setConfigMsg("Configuration saved.");
    } catch (err) {
      setConfigMsg(err?.response?.data?.detail || err?.message || "Save failed.");
    }
  };

  const setCfg = (section, key, value) => {
    setConfigForm((prev) => ({ ...prev, [section]: { ...prev[section], [key]: value } }));
  };

  const toggleTrendField = (group, key) => {
    setTrendFields((prev) => ({
      ...prev,
      [group]: { ...prev[group], [key]: !prev[group][key] },
    }));
  };

  const asFieldEntries = (group) => TREND_GROUPS[group].map((k) => ({ key: k, enabled: !!trendFields[group][k] }));

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
  }, [tab, siteId]);

  useEffect(() => {
    if (tab !== "trends") return;
    if (!trendApplied.start && !trendApplied.end) return;
    loadSeries(siteId, trendApplied);
  }, [trendApplied, tab, siteId]);

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
              <h3>Batteries</h3>
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
          <div className="section-title">Date Range</div>
          <div className="card">
            <div className="row">
              <label className="inline-label">From</label>
              <input type="datetime-local" value={trendStart} onChange={(e) => setTrendStart(e.target.value)} />
              <label className="inline-label">To</label>
              <input type="datetime-local" value={trendEnd} onChange={(e) => setTrendEnd(e.target.value)} />
              <button type="button" onClick={applyTrendRange}>Apply</button>
            </div>
          </div>

          <div className="chart-grid">
            <TrendCard title="Grid" data={series} fields={asFieldEntries("grid")} onToggleField={(k) => toggleTrendField("grid", k)} />
            <TrendCard title="Fuel" data={series} fields={asFieldEntries("fuel")} onToggleField={(k) => toggleTrendField("fuel", k)} />
            <TrendCard title="Generator" data={series} fields={asFieldEntries("generator")} onToggleField={(k) => toggleTrendField("generator", k)} />
            <TrendCard title="Batteries" data={series} fields={asFieldEntries("batteries")} onToggleField={(k) => toggleTrendField("batteries", k)} />
            <TrendCard title="Power Availability" data={series} fields={asFieldEntries("availability")} onToggleField={(k) => toggleTrendField("availability", k)} />
          </div>
        </>
      )}

      {tab === "configuration" && (
        <>
          <div className="section-title">Remote Subsystem Configuration</div>
          <div className="subsystem-grid">
            <div className="card">
              <h3>Grid</h3>
              <div className="form-row"><span>PZEM Enabled</span><input type="checkbox" checked={!!configForm.rs485.pzem_enabled} onChange={(e) => setCfg("rs485", "pzem_enabled", e.target.checked)} disabled={!isAdmin} /></div>
              <div className="form-row"><span>PZEM Slave ID</span><input type="number" value={configForm.rs485.pzem_slave_id} onChange={(e) => setCfg("rs485", "pzem_slave_id", Number(e.target.value))} disabled={!isAdmin} /></div>
              <div className="form-row"><span>RS485 Baud Rate</span><input type="number" value={configForm.rs485.baud_rate} onChange={(e) => setCfg("rs485", "baud_rate", Number(e.target.value))} disabled={!isAdmin} /></div>
            </div>

            <div className="card">
              <h3>Generator</h3>
              <div className="form-row"><span>Generator Enabled</span><input type="checkbox" checked={!!configForm.rs485.generator_enabled} onChange={(e) => setCfg("rs485", "generator_enabled", e.target.checked)} disabled={!isAdmin} /></div>
              <div className="form-row"><span>Generator Count</span><input type="number" min="1" max="4" value={configForm.rs485.generator_count} onChange={(e) => setCfg("rs485", "generator_count", Number(e.target.value))} disabled={!isAdmin} /></div>
              <div className="form-row"><span>Run Active High</span><input type="checkbox" checked={!!configForm.alarms.genset_run_active_high} onChange={(e) => setCfg("alarms", "genset_run_active_high", e.target.checked)} disabled={!isAdmin} /></div>
              <div className="form-row"><span>Fail Active High</span><input type="checkbox" checked={!!configForm.alarms.genset_fail_active_high} onChange={(e) => setCfg("alarms", "genset_fail_active_high", e.target.checked)} disabled={!isAdmin} /></div>
            </div>

            <div className="card">
              <h3>Fuel</h3>
              <div className="form-row"><span>Fuel Enabled</span><input type="checkbox" checked={!!configForm.fuel.enabled} onChange={(e) => setCfg("fuel", "enabled", e.target.checked)} disabled={!isAdmin} /></div>
              <div className="form-row"><span>Tank Length (cm)</span><input type="number" value={configForm.fuel.tank_length_cm} onChange={(e) => setCfg("fuel", "tank_length_cm", Number(e.target.value))} disabled={!isAdmin} /></div>
              <div className="form-row"><span>Tank Diameter (cm)</span><input type="number" value={configForm.fuel.tank_diameter_cm} onChange={(e) => setCfg("fuel", "tank_diameter_cm", Number(e.target.value))} disabled={!isAdmin} /></div>
              <div className="form-row"><span>Sensor Reach (cm)</span><input type="number" value={configForm.fuel.sensor_reach_height_cm} onChange={(e) => setCfg("fuel", "sensor_reach_height_cm", Number(e.target.value))} disabled={!isAdmin} /></div>
              <div className="form-row"><span>Sensor Unreached (cm)</span><input type="number" value={configForm.fuel.sensor_unreached_height_cm} onChange={(e) => setCfg("fuel", "sensor_unreached_height_cm", Number(e.target.value))} disabled={!isAdmin} /></div>
            </div>

            <div className="card">
              <h3>Batteries</h3>
              <div className="form-row"><span>Battery Enabled</span><input type="checkbox" checked={!!configForm.rs485.battery_enabled} onChange={(e) => setCfg("rs485", "battery_enabled", e.target.checked)} disabled={!isAdmin} /></div>
              <div className="form-row"><span>Battery Bank Count</span><input type="number" min="1" max="16" value={configForm.rs485.battery_bank_count} onChange={(e) => setCfg("rs485", "battery_bank_count", Number(e.target.value))} disabled={!isAdmin} /></div>
            </div>

            <div className="card">
              <h3>Power Availability</h3>
              <div className="form-row"><span>Source Priority</span><input type="text" value={configForm.power_availability.source_priority} onChange={(e) => setCfg("power_availability", "source_priority", e.target.value)} disabled={!isAdmin} /></div>
              <div className="form-row"><span>Grid Voltage Threshold</span><input type="number" value={configForm.power_availability.grid_voltage_threshold} onChange={(e) => setCfg("power_availability", "grid_voltage_threshold", Number(e.target.value))} disabled={!isAdmin} /></div>
              <div className="form-row"><span>Min Battery Online</span><input type="number" value={configForm.power_availability.min_battery_online_count} onChange={(e) => setCfg("power_availability", "min_battery_online_count", Number(e.target.value))} disabled={!isAdmin} /></div>
            </div>
          </div>
          <div className="card">
            <div className="row">
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
            <div className="row form-actions">
              <button type="button" className="secondary" onClick={() => loadEvents(siteId)}>Refresh Events</button>
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
                    <td colSpan={3}>No alarm events for selected date range.</td>
                  </tr>
                ) : (
                  (events.history || []).map((evt, idx) => (
                    <tr key={`${evt.alarm_key}-${evt.time}-${idx}`}>
                      <td>{formatBrowserDateTime(evt.time)}</td>
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
