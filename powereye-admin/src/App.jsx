import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import SiteList from "./pages/SiteList";
import SiteForm from "./pages/SiteForm";

export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Navigate to="/sites" replace />} />
        <Route path="/sites" element={<SiteList />} />
        <Route path="/sites/new" element={<SiteForm />} />
        <Route path="/sites/:site_id/edit" element={<SiteForm />} />
      </Routes>
    </BrowserRouter>
  );
}
