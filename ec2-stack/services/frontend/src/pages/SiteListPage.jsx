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

  const load = async () => {
    const { data } = await api.get("/sites", { params: { search } });
    setItems(data.items || []);
  };

  useEffect(() => {
    load();
  }, []);

  const onSave = async (e) => {
    e.preventDefault();
    const payload = {
      ...form,
      lat: form.lat === "" ? null : Number(form.lat),
      lng: form.lng === "" ? null : Number(form.lng),
      criticality_weight: Number(form.criticality_weight || 1),
    };
    if (editing) {
      await api.put(`/sites/${form.site_id}`, payload);
    } else {
      await api.post("/sites", payload);
    }
    setForm(emptySite);
    setEditing(false);
    await load();
  };

  const onEdit = (s) => {
    setForm({ ...s, lat: s.lat ?? "", lng: s.lng ?? "" });
    setEditing(true);
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
        </div>
      </div>

      {isAdmin && (
        <form className="card form-grid" onSubmit={onSave}>
          <h3>{editing ? "Edit Site" : "Create Site"}</h3>
          <input placeholder="Site ID" value={form.site_id} disabled={editing} onChange={(e) => setForm({ ...form, site_id: e.target.value })} />
          <input placeholder="Site Name" value={form.site_name} onChange={(e) => setForm({ ...form, site_name: e.target.value })} />
          <input placeholder="Area ID" value={form.area_id} onChange={(e) => setForm({ ...form, area_id: e.target.value })} />
          <input placeholder="Region" value={form.region} onChange={(e) => setForm({ ...form, region: e.target.value })} />
          <input placeholder="City" value={form.city} onChange={(e) => setForm({ ...form, city: e.target.value })} />
          <input placeholder="Province" value={form.province} onChange={(e) => setForm({ ...form, province: e.target.value })} />
          <input placeholder="Latitude" value={form.lat} onChange={(e) => setForm({ ...form, lat: e.target.value })} />
          <input placeholder="Longitude" value={form.lng} onChange={(e) => setForm({ ...form, lng: e.target.value })} />
          <input placeholder="Criticality Weight" value={form.criticality_weight} onChange={(e) => setForm({ ...form, criticality_weight: e.target.value })} />
          <button type="submit">{editing ? "Update" : "Create"}</button>
        </form>
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
          </tbody>
        </table>
      </div>
    </div>
  );
}
