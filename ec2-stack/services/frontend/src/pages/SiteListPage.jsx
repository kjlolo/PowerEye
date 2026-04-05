import { useEffect, useMemo, useState } from "react";
import api from "../api";
import { useAuth } from "../state/AuthContext";

const emptySite = {
  site_id: "",
  site_name: "",
  area_id: "",
  region: "",
  city: "",
  province: "",
  lat: "",
  lng: "",
  criticality_weight: 1,
  is_active: true,
};

export default function SiteListPage() {
  const { user } = useAuth();
  const isAdmin = useMemo(() => (user?.roles || []).includes("admin"), [user]);
  const [items, setItems] = useState([]);
  const [search, setSearch] = useState("");
  const [form, setForm] = useState(emptySite);
  const [editing, setEditing] = useState(false);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState("");
  const [notice, setNotice] = useState("");
  const [showModal, setShowModal] = useState(false);
  const mqttCredentialsUrl = `${window.location.protocol}//${window.location.hostname}:18083/#/authentication`;

  const load = async (searchTerm = search) => {
    const { data } = await api.get("/sites", { params: { search: searchTerm } });
    setItems(data.items || []);
  };

  useEffect(() => {
    load();
  }, []);

  const onSave = async (e) => {
    e.preventDefault();
    setError("");
    setNotice("");
    if (!form.site_id?.trim()) {
      setError("Site ID is required.");
      return;
    }
    if (!form.site_name?.trim()) {
      setError("Site Name is required.");
      return;
    }
    if (!form.area_id?.trim()) {
      setError("Area ID is required.");
      return;
    }
    if (!form.region?.trim()) {
      setError("Region is required.");
      return;
    }
    const payload = {
      ...form,
      site_id: form.site_id.trim(),
      site_name: form.site_name.trim(),
      area_id: form.area_id.trim(),
      region: form.region.trim(),
      city: (form.city || "").trim(),
      province: (form.province || "").trim(),
      lat: form.lat === "" ? null : Number(form.lat),
      lng: form.lng === "" ? null : Number(form.lng),
      criticality_weight: Number(form.criticality_weight || 1),
    };
    if ((form.lat !== "" && Number.isNaN(payload.lat)) || (form.lng !== "" && Number.isNaN(payload.lng))) {
      setError("Latitude/Longitude must be valid numbers.");
      return;
    }
    setSaving(true);
    try {
      if (editing) {
        await api.put(`/sites/${form.site_id}`, payload);
        setNotice(`Updated site ${form.site_id}.`);
      } else {
        await api.post("/sites", payload);
        setNotice(`Created site ${form.site_id}.`);
      }
      setForm(emptySite);
      setEditing(false);
      setShowModal(false);
      await load();
    } catch (err) {
      const detail = err?.response?.data?.detail;
      if (detail === "site_exists") {
        setSearch(payload.site_id);
        await load(payload.site_id);
        setError(`Site ID ${payload.site_id} already exists. Showing existing record.`);
      } else {
        setError(detail || err?.message || "Failed to save site.");
      }
    } finally {
      setSaving(false);
    }
  };

  const onEdit = (s) => {
    setError("");
    setNotice("");
    setForm({ ...s, lat: s.lat ?? "", lng: s.lng ?? "" });
    setEditing(true);
    setShowModal(true);
  };

  const onDelete = async (siteId) => {
    if (!confirm(`Delete site ${siteId}?`)) return;
    await api.delete(`/sites/${siteId}`);
    await load();
  };

  return (
    <div>
      <div className="topbar">
        <h2>Site List</h2>
        <div className="row">
          <input placeholder="Search site..." value={search} onChange={(e) => setSearch(e.target.value)} />
          <button onClick={load}>Refresh</button>
          {isAdmin && (
            <button
              type="button"
              onClick={() => {
                setEditing(false);
                setForm(emptySite);
                setError("");
                setNotice("");
                setShowModal(true);
              }}
            >
              Create Site
            </button>
          )}
          {isAdmin && (
            <a href={mqttCredentialsUrl} target="_blank" rel="noreferrer">
              <button type="button" className="secondary">Device Credentials</button>
            </a>
          )}
        </div>
      </div>

      {error && !showModal && <div className="card text-danger">{error}</div>}
      {notice && !showModal && <div className="card">{notice}</div>}

      {isAdmin && showModal && (
        <div className="modal-backdrop" onClick={() => !saving && setShowModal(false)}>
          <div className="card modal-card" onClick={(e) => e.stopPropagation()}>
            <form className="form-grid" onSubmit={onSave}>
              <h3>{editing ? "Edit Site" : "Create Site"}</h3>
              <input placeholder="Site ID" value={form.site_id} disabled={editing || saving} onChange={(e) => setForm({ ...form, site_id: e.target.value })} />
              <input placeholder="Site Name" value={form.site_name} disabled={saving} onChange={(e) => setForm({ ...form, site_name: e.target.value })} />
              <input placeholder="Area ID" value={form.area_id} disabled={saving} onChange={(e) => setForm({ ...form, area_id: e.target.value })} />
              <input placeholder="Region" value={form.region} disabled={saving} onChange={(e) => setForm({ ...form, region: e.target.value })} />
              <input placeholder="City" value={form.city} onChange={(e) => setForm({ ...form, city: e.target.value })} />
              <input placeholder="Province" value={form.province} onChange={(e) => setForm({ ...form, province: e.target.value })} />
              <input placeholder="Latitude" value={form.lat} onChange={(e) => setForm({ ...form, lat: e.target.value })} />
              <input placeholder="Longitude" value={form.lng} onChange={(e) => setForm({ ...form, lng: e.target.value })} />
              <input placeholder="Criticality Weight" value={form.criticality_weight} onChange={(e) => setForm({ ...form, criticality_weight: e.target.value })} />
              {error && <div className="text-danger">{error}</div>}
              {notice && <div>{notice}</div>}
              <div className="row">
                <button type="submit" disabled={saving}>{saving ? "Saving..." : editing ? "Update" : "Create"}</button>
                <button
                  type="button"
                  className="secondary"
                  onClick={() => {
                    setShowModal(false);
                    setEditing(false);
                    setForm(emptySite);
                    setError("");
                    setNotice("");
                  }}
                  disabled={saving}
                >
                  Close
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      <div className="card">
        <table>
          <thead>
            <tr>
              <th>Site ID</th>
              <th>Name</th>
              <th>Area</th>
              <th>Region</th>
              <th>City</th>
              <th>Status</th>
              {isAdmin && <th>Actions</th>}
            </tr>
          </thead>
          <tbody>
            {items.map((s) => (
              <tr key={s.site_id}>
                <td>{s.site_id}</td>
                <td>{s.site_name}</td>
                <td>{s.area_id}</td>
                <td>{s.region}</td>
                <td>{s.city}</td>
                <td>{s.is_active ? "ACTIVE" : "INACTIVE"}</td>
                {isAdmin && (
                  <td>
                    <button onClick={() => onEdit(s)}>Edit</button>
                    <button className="danger" onClick={() => onDelete(s.site_id)}>
                      Delete
                    </button>
                  </td>
                )}
              </tr>
            ))}
            {items.length === 0 && (
              <tr>
                <td colSpan={isAdmin ? 7 : 6}>No sites found.</td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
