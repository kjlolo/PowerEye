import { useEffect, useMemo, useState } from "react";
import api from "../api";
import { useAuth } from "../state/AuthContext";

export default function OtaPage() {
  const { user } = useAuth();
  const isAdmin = useMemo(() => (user?.roles || []).includes("admin"), [user]);
  const [releases, setReleases] = useState([]);
  const [jobs, setJobs] = useState([]);
  const [sites, setSites] = useState([]);
  const [reports, setReports] = useState([]);
  const [selectedJobId, setSelectedJobId] = useState(null);
  const [releaseForm, setReleaseForm] = useState({ version: "", filename: "", sha256: "", notes: "" });
  const [jobForm, setJobForm] = useState({ firmware_version: "", target_scope: "all", target_value: "" });
  const [firmwareFile, setFirmwareFile] = useState(null);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState("");

  const load = async () => {
    const [r1, r2, r3] = await Promise.all([api.get("/ota/firmware"), api.get("/ota/jobs"), api.get("/sites")]);
    setReleases(r1.data.items || []);
    setJobs(r2.data.items || []);
    setSites(r3.data.items || []);
  };

  useEffect(() => {
    load();
  }, []);

  const formatBytes = (v) => {
    const n = Number(v || 0);
    if (!n) return "0 B";
    if (n < 1024) return `${n} B`;
    if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
    return `${(n / (1024 * 1024)).toFixed(2)} MB`;
  };

  const createRelease = async (e) => {
    e.preventDefault();
    if (!firmwareFile) {
      setMessage("Select a firmware .bin file first.");
      return;
    }
    try {
      setBusy(true);
      setMessage("Preparing release...");
      const filename = releaseForm.filename || firmwareFile.name;
      const sha256 = (releaseForm.sha256 || "").trim();
      if (!sha256) {
        throw new Error("SHA256 is required. Paste the firmware SHA256 before upload.");
      }
      const payload = {
        version: releaseForm.version.trim(),
        filename,
        sha256,
        notes: releaseForm.notes || "",
      };
      const { data } = await api.post("/ota/firmware", payload);
      setMessage("Uploading firmware binary to S3...");
      const putResp = await fetch(data.upload_url, {
        method: "PUT",
        headers: { "Content-Type": "application/octet-stream" },
        body: firmwareFile,
      });
      if (!putResp.ok) {
        throw new Error(`Firmware upload failed (${putResp.status})`);
      }
      setReleaseForm({ version: "", filename: "", sha256: "", notes: "" });
      setFirmwareFile(null);
      setMessage("Release created and binary uploaded.");
      await load();
    } catch (err) {
      setMessage(err?.response?.data?.detail || err?.message || "Failed to create firmware release.");
    } finally {
      setBusy(false);
    }
  };

  const createJob = async (e) => {
    e.preventDefault();
    try {
      setBusy(true);
      setMessage("Creating OTA job...");
      await api.post("/ota/jobs", jobForm);
      setJobForm({ firmware_version: "", target_scope: "all", target_value: "" });
      setMessage("OTA job created.");
      await load();
    } catch (err) {
      setMessage(err?.response?.data?.detail || err?.message || "Failed to create OTA job.");
    } finally {
      setBusy(false);
    }
  };

  const areaOptions = useMemo(() => {
    const set = new Set();
    for (const s of sites) {
      const area = String(s?.area_id || "").trim();
      if (area) set.add(area);
    }
    return Array.from(set).sort((a, b) => a.localeCompare(b));
  }, [sites]);

  const siteOptions = useMemo(
    () => [...sites].sort((a, b) => String(a.site_id || "").localeCompare(String(b.site_id || ""))),
    [sites]
  );

  const cancelJob = async (id) => {
    try {
      setBusy(true);
      await api.post(`/ota/jobs/${id}/cancel`);
      setMessage(`OTA job ${id} cancelled.`);
      await load();
    } finally {
      setBusy(false);
    }
  };

  const loadReports = async (id) => {
    setSelectedJobId(id);
    const { data } = await api.get(`/ota/jobs/${id}/reports`);
    setReports(data.items || []);
  };

  if (!isAdmin) {
    return <div className="card"><h3>OTA Manager</h3><p>Viewer role cannot manage OTA jobs.</p></div>;
  }

  return (
    <div>
      <h2>OTA Manager</h2>
      {message && <div className="card"><div className="value-line">{message}</div></div>}

      <form className="card form-grid" onSubmit={createRelease}>
        <h3>Upload Firmware Release</h3>
        <input required placeholder="Version (e.g. 1.0.4)" value={releaseForm.version} onChange={(e) => setReleaseForm({ ...releaseForm, version: e.target.value })} />
        <input placeholder="Filename (.bin) (optional)" value={releaseForm.filename} onChange={(e) => setReleaseForm({ ...releaseForm, filename: e.target.value })} />
        <input placeholder="SHA256 (optional, auto if blank)" value={releaseForm.sha256} onChange={(e) => setReleaseForm({ ...releaseForm, sha256: e.target.value })} />
        <input placeholder="Release notes" value={releaseForm.notes} onChange={(e) => setReleaseForm({ ...releaseForm, notes: e.target.value })} />
        <input
          type="file"
          accept=".bin,application/octet-stream"
          onChange={(e) => {
            const f = e.target.files?.[0] || null;
            setFirmwareFile(f);
            if (f && !releaseForm.filename) {
              setReleaseForm((prev) => ({ ...prev, filename: f.name }));
            }
          }}
        />
        <button type="submit" disabled={busy}>{busy ? "Uploading..." : "Upload"}</button>
      </form>

      <form className="card form-grid" onSubmit={createJob}>
        <h3>Create OTA Job</h3>
        <select value={jobForm.firmware_version} onChange={(e) => setJobForm({ ...jobForm, firmware_version: e.target.value })}>
          <option value="">Select firmware version</option>
          {releases.map((r) => (
            <option key={r.version} value={r.version}>
              {r.version}
            </option>
          ))}
        </select>
        <select
          value={jobForm.target_scope}
          onChange={(e) => setJobForm({ ...jobForm, target_scope: e.target.value, target_value: "" })}
        >
          <option value="all">All Devices</option>
          <option value="region">By Region</option>
          <option value="area">By Area</option>
          <option value="site">By Site</option>
        </select>
        {jobForm.target_scope === "area" ? (
          <select value={jobForm.target_value} onChange={(e) => setJobForm({ ...jobForm, target_value: e.target.value })}>
            <option value="">Select area</option>
            {areaOptions.map((a) => (
              <option key={a} value={a}>
                {a}
              </option>
            ))}
          </select>
        ) : jobForm.target_scope === "site" ? (
          <select value={jobForm.target_value} onChange={(e) => setJobForm({ ...jobForm, target_value: e.target.value })}>
            <option value="">Select site</option>
            {siteOptions.map((s) => (
              <option key={s.site_id} value={s.site_id}>
                {s.site_id} - {s.site_name}
              </option>
            ))}
          </select>
        ) : (
          <input
            placeholder={jobForm.target_scope === "region" ? "Region value (e.g. MIN)" : "Target value"}
            value={jobForm.target_value}
            onChange={(e) => setJobForm({ ...jobForm, target_value: e.target.value })}
            disabled={jobForm.target_scope === "all"}
          />
        )}
        <button type="submit" disabled={busy}>{busy ? "Working..." : "Create OTA Job"}</button>
      </form>

      <div className="card">
        <h3>Firmware Releases</h3>
        <table>
          <thead>
            <tr>
              <th>Version</th>
              <th>Uploaded</th>
              <th>Size</th>
              <th>Created</th>
              <th>Notes</th>
            </tr>
          </thead>
          <tbody>
            {releases.map((r) => (
              <tr key={r.id || r.version}>
                <td>{r.version}</td>
                <td>{r.uploaded ? "Yes" : "No"}</td>
                <td>{formatBytes(r.size_bytes)}</td>
                <td>{r.created_at ? new Date(r.created_at).toLocaleString() : "-"}</td>
                <td>{r.notes || "-"}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <div className="card">
        <h3>OTA Jobs</h3>
        <table>
          <thead>
            <tr>
              <th>ID</th>
              <th>Version</th>
              <th>Scope</th>
              <th>Target</th>
              <th>Status</th>
              <th>Progress</th>
              <th>Reports</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            {jobs.map((x) => (
              <tr key={x.job.id}>
                <td>{x.job.id}</td>
                <td>{x.job.firmware_version}</td>
                <td>{x.job.target_scope}</td>
                <td>{x.job.target_value}</td>
                <td>{x.job.status}</td>
                <td>
                  {x.completed_targets}/{x.total_targets}
                </td>
                <td>
                  <button onClick={() => loadReports(x.job.id)}>View</button>
                </td>
                <td>
                  <button className="danger" onClick={() => cancelJob(x.job.id)}>
                    Cancel
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {selectedJobId && (
        <div className="card">
          <h3>Job Reports #{selectedJobId}</h3>
          <table>
            <thead>
              <tr>
                <th>Time</th>
                <th>Device</th>
                <th>Site</th>
                <th>Status</th>
                <th>Detail</th>
              </tr>
            </thead>
            <tbody>
              {reports.map((r) => (
                <tr key={r.id}>
                  <td>{r.reported_at ? new Date(r.reported_at).toLocaleString() : "-"}</td>
                  <td>{r.device_id}</td>
                  <td>{r.site_id}</td>
                  <td>{r.status}</td>
                  <td>{r.detail || "-"}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}
