import { NavLink } from "react-router-dom";
import { useAuth } from "../state/AuthContext";

export default function Layout({ children }) {
  const { user, logout } = useAuth();
  const roles = user?.roles || [];
  const isAdmin = roles.includes("admin");

  return (
    <div className="app">
      <aside className="sidebar">
        <h1>Power Eye</h1>
        <p className="subtitle">Control Plane</p>
        <nav>
          <NavLink to="/sites">Site List</NavLink>
          <NavLink to="/regional">Regional View</NavLink>
          <NavLink to="/dashboard">Site Dashboard</NavLink>
          {isAdmin && <NavLink to="/ota">OTA Manager</NavLink>}
          {isAdmin && <NavLink to="/users">Users</NavLink>}
        </nav>
        <div className="userline">{user?.email || "unknown user"}</div>
        <button className="danger" onClick={logout}>
          Logout
        </button>
      </aside>
      <main className="content">{children}</main>
    </div>
  );
}
