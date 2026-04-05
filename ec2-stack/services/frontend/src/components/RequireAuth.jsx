import { Navigate } from "react-router-dom";
import { useAuth } from "../state/AuthContext";

export default function RequireAuth({ children }) {
  const { user, loading } = useAuth();
  if (loading) return <div className="loading">Loading...</div>;
  if (!user) return <Navigate to="/login" replace />;
  return children;
}
