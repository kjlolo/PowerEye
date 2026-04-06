import { useEffect, useState } from "react";
import api from "../api";

export default function RegionalDashboardPage() {
  const [regions, setRegions] = useState([]);
  const [selectedRegion, setSelectedRegion] = useState("");
  const [summary, setSummary] = useState(null);
  const [areas, setAreas] = useState([]);

  const load = async () => {
    const [sitesRes, regionalRes] = await Promise.all([
      api.get("/sites"),
      api.get("/fleet/regional-view", { params: { region: selectedRegion } }),
    ]);
    const uniqueRegions = Array.from(new Set((sitesRes.data.items || []).map((s) => s.region).filter(Boolean))).sort();
    setRegions(uniqueRegions);
    if (!selectedRegion && uniqueRegions.length) {
      setSelectedRegion(uniqueRegions[0]);
      return;
    }
    setSummary(regionalRes.data.summary || null);
    setAreas(regionalRes.data.areas || []);
  };

  useEffect(() => {
    load();
  }, [selectedRegion]);

  return (
    <div>
      <div className="topbar">
        <h2>Regional Dashboard</h2>
        <div className="row">
          <select value={selectedRegion} onChange={(e) => setSelectedRegion(e.target.value)}>
            {regions.map((r) => (
              <option key={r} value={r}>
                {r}
              </option>
            ))}
          </select>
          <button onClick={load}>Refresh</button>
        </div>
      </div>

      <div className="section-title">Region Summary</div>
      <div className="card">
        <div className="grid">
          <div>
            <div className="meta-line">Region</div>
            <div className="value-line">{summary?.region || selectedRegion || "-"}</div>
          </div>
          <div>
            <div className="meta-line">Total Sites</div>
            <div className="value-line">{summary?.total_sites ?? 0}</div>
          </div>
          <div>
            <div className="meta-line">Available Sites</div>
            <div className="value-line">{summary?.available_sites ?? 0}</div>
          </div>
          <div>
            <div className="meta-line">Availability</div>
            <div className="value-line">{Number(summary?.availability_pct ?? 0).toFixed(1)}%</div>
          </div>
          <div>
            <div className="meta-line">Stale Sites</div>
            <div className="value-line">{summary?.stale_sites ?? 0}</div>
          </div>
          <div>
            <div className="meta-line">Areas</div>
            <div className="value-line">{summary?.area_count ?? 0}</div>
          </div>
        </div>
      </div>

      <div className="section-title">Areas</div>
      <div className="area-grid">
        {areas.map((a) => (
          <div className="card" key={a.area_id}>
            <h3>{a.area_id}</h3>
            <div className="metric-list">
              <div className="metric-row"><span>Availability</span><b>{Number(a.availability_pct || 0).toFixed(1)}%</b></div>
              <div className="metric-row"><span>Sites</span><b>{a.available_sites}/{a.total_sites}</b></div>
              <div className="metric-row"><span>Stale Sites</span><b>{a.stale_sites}</b></div>
            </div>
            <div className="meta-line">Top Contributors Lowering Availability</div>
            <table>
              <thead>
                <tr>
                  <th>Site</th>
                  <th>Reason</th>
                  <th>Source</th>
                </tr>
              </thead>
              <tbody>
                {(a.contributors || []).length === 0 ? (
                  <tr>
                    <td colSpan={3}>No current contributors.</td>
                  </tr>
                ) : (
                  (a.contributors || []).map((c) => (
                    <tr key={`${a.area_id}-${c.site_id}`}>
                      <td>{c.site_id}</td>
                      <td>{String(c.reason || "").replaceAll("_", " ")}</td>
                      <td>{String(c.power_source || "none").toUpperCase()}</td>
                    </tr>
                  ))
                )}
              </tbody>
            </table>
          </div>
        ))}
      </div>
    </div>
  );
}
