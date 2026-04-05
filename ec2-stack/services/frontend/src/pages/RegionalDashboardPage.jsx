import { useEffect, useMemo, useState } from "react";
import api from "../api";

export default function RegionalDashboardPage() {
  const [rows, setRows] = useState([]);

  const load = async () => {
    const { data } = await api.get("/fleet/overview");
    setRows(data.items || []);
  };

  useEffect(() => {
    load();
  }, []);

  const byRegion = useMemo(() => {
    const map = new Map();
    rows.forEach((r) => {
      const x = map.get(r.region) || { region: r.region, total: 0, stale: 0 };
      x.total += 1;
      if (!r.last_seen_at) x.stale += 1;
      map.set(r.region, x);
    });
    return Array.from(map.values()).sort((a, b) => a.region.localeCompare(b.region));
  }, [rows]);

  return (
    <div>
      <div className="topbar">
        <h2>Regional Dashboard</h2>
        <button onClick={load}>Refresh</button>
      </div>
      <div className="card">
        <h3>Region Availability Overview</h3>
        <table>
          <thead>
            <tr>
              <th>Region</th>
              <th>Total Sites</th>
              <th>Sites Missing Heartbeat</th>
              <th>Availability %</th>
            </tr>
          </thead>
          <tbody>
            {byRegion.map((r) => {
              const avail = r.total > 0 ? ((r.total - r.stale) / r.total) * 100 : 0;
              return (
                <tr key={r.region}>
                  <td>{r.region}</td>
                  <td>{r.total}</td>
                  <td>{r.stale}</td>
                  <td>{avail.toFixed(1)}%</td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
}
