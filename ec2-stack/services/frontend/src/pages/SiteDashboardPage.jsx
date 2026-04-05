import { useEffect, useState } from "react";
import { Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from "recharts";
import api from "../api";

export default function SiteDashboardPage() {
  const [siteId, setSiteId] = useState("");
  const [sites, setSites] = useState([]);
  const [latest, setLatest] = useState(null);
  const [series, setSeries] = useState([]);

  const loadSites = async () => {
    const { data } = await api.get("/sites");
    setSites(data.items || []);
    if (!siteId && data.items?.length) setSiteId(data.items[0].site_id);
  };

  const loadData = async (selected) => {
    if (!selected) return;
    const latestRes = await api.get(`/fleet/sites/${selected}/latest`);
    setLatest(latestRes.data.item);
    const tsRes = await api.get(`/fleet/sites/${selected}/timeseries`, { params: { hours: 24 } });
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

  useEffect(() => {
    loadSites();
  }, []);

  useEffect(() => {
    loadData(siteId);
  }, [siteId]);

  const getLatest = (field, fallback = "-") => {
    for (let i = series.length - 1; i >= 0; i -= 1) {
      const value = series[i][field];
      if (value !== undefined && value !== null && !Number.isNaN(value)) return value;
    }
    return fallback;
  };

  const gridVoltage = getLatest("grid_voltage");
  const gridPower = getLatest("grid_power");
  const fuelPercent = getLatest("fuel_percent");
  const fuelLiters = getLatest("fuel_liters");
  const gensetOnline = getLatest("genset_online_count");
  const batteryOnline = getLatest("battery_online_count");

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
        <div className="card"><h3>Last Seen</h3><p>{latest?.last_seen_at || "-"}</p></div>
      </div>

      <div className="subsystem-grid">
        <div className="card">
          <h3>Grid</h3>
          <p>Status: {typeof gridVoltage === "number" && gridVoltage > 0 ? "ONLINE" : "OFFLINE"}</p>
          <p>Voltage: {typeof gridVoltage === "number" ? `${gridVoltage.toFixed(1)} V` : "-"}</p>
          <p>Power: {typeof gridPower === "number" ? `${gridPower.toFixed(1)} W` : "-"}</p>
        </div>
        <div className="card">
          <h3>Fuel</h3>
          <p>Level: {typeof fuelPercent === "number" ? `${fuelPercent.toFixed(1)} %` : "-"}</p>
          <p>Volume: {typeof fuelLiters === "number" ? `${fuelLiters.toFixed(1)} L` : "-"}</p>
        </div>
        <div className="card">
          <h3>Generator</h3>
          <p>Online Count: {gensetOnline}</p>
          <p>Status: {typeof gensetOnline === "number" && gensetOnline > 0 ? "ONLINE" : "OFFLINE"}</p>
        </div>
        <div className="card">
          <h3>Battery Banks</h3>
          <p>Online Count: {batteryOnline}</p>
          <p>Status: {typeof batteryOnline === "number" && batteryOnline > 0 ? "ONLINE" : "OFFLINE"}</p>
        </div>
      </div>

      <div className="card">
        <h3>Grid Voltage / Fuel / Online Counts (24h)</h3>
        <div style={{ height: 320 }}>
          <ResponsiveContainer width="100%" height="100%">
            <LineChart data={series}>
              <XAxis dataKey="time" hide />
              <YAxis />
              <Tooltip />
              <Line type="monotone" dataKey="grid_voltage" stroke="#facc15" dot={false} />
              <Line type="monotone" dataKey="fuel_percent" stroke="#38bdf8" dot={false} />
              <Line type="monotone" dataKey="genset_online_count" stroke="#22c55e" dot={false} />
              <Line type="monotone" dataKey="battery_online_count" stroke="#f97316" dot={false} />
            </LineChart>
          </ResponsiveContainer>
        </div>
      </div>
    </div>
  );
}
