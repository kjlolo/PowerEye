import { useEffect, useMemo, useState } from "react";
import api from "../api";
import { useAuth } from "../state/AuthContext";

export default function UsersPage() {
  const { user } = useAuth();
  const isAdmin = useMemo(() => (user?.roles || []).includes("admin"), [user]);
  const [items, setItems] = useState([]);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState("");
  const [form, setForm] = useState({ email: "", password: "", role: "viewer", is_active: true });
  const [pwdEdit, setPwdEdit] = useState({});

  const load = async () => {
    const { data } = await api.get("/admin/users");
    setItems(data.items || []);
  };

  useEffect(() => {
    if (isAdmin) load();
  }, [isAdmin]);

  const createUser = async (e) => {
    e.preventDefault();
    try {
      setBusy(true);
      setMessage("");
      await api.post("/admin/users", form);
      setForm({ email: "", password: "", role: "viewer", is_active: true });
      setMessage("User created.");
      await load();
    } catch (err) {
      setMessage(err?.response?.data?.detail || err?.message || "Create user failed.");
    } finally {
      setBusy(false);
    }
  };

  const saveUser = async (u) => {
    try {
      setBusy(true);
      const payload = {
        role: (u.roles || [])[0] || "viewer",
        is_active: !!u.is_active,
      };
      const newPwd = (pwdEdit[u.id] || "").trim();
      if (newPwd) payload.password = newPwd;
      await api.patch(`/admin/users/${u.id}`, payload);
      setPwdEdit((prev) => ({ ...prev, [u.id]: "" }));
      setMessage(`Updated ${u.email}.`);
      await load();
    } catch (err) {
      setMessage(err?.response?.data?.detail || err?.message || "Update user failed.");
    } finally {
      setBusy(false);
    }
  };

  if (!isAdmin) return <div className="card"><h3>Users</h3><p>Admin role required.</p></div>;

  return (
    <div>
      <h2>Users</h2>
      {message ? <div className="card"><div className="meta-line">{message}</div></div> : null}

      <form className="card form-grid" onSubmit={createUser}>
        <h3>Create User</h3>
        <input
          placeholder="Email"
          value={form.email}
          onChange={(e) => setForm({ ...form, email: e.target.value })}
          required
        />
        <input
          type="password"
          placeholder="Password (8-72 chars)"
          value={form.password}
          onChange={(e) => setForm({ ...form, password: e.target.value })}
          required
        />
        <select value={form.role} onChange={(e) => setForm({ ...form, role: e.target.value })}>
          <option value="viewer">viewer</option>
          <option value="admin">admin</option>
        </select>
        <label className="field-check">
          <input
            type="checkbox"
            checked={!!form.is_active}
            onChange={(e) => setForm({ ...form, is_active: e.target.checked })}
          />
          Active
        </label>
        <button type="submit" disabled={busy}>{busy ? "Working..." : "Create User"}</button>
      </form>

      <div className="card">
        <h3>User Accounts</h3>
        <table>
          <thead>
            <tr>
              <th>Email</th>
              <th>Role</th>
              <th>Active</th>
              <th>Reset Password</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            {items.map((u) => (
              <tr key={u.id}>
                <td>{u.email}</td>
                <td>
                  <select
                    value={(u.roles || [])[0] || "viewer"}
                    onChange={(e) => {
                      const role = e.target.value;
                      setItems((prev) => prev.map((x) => (x.id === u.id ? { ...x, roles: [role] } : x)));
                    }}
                  >
                    <option value="viewer">viewer</option>
                    <option value="admin">admin</option>
                  </select>
                </td>
                <td>
                  <input
                    type="checkbox"
                    checked={!!u.is_active}
                    onChange={(e) => {
                      const is_active = e.target.checked;
                      setItems((prev) => prev.map((x) => (x.id === u.id ? { ...x, is_active } : x)));
                    }}
                  />
                </td>
                <td>
                  <input
                    type="password"
                    placeholder="new password"
                    value={pwdEdit[u.id] || ""}
                    onChange={(e) => setPwdEdit((prev) => ({ ...prev, [u.id]: e.target.value }))}
                  />
                </td>
                <td>
                  <button type="button" onClick={() => saveUser(u)} disabled={busy}>Save</button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

