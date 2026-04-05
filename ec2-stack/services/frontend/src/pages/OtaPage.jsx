import { useEffect, useMemo, useState } from "react";
import api from "../api";
import { useAuth } from "../state/AuthContext";

export default function OtaPage() {
  const { user } = useAuth();
  const isAdmin = useMemo(() => (user?.roles || []).includes("admin"), [user]);
  const [releases, setReleases] = useState([]);
  const [jobs, setJobs] = useState([]);
  const [releaseForm, setReleaseForm] = useState({ version: "", filename: "", sha256: "", notes: "" });
  const [jobForm, setJobForm] = useState({ firmware_version: "", target_scope: "all", target_value: "" });

  const load = async () => {
    const [r1, r2] = await Promise.all([api.get("/ota/firmware"), api.get("/ota/jobs")]);
    setReleases(r1.data.items || []);
    setJobs(r2.data.items || []);
  };

  useEffect(() => {
    load();
  }, []);

  const createRelease = async (e) => {
    e.preventDefault();
    const { data } = await api.post("/ota/firmware", releaseForm);
    alert(`Upload firmware binary to this signed URL:\n${data.upload_url}`);
    setReleaseForm({ version: "", filename: "", sha256: "", notes: "" });
    await load();
  };

  const createJob = async (e) => {
    e.preventDefault();
    await api.post("/ota/jobs", jobForm);
    setJobForm({ firmware_version: "", target_scope: "all", target_value: "" });
    await load();
  };

  const cancelJob = async (id) => {
    await api.post(`/ota/jobs/${id}/cancel`);
    await load();
  };

  if (!isAdmin) {
    return <div className="card"><h3>OTA Manager</h3><p>Viewer role cannot manage OTA jobs.</p></div>;
  }

  return (
    <div>
      <h2>OTA Manager</h2>

      <form className="card form-grid" onSubmit={createRelease}>
        <h3>Upload Firmware Release</h3>
        <input placeholder="Version (e.g. 1.0.4)" value={releaseForm.version} onChange={(e) => setReleaseForm({ ...releaseForm, version: e.target.value })} />
        <input placeholder="Filename (.bin)" value={releaseForm.filename} onChange={(e) => setReleaseForm({ ...releaseForm, filename: e.target.value })} />
        <input placeholder="SHA256" value={releaseForm.sha256} onChange={(e) => setReleaseForm({ ...releaseForm, sha256: e.target.value })} />
        <input placeholder="Release notes" value={releaseForm.notes} onChange={(e) => setReleaseForm({ ...releaseForm, notes: e.target.value })} />
        <button type="submit">Create Release + Get Upload URL</button>
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
        <select value={jobForm.target_scope} onChange={(e) => setJobForm({ ...jobForm, target_scope: e.target.value })}>
          <option value="all">All Devices</option>
          <option value="region">By Region</option>
          <option value="area">By Area</option>
          <option value="site">By Site</option>
        </select>
        <input
          placeholder="Target value (site_id/area_id/region)"
          value={jobForm.target_value}
          onChange={(e) => setJobForm({ ...jobForm, target_value: e.target.value })}
        />
        <button type="submit">Create OTA Job</button>
      </form>

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
                  <button className="danger" onClick={() => cancelJob(x.job.id)}>
                    Cancel
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
