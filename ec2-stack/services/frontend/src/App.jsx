import { Navigate, Route, Routes } from "react-router-dom";
import RequireAuth from "./components/RequireAuth";
import Layout from "./components/Layout";
import LoginPage from "./pages/LoginPage";
import SiteListPage from "./pages/SiteListPage";
import SiteDashboardPage from "./pages/SiteDashboardPageV2";
import RegionalDashboardPage from "./pages/RegionalDashboardPage";
import OtaPage from "./pages/OtaPage";
import UsersPage from "./pages/UsersPage";

export default function App() {
  return (
    <Routes>
      <Route path="/login" element={<LoginPage />} />
      <Route
        path="*"
        element={
          <RequireAuth>
            <Layout>
              <Routes>
                <Route path="/" element={<Navigate to="/sites" replace />} />
                <Route path="/sites" element={<SiteListPage />} />
                <Route path="/dashboard" element={<SiteDashboardPage />} />
                <Route path="/regional" element={<RegionalDashboardPage />} />
                <Route path="/ota" element={<OtaPage />} />
                <Route path="/users" element={<UsersPage />} />
              </Routes>
            </Layout>
          </RequireAuth>
        }
      />
    </Routes>
  );
}
