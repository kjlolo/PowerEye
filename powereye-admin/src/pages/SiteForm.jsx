import { useEffect, useState } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import { api } from "../api";

const EMPTY = {
  site_id: "",
  site_name: "",
  area_id: "",
  city: "",
  province: "",
  region: "",
  lat: "",
  lng: "",
  is_active: true,
};

export default function SiteForm() {
  const { site_id } = useParams();
  const editMode = Boolean(site_id);
  const [form, setForm] = useState(EMPTY);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState("");
  const navigate = useNavigate();

  useEffect(() => {
    if (!editMode) return;
    (async () => {
      try {
        const res = await api.get(`/sites/${site_id}`);
        setForm({ ...EMPTY, ...(res.data.item || {}) });
      } catch (e) {
        setError(e?.response?.data?.error || e.message || "Failed to load site");
      }
    })();
  }, [editMode, site_id]);

  function updateField(key, value) {
    setForm((prev) => ({ ...prev, [key]: value }));
  }

  async function submit(e) {
    e.preventDefault();
    setSaving(true);
    setError("");
    try {
      const payload = {
        ...form,
        lat: form.lat === "" ? null : Number(form.lat),
        lng: form.lng === "" ? null : Number(form.lng),
        updated_at: new Date().toISOString(),
      };

      if (editMode) {
        await api.put(`/sites/${site_id}`, payload);
      } else {
        await api.post("/sites", payload);
      }
      navigate("/sites");
    } catch (err) {
      setError(err?.response?.data?.error || err.message || "Failed to save site");
    } finally {
      setSaving(false);
    }
  }

  return (
    <main className="container">
      <header className="header">
        <h1>{editMode ? "Edit Site" : "Add Site"}</h1>
        <Link className="btn-secondary" to="/sites">
          Back
        </Link>
      </header>

      {error && <p className="error">{error}</p>}

      <form className="form" onSubmit={submit}>
        <label>
          Site ID
          <input
            required
            disabled={editMode}
            value={form.site_id}
            onChange={(e) => updateField("site_id", e.target.value)}
          />
        </label>

        <label>
          Site Name
          <input
            required
            value={form.site_name}
            onChange={(e) => updateField("site_name", e.target.value)}
          />
        </label>

        <label>
          Area ID
          <input value={form.area_id} onChange={(e) => updateField("area_id", e.target.value)} />
        </label>

        <label>
          Region
          <input value={form.region} onChange={(e) => updateField("region", e.target.value)} />
        </label>

        <label>
          Province
          <input value={form.province} onChange={(e) => updateField("province", e.target.value)} />
        </label>

        <label>
          City
          <input value={form.city} onChange={(e) => updateField("city", e.target.value)} />
        </label>

        <label>
          Latitude
          <input value={form.lat ?? ""} onChange={(e) => updateField("lat", e.target.value)} />
        </label>

        <label>
          Longitude
          <input value={form.lng ?? ""} onChange={(e) => updateField("lng", e.target.value)} />
        </label>

        <label className="checkbox">
          <input
            type="checkbox"
            checked={Boolean(form.is_active)}
            onChange={(e) => updateField("is_active", e.target.checked)}
          />
          Active
        </label>

        <button className="btn" type="submit" disabled={saving}>
          {saving ? "Saving..." : "Save Site"}
        </button>
      </form>
    </main>
  );
}
