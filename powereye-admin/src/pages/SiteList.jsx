import { useEffect, useMemo, useState } from "react";
import { Link } from "react-router-dom";
import { api } from "../api";

export default function SiteList() {
  const [sites, setSites] = useState([]);
  const [search, setSearch] = useState("");
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");

  async function loadSites() {
    setLoading(true);
    setError("");
    try {
      const res = await api.get("/sites");
      setSites(res.data.items || []);
    } catch (e) {
      setError(e?.response?.data?.error || e.message || "Failed to load sites");
    } finally {
      setLoading(false);
    }
  }

  async function deleteSite(siteId) {
    if (!window.confirm(`Delete site ${siteId}?`)) return;
    try {
      await api.delete(`/sites/${siteId}`);
      await loadSites();
    } catch (e) {
      setError(e?.response?.data?.error || e.message || "Failed to delete site");
    }
  }

  useEffect(() => {
    loadSites();
  }, []);

  const filtered = useMemo(() => {
    const q = search.trim().toLowerCase();
    if (!q) return sites;
    return sites.filter((s) =>
      [s.site_id, s.site_name, s.area_id, s.region, s.city]
        .filter(Boolean)
        .join(" ")
        .toLowerCase()
        .includes(q)
    );
  }, [sites, search]);

  return (
    <main className="container">
      <header className="header">
        <h1>Power Eye Site Registry</h1>
        <Link className="btn" to="/sites/new">
          Add Site
        </Link>
      </header>

      <div className="toolbar">
        <input
          type="text"
          placeholder="Search site, area, region..."
          value={search}
          onChange={(e) => setSearch(e.target.value)}
        />
        <button className="btn-secondary" onClick={loadSites} type="button">
          Refresh
        </button>
      </div>

      {error && <p className="error">{error}</p>}
      {loading ? (
        <p>Loading...</p>
      ) : (
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Site ID</th>
                <th>Site Name</th>
                <th>Area</th>
                <th>Region</th>
                <th>City</th>
                <th>Actions</th>
              </tr>
            </thead>
            <tbody>
              {filtered.map((s) => (
                <tr key={s.site_id}>
                  <td>{s.site_id}</td>
                  <td>{s.site_name}</td>
                  <td>{s.area_id}</td>
                  <td>{s.region}</td>
                  <td>{s.city}</td>
                  <td className="actions">
                    <Link to={`/sites/${s.site_id}/edit`}>Edit</Link>
                    <button type="button" onClick={() => deleteSite(s.site_id)}>
                      Delete
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </main>
  );
}
