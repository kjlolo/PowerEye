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

function TrendCard({ title, data, fields, onToggleField, domainStart, domainEnd }) {
  const xDomain = Number.isFinite(domainStart) && Number.isFinite(domainEnd)
    ? [domainStart, domainEnd]
    : ["dataMin", "dataMax"];
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
          <LineChart data={data} syncId="site-trends">
            <XAxis
              dataKey="ts"
              type="number"
              domain={xDomain}
              allowDataOverflow
              tickFormatter={formatAxisDateTime}
            />
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
  const [gensetControlMsg, setGensetControlMsg] = useState("");
  const [configForm, setConfigForm] = useState(defaultConfigForm());
  const [configMeta, setConfigMeta] = useState({ updated_by: null, updated_at: null });
  const [configMsg, setConfigMsg] = useState("");
  const now = new Date();
  const [trendStart, setTrendStart] = useState(toDatetimeLocalInput(new Date(now.getTime() - 24 * 60 * 60 * 1000)));
  const [trendEnd, setTrendEnd] = useState(toDatetimeLocalInput(now));
  const [trendApplied, setTrendApplied] = useState({ start: "", end: "", seq: 0 });
  const [trendDomain, setTrendDomain] = useState({ startMs: null, endMs: null });
  const [trendLoading, setTrendLoading] = useState(false);
  const [trendHasLoaded, setTrendHasLoaded] = useState(false);
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
    setTrendLoading(true);
    setSeries([]);
    const params = {};
    const requestedStart = rangeOverride?.start ?? trendStart;
    const requestedEnd = rangeOverride?.end ?? trendEnd;
    const startIso = localToIso(requestedStart);
    const endIso = localToIso(requestedEnd);
    if (startIso) params.start = startIso;
    if (endIso) params.end = endIso;
    try {
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
    } finally {
      setTrendLoading(false);
      setTrendHasLoaded(true);
    }
  };

  const applyTrendRange = async () => {
    const next = { start: trendStart, end: trendEnd, seq: Date.now() };
    const startMs = new Date(next.start).getTime();
    const endMs = new Date(next.end).getTime();
    setTrendDomain({
      startMs: Number.isFinite(startMs) ? startMs : null,
      endMs: Number.isFinite(endMs) ? endMs : null,
    });
    setTrendApplied(next);
    await loadSeries(siteId, next);
  };

  const applyPresetHours = async (hours) => {
    const end = new Date();
    const start = new Date(end.getTime() - hours * 60 * 60 * 1000);
    const startLocal = toDatetimeLocalInput(start);
    const endLocal = toDatetimeLocalInput(end);
    setTrendStart(startLocal);
    setTrendEnd(endLocal);
    const next = { start: startLocal, end: endLocal, seq: Date.now() };
    const startMs = new Date(next.start).getTime();
    const endMs = new Date(next.end).getTime();
    setTrendDomain({
      startMs: Number.isFinite(startMs) ? startMs : null,
      endMs: Number.isFinite(endMs) ? endMs : null,
    });
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

    // Reflect device-reported runtime configuration when telemetry is available.
    const lv = live?.values || {};
    if (typeof lv.cfg_pzem_enabled === "boolean") merged.rs485.pzem_enabled = !!lv.cfg_pzem_enabled;
    if (typeof lv.cfg_generator_enabled === "boolean") merged.rs485.generator_enabled = !!lv.cfg_generator_enabled;
    if (typeof lv.cfg_battery_enabled === "boolean") merged.rs485.battery_enabled = !!lv.cfg_battery_enabled;
    if (typeof lv.cfg_fuel_enabled === "boolean") merged.fuel.enabled = !!lv.cfg_fuel_enabled;
    if (Number.isFinite(Number(lv.genset_count_configured))) merged.rs485.generator_count = Number(lv.genset_count_configured);
    if (Number.isFinite(Number(lv.battery_bank_count_configured))) merged.rs485.battery_bank_count = Number(lv.battery_bank_count_configured);

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
    const startMs = new Date(trendStart).getTime();
    const endMs = new Date(trendEnd).getTime();
    setTrendDomain({
      startMs: Number.isFinite(startMs) ? startMs : null,
      endMs: Number.isFinite(endMs) ? endMs : null,
    });
  }, [tab]);

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
  const batteryBanksConfigured = Number(values.battery_bank_count_configured ?? 0);
  const heartbeatOnline = !!live?.network_heartbeat_online;
  const lastTelemetryAt = live?.last_telemetry_at ? new Date(live.last_telemetry_at).toLocaleString() : "-";
  const telemetryAgeSec = live?.last_telemetry_age_sec;
  const telemetryAgeText = Number.isFinite(telemetryAgeSec) ? `${telemetryAgeSec}s ago` : "-";
  const telemetryFresh = Number.isFinite(telemetryAgeSec) ? telemetryAgeSec <= 120 : false;
  const gridOnline = !!values.grid_online || gridVoltage > 150;
  const fuelOnline = !!values.fuel_online || !!values.fuel_sensor_online;
  const gensetAlarm = !!values.genset_alarm || !!values.genset_any_alarm;
  const gensetState = String(values.genset_state || (gensetOnline > 0 ? "standby" : "offline"));
  const gensetCommOnline = gensetOnline > 0;
  const powerSource = String(values.power_source || "-");
  const sitePowerAvailable = !!values.site_power_available;
  const batterySupplying = !!values.power_supply_battery;
  const gridSupplying = !!values.power_supply_grid;
  const gensetSupplying = !!values.power_supply_genset;
  const activeAlarmCount = (events.active_alarms || []).length;
  const criticalAlarmCount = (events.active_alarms || []).filter((a) => a.active && a.severity === "critical").length;

  const gridCardState = gridOnline ? "ok" : "fault";
  const fuelCardState = fuelOnline ? "ok" : "fault";
  const gensetCardState = gensetOnline > 0 ? (gensetAlarm ? "warn" : "ok") : "fault";
  const bankCount = Math.max(0, Math.min(16, batteryBanksConfigured || 0));
  const bankRows = Array.from({ length: bankCount }, (_, i) => {
    const idx = i + 1;
    const online = !!values[`bank_${idx}_online`];
    const alarm = !!values[`bank_${idx}_alarm`];
    const soc = Number(values[`bank_${idx}_soc`] ?? 0);
    const soh = Number(values[`bank_${idx}_soh`] ?? 0);
    const voltage = Number(values[`bank_${idx}_voltage`] ?? 0);
    const current = Number(values[`bank_${idx}_current`] ?? 0);
    const rectifier = Math.min(4, Math.floor(i / 4) + 1);
    const state = !online ? "fault" : alarm || soc <= 20 ? "warn" : "ok";
    return { idx, online, alarm, soc, soh, voltage, current, rectifier, state };
  });
  const bankGroups = [1, 2, 3, 4].map((rs) => ({
    rs,
    items: bankRows.filter((b) => b.rectifier === rs),
  })).filter((g) => g.items.length > 0);

  const requestGensetControl = (action) => {
    // UI is ready; command-plane API wiring follows in the next patch.
    setGensetControlMsg(`Command requested: ${action}. Command dispatch API not yet wired.`);
  };

  return (
    <div>
      <div className="topbar">
        <h2>Site Dashboard</h2>
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

      <div className="noc-strip">
        <div className="noc-tile">
          <span>Site</span>
          <b>{siteId || "-"}</b>
        </div>
        <div className="noc-tile">
          <span>Power Source</span>
          <b>{powerSource.toUpperCase()}</b>
        </div>
        <div className="noc-tile">
          <span>Power State</span>
          <b><StatusChip online={sitePowerAvailable} onlineText="AVAILABLE" offlineText="UNAVAILABLE" /></b>
        </div>
        <div className="noc-tile">
          <span>Supplying Source</span>
          <b>{gridSupplying ? "GRID" : gensetSupplying ? "GENSET" : batterySupplying ? "BATTERY" : "NONE"}</b>
        </div>
        <div className="noc-tile">
          <span>Active Alarms</span>
          <b>{activeAlarmCount}</b>
        </div>
        <div className="noc-tile">
          <span>Last Telemetry</span>
          <b>{telemetryAgeText}</b>
        </div>
      </div>

      <div className="tab-row">
        <button type="button" className={`secondary ${tab === "overview" ? "tab-active" : ""}`} onClick={() => setTab("overview")}>Overview</button>
        <button type="button" className={`secondary ${tab === "trends" ? "tab-active" : ""}`} onClick={() => setTab("trends")}>Trends</button>
        <button type="button" className={`secondary ${tab === "configuration" ? "tab-active" : ""}`} onClick={() => setTab("configuration")}>Configuration</button>
        <button type="button" className={`secondary ${tab === "events" ? "tab-active" : ""}`} onClick={() => setTab("events")}>Events</button>
      </div>

      {criticalAlarmCount > 0 ? (
        <div className="critical-banner">
          Critical incident active: {criticalAlarmCount} critical alarm{criticalAlarmCount > 1 ? "s" : ""} detected.
        </div>
      ) : null}

      {tab === "overview" && (
        <>
          <div className="section-title">Site Status</div>
          <div className="grid">
            <div className="card card-state-neutral">
              <h3>Device</h3>
              <div className="value-line">{latest?.device_id || "-"}</div>
              <div className="meta-line">Firmware: {latest?.fw_version || "-"}</div>
            </div>
            <div className="card card-state-neutral">
              <h3>Transport</h3>
              <div className="value-line">{String(latest?.transport_status || "-").toUpperCase()}</div>
              <div className="meta-line">Source: MQTT/HTTP runtime</div>
            </div>
            <div className={`card ${heartbeatOnline ? "card-state-ok" : "card-state-warn"}`}>
              <h3>Network Heartbeat</h3>
              <div className="value-line"><StatusChip online={heartbeatOnline} /></div>
              <div className="meta-line">Telemetry link state</div>
              <div className="meta-line">Phone: {latest?.phone_number || "-"}</div>
            </div>
            <div className={`card ${telemetryFresh ? "card-state-ok" : "card-state-warn"}`}>
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
            <div className={`card card-state-${gridCardState}`}>
              <h3>Grid</h3>
              <div className="meta-line">Updated: {lastTelemetryAt}</div>
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
            <div className={`card card-state-${fuelCardState}`}>
              <h3>Fuel</h3>
              <div className="meta-line">Updated: {lastTelemetryAt}</div>
              <div className="metric-list">
                <MetricRow label="Status" value={<StatusChip online={fuelOnline} />} />
                <MetricRow label="Level" value={`${fuelPercent.toFixed(1)} %`} />
                <MetricRow label="Volume" value={`${fuelLiters.toFixed(1)} L`} />
                <MetricRow label="Raw" value={`${Number(values.fuel_raw ?? 0)}`} />
              </div>
            </div>
            <div className={`card card-state-${gensetCardState} card-span-2`}>
              <h3>Generator</h3>
              <div className="meta-line">Updated: {lastTelemetryAt}</div>
              <div className="generator-layout">
                <div className="metric-list metric-list-two">
                  <MetricRow label="Status (Comms)" value={<StatusChip online={gensetCommOnline} onlineText="ONLINE" offlineText="OFFLINE" />} />
                  <MetricRow label="State (Operation)" value={gensetState.toUpperCase()} />
                  <MetricRow label="Online Count" value={`${gensetOnline} / ${Number(values.genset_count_configured ?? 0)}`} />
                  <MetricRow label="Mode" value={String(values.genset_mode || "-").toUpperCase()} />
                  <MetricRow label="Alarm" value={<WarnChip active={gensetAlarm} />} />
                  <MetricRow label="Run Hours" value={`${Number(values.genset_run_hours ?? 0).toFixed(0)}`} />
                  <MetricRow label="Voltage" value={`${Number(values.genset_voltage ?? 0).toFixed(1)} V`} />
                  <MetricRow label="Current" value={`${Number(values.genset_current ?? 0).toFixed(2)} A`} />
                  <MetricRow label="Battery Voltage" value={`${Number(values.genset_battery_voltage ?? 0).toFixed(2)} V`} />
                  <MetricRow label="Fuel Level" value={`${Number(values.genset_fuel_level_percent ?? 0).toFixed(1)} %`} />
                  <MetricRow label="Engine Temp" value={`${Number(values.genset_engine_temp_c ?? 0).toFixed(1)} C`} />
                  <MetricRow label="Oil Pressure" value={`${Number(values.genset_oil_pressure_kpa ?? 0).toFixed(1)} kPa`} />
                  <MetricRow label="Active Power" value={`${Number(values.genset_active_power_kw ?? 0).toFixed(2)} kW`} />
                  <MetricRow label="Apparent Power" value={`${Number(values.genset_apparent_power_kva ?? 0).toFixed(2)} kVA`} />
                </div>
                <div className="control-panel">
                  <div className="meta-line">Genset Control</div>
                  <div className="row">
                    <button type="button" className="secondary" onClick={() => requestGensetControl("start")}>Start</button>
                    <button type="button" className="secondary danger" onClick={() => requestGensetControl("stop")}>Stop</button>
                  </div>
                  <div className="row" style={{ marginTop: 6 }}>
                    <button type="button" className="secondary" onClick={() => requestGensetControl("mode:auto")}>Auto</button>
                    <button type="button" className="secondary" onClick={() => requestGensetControl("mode:manual")}>Manual</button>
                    <button type="button" className="secondary" onClick={() => requestGensetControl("mode:test")}>Test</button>
                  </div>
                  {gensetControlMsg ? <div className="meta-line">{gensetControlMsg}</div> : null}
                </div>
              </div>
            </div>
          </div>

          <div className="section-title">Battery Banks</div>
          {bankGroups.length === 0 ? (
            <div className="card"><div className="meta-line">No battery banks configured.</div></div>
          ) : (
            bankGroups.map((g) => (
              <div className="card" key={`rs-${g.rs}`}>
                <h3>{`RS${g.rs} Banks`}</h3>
                <div className="bank-grid">
                  {g.items.map((b) => (
                    <div className={`bank-card card-state-${b.state}`} key={`bank-${b.idx}`}>
                      <div className="bank-head">
                        <span>{`Bank ${b.idx}`}</span>
                        <StatusChip online={b.online} />
                      </div>
                      <div className="metric-list">
                        <MetricRow label="Voltage" value={`${b.voltage.toFixed(2)} V`} />
                        <MetricRow label="Current" value={`${b.current.toFixed(2)} A`} />
                        <MetricRow label="SOC" value={`${b.soc.toFixed(1)} %`} />
                        <MetricRow label="SOH" value={`${b.soh.toFixed(1)} %`} />
                        <MetricRow label="Alarm" value={<WarnChip active={b.alarm} />} />
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            ))
          )}
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
              <button type="button" className="secondary" onClick={() => applyPresetHours(1)}>1H</button>
              <button type="button" className="secondary" onClick={() => applyPresetHours(6)}>6H</button>
              <button type="button" className="secondary" onClick={() => applyPresetHours(24)}>24H</button>
              <button type="button" className="secondary" onClick={() => applyPresetHours(168)}>7D</button>
            </div>
          </div>

          <div className="chart-grid">
            {trendLoading ? (
              <div className="card"><div className="meta-line">Loading trend data...</div></div>
            ) : trendHasLoaded && series.length === 0 ? (
              <div className="card"><div className="meta-line">No data for the selected date range.</div></div>
            ) : (
              <>
                <TrendCard
                  title="Grid"
                  data={series}
                  fields={asFieldEntries("grid")}
                  onToggleField={(k) => toggleTrendField("grid", k)}
                  domainStart={trendDomain.startMs}
                  domainEnd={trendDomain.endMs}
                />
                <TrendCard
                  title="Fuel"
                  data={series}
                  fields={asFieldEntries("fuel")}
                  onToggleField={(k) => toggleTrendField("fuel", k)}
                  domainStart={trendDomain.startMs}
                  domainEnd={trendDomain.endMs}
                />
                <TrendCard
                  title="Generator"
                  data={series}
                  fields={asFieldEntries("generator")}
                  onToggleField={(k) => toggleTrendField("generator", k)}
                  domainStart={trendDomain.startMs}
                  domainEnd={trendDomain.endMs}
                />
                <TrendCard
                  title="Batteries"
                  data={series}
                  fields={asFieldEntries("batteries")}
                  onToggleField={(k) => toggleTrendField("batteries", k)}
                  domainStart={trendDomain.startMs}
                  domainEnd={trendDomain.endMs}
                />
                <TrendCard
                  title="Power Availability"
                  data={series}
                  fields={asFieldEntries("availability")}
                  onToggleField={(k) => toggleTrendField("availability", k)}
                  domainStart={trendDomain.startMs}
                  domainEnd={trendDomain.endMs}
                />
              </>
            )}
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
            <table>
              <thead>
                <tr>
                  <th>Alarm</th>
                  <th>Severity</th>
                  <th>Alarm Time</th>
                </tr>
              </thead>
              <tbody>
                {(events.active_alarms || []).length === 0 ? (
                  <tr>
                    <td colSpan={3}>No active alarms.</td>
                  </tr>
                ) : (
                  (events.active_alarms || []).map((a) => (
                    <tr key={`active-${a.alarm_key}-${a.alarm_time}`}>
                      <td>{a.alarm_label}</td>
                      <td>{String(a.severity || "major").toUpperCase()}</td>
                      <td>{formatBrowserDateTime(a.alarm_time)}</td>
                    </tr>
                  ))
                )}
              </tbody>
            </table>
            <div className="row form-actions">
              <button type="button" className="secondary" onClick={() => loadEvents(siteId)}>Refresh Events</button>
            </div>
          </div>

          <div className="section-title">Historical Alarms</div>
          <div className="card">
            <table>
              <thead>
                <tr>
                  <th>Alarm</th>
                  <th>Severity</th>
                  <th>Alarm Time</th>
                  <th>Clear Time</th>
                </tr>
              </thead>
              <tbody>
                {(events.history || []).length === 0 ? (
                  <tr>
                    <td colSpan={4}>No cleared alarms for selected date range.</td>
                  </tr>
                ) : (
                  (events.history || []).map((evt, idx) => (
                    <tr key={`${evt.alarm_key}-${evt.alarm_time}-${evt.clear_time}-${idx}`}>
                      <td>{evt.alarm_label}</td>
                      <td>{String(evt.severity || "major").toUpperCase()}</td>
                      <td>{formatBrowserDateTime(evt.alarm_time)}</td>
                      <td>{formatBrowserDateTime(evt.clear_time)}</td>
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
