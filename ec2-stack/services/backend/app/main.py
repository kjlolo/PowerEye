from datetime import datetime, timezone
from fastapi import Depends, FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.encoders import jsonable_encoder
from sqlalchemy.orm import Session
from influxdb_client.client.flux_table import FluxRecord

from .config import settings
from .db import Base, SessionLocal, engine, get_db
from .deps import get_current_user, require_role
from .models import (
    DeviceRegistry,
    FirmwareRelease,
    OtaJob,
    OtaReport,
    OtaTarget,
    Role,
    Site,
    SiteSubsystemConfig,
    User,
    UserRole,
)
from .schemas import (
    FirmwareReleaseIn,
    LoginRequest,
    OtaCheckRequest,
    OtaJobIn,
    OtaReportIn,
    RefreshRequest,
    SiteIn,
    SiteSubsystemConfigIn,
    TokenResponse,
    UserMe,
)
from .security import create_access_token, create_refresh_token, decode_token, hash_password, verify_password
from .services import (
    assign_ota_targets,
    get_s3_object_meta,
    get_influx_client,
    make_presigned_get_url,
    make_presigned_put_url,
    resolve_target_devices,
    upsert_ota_report,
    write_audit,
)

app = FastAPI(title="PowerEye Control Plane", version="1.0.0")
NETWORK_HEARTBEAT_STALE_SEC = 180

BLOCKED_REMOTE_CONFIG_KEYS = {"identity", "cloud"}
ALLOWED_REMOTE_CONFIG_KEYS = {"rs485", "fuel", "alarms", "power_availability"}
ALARM_FIELDS = {
    "alarm_ac_mains": "AC Mains Failure",
    "alarm_genset_run": "Genset Running",
    "alarm_genset_fail": "Genset Fail",
    "alarm_battery_theft": "Battery Theft",
    "alarm_power_cable_theft": "Power Cable Theft",
    "alarm_door_open": "Door Open",
    "genset_any_alarm": "Generator Alarm",
    "alarm_grid_offline": "Grid Subsystem Offline",
    "alarm_fuel_offline": "Fuel Subsystem Offline",
    "alarm_genset_offline": "Generator Subsystem Offline",
    "alarm_battery_offline": "Battery Subsystem Offline",
}
ALARM_SEVERITY = {
    "alarm_ac_mains": "critical",
    "alarm_genset_run": "minor",
    "alarm_genset_fail": "critical",
    "alarm_battery_theft": "critical",
    "alarm_power_cable_theft": "critical",
    "alarm_door_open": "major",
    "genset_any_alarm": "major",
    "alarm_grid_offline": "major",
    "alarm_fuel_offline": "major",
    "alarm_genset_offline": "major",
    "alarm_battery_offline": "major",
}

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


def seed_defaults(db: Session) -> None:
    admin_role = db.query(Role).filter(Role.name == "admin").first()
    viewer_role = db.query(Role).filter(Role.name == "viewer").first()
    if not admin_role:
        admin_role = Role(name="admin")
        db.add(admin_role)
    if not viewer_role:
        viewer_role = Role(name="viewer")
        db.add(viewer_role)
    db.commit()

    admin = db.query(User).filter(User.email == settings.seed_admin_email).first()
    if not admin:
        admin = User(email=settings.seed_admin_email, password_hash=hash_password(settings.seed_admin_password), is_active=True)
        db.add(admin)
        db.commit()
        db.refresh(admin)
        db.add(UserRole(user_id=admin.id, role_id=admin_role.id))
        db.commit()


@app.on_event("startup")
def startup() -> None:
    Base.metadata.create_all(bind=engine)
    db = SessionLocal()
    try:
        seed_defaults(db)
    finally:
        db.close()


@app.get("/health")
def health() -> dict:
    return {"ok": True, "ts": datetime.now(timezone.utc).isoformat()}


@app.post("/auth/login", response_model=TokenResponse)
def login(payload: LoginRequest, db: Session = Depends(get_db)) -> TokenResponse:
    user = db.query(User).filter(User.email == payload.email, User.is_active.is_(True)).first()
    if not user or not verify_password(payload.password, user.password_hash):
        raise HTTPException(status_code=401, detail="invalid_credentials")
    return TokenResponse(access_token=create_access_token(user.email), refresh_token=create_refresh_token(user.email))


@app.post("/auth/refresh", response_model=TokenResponse)
def refresh(payload: RefreshRequest) -> TokenResponse:
    token = decode_token(payload.refresh_token)
    if token.get("type") != "refresh":
        raise HTTPException(status_code=401, detail="invalid_token_type")
    sub = token.get("sub")
    return TokenResponse(access_token=create_access_token(sub), refresh_token=create_refresh_token(sub))


@app.post("/auth/logout")
def logout(_: User = Depends(get_current_user)) -> dict:
    return {"ok": True}


@app.get("/auth/me", response_model=UserMe)
def me(user: User = Depends(get_current_user)) -> UserMe:
    return UserMe(email=user.email, roles=[ur.role.name for ur in user.roles])


@app.get("/sites")
def list_sites(
    search: str = Query("", description="search by site id/name"),
    area_id: str = "",
    region: str = "",
    user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> dict:
    q = db.query(Site)
    if search:
        term = f"%{search}%"
        q = q.filter((Site.site_id.ilike(term)) | (Site.site_name.ilike(term)))
    if area_id:
        q = q.filter(Site.area_id == area_id)
    if region:
        q = q.filter(Site.region == region)
    sites = q.order_by(Site.site_id.asc()).all()
    items = [
        {
            "site_id": s.site_id,
            "site_name": s.site_name,
            "area_id": s.area_id,
            "region": s.region,
            "city": s.city,
            "province": s.province,
            "lat": s.lat,
            "lng": s.lng,
            "criticality_weight": s.criticality_weight,
            "is_active": s.is_active,
            "updated_at": s.updated_at,
        }
        for s in sites
    ]
    return {"ok": True, "items": jsonable_encoder(items), "count": len(items), "viewer": user.email}


@app.post("/sites")
def create_site(payload: SiteIn, user: User = Depends(require_role("admin")), db: Session = Depends(get_db)) -> dict:
    exists = db.query(Site).filter(Site.site_id == payload.site_id).first()
    if exists:
        raise HTTPException(status_code=409, detail="site_exists")
    item = Site(**payload.model_dump(), updated_at=datetime.utcnow())
    db.add(item)
    db.commit()
    db.refresh(item)
    write_audit(db, user.email, "site.create", "site", payload.site_id)
    return {"ok": True, "item": item}


@app.put("/sites/{site_id}")
def update_site(site_id: str, payload: SiteIn, user: User = Depends(require_role("admin")), db: Session = Depends(get_db)) -> dict:
    item = db.query(Site).filter(Site.site_id == site_id).first()
    if not item:
        raise HTTPException(status_code=404, detail="site_not_found")
    data = payload.model_dump()
    for k, v in data.items():
        setattr(item, k, v)
    item.site_id = site_id
    item.updated_at = datetime.utcnow()
    db.commit()
    db.refresh(item)
    write_audit(db, user.email, "site.update", "site", site_id)
    return {"ok": True, "item": item}


@app.delete("/sites/{site_id}")
def delete_site(site_id: str, user: User = Depends(require_role("admin")), db: Session = Depends(get_db)) -> dict:
    item = db.query(Site).filter(Site.site_id == site_id).first()
    if not item:
        raise HTTPException(status_code=404, detail="site_not_found")
    db.delete(item)
    db.commit()
    write_audit(db, user.email, "site.delete", "site", site_id)
    return {"ok": True, "site_id": site_id}


@app.get("/fleet/overview")
def fleet_overview(user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> dict:
    sites = db.query(Site).filter(Site.is_active.is_(True)).all()
    devices = db.query(DeviceRegistry).all()
    by_site = {d.site_id: d for d in devices}
    summary = []
    for s in sites:
        d = by_site.get(s.site_id)
        summary.append(
            {
                "site_id": s.site_id,
                "site_name": s.site_name,
                "area_id": s.area_id,
                "region": s.region,
                "last_seen_at": d.last_seen_at.isoformat() if d and d.last_seen_at else None,
                "transport_status": d.transport_status if d else "unknown",
            }
        )
    return {"ok": True, "items": summary, "count": len(summary), "viewer": user.email}


@app.get("/fleet/regional-view")
def fleet_regional_view(
    region: str = "",
    user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> dict:
    q = db.query(Site).filter(Site.is_active.is_(True))
    if region:
        q = q.filter(Site.region == region)
    sites = q.order_by(Site.area_id.asc(), Site.site_id.asc()).all()
    if not sites:
        return {"ok": True, "region": region, "summary": {}, "areas": [], "viewer": user.email}

    site_ids = [s.site_id for s in sites]
    site_meta = {s.site_id: s for s in sites}
    quoted_sites = ",".join([f'"{sid}"' for sid in site_ids])
    query = f"""
from(bucket: "{settings.influxdb_bucket}")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "telemetry")
  |> filter(fn: (r) => r["_field"] == "site_power_available" or r["_field"] == "power_source")
  |> filter(fn: (r) => contains(value: r["site_id"], set: [{quoted_sites}]))
  |> group(columns: ["site_id","_field"])
  |> last()
"""
    client = get_influx_client()
    tables = client.query_api().query(query=query)
    latest_by_site: dict[str, dict] = {}
    for table in tables:
        for rec in table.records:  # type: FluxRecord
            sid = str(rec.values.get("site_id", ""))
            if not sid:
                continue
            row = latest_by_site.get(sid, {"site_id": sid, "site_power_available": False, "power_source": "none", "ts": None})
            field = str(rec.get_field())
            if field == "site_power_available":
                row["site_power_available"] = bool(rec.get_value())
            elif field == "power_source":
                row["power_source"] = str(rec.get_value() or "none")
            ts = rec.get_time()
            if ts is not None and (row["ts"] is None or ts > row["ts"]):
                row["ts"] = ts
            latest_by_site[sid] = row

    devices = db.query(DeviceRegistry).all()
    by_site_device = {d.site_id: d for d in devices}

    area_map: dict[str, dict] = {}
    region_total = len(sites)
    region_available = 0
    region_stale = 0
    for sid in site_ids:
        s = site_meta[sid]
        lv = latest_by_site.get(sid, {"site_id": sid, "site_power_available": False, "power_source": "none", "ts": None})
        dev = by_site_device.get(sid)
        stale = dev is None or dev.last_seen_at is None
        available = bool(lv.get("site_power_available", False)) and not stale
        if available:
            region_available += 1
        if stale:
            region_stale += 1
        area = s.area_id or "UNASSIGNED"
        a = area_map.get(area)
        if not a:
            a = {"area_id": area, "total_sites": 0, "available_sites": 0, "stale_sites": 0, "contributors": []}
            area_map[area] = a
        a["total_sites"] += 1
        if available:
            a["available_sites"] += 1
        if stale:
            a["stale_sites"] += 1
        if (not available) or stale:
            reason = "stale"
            if not stale and lv.get("power_source") == "none":
                reason = "no_power_source"
            elif not stale and not lv.get("site_power_available", False):
                reason = "power_unavailable"
            a["contributors"].append(
                {
                    "site_id": sid,
                    "site_name": s.site_name,
                    "reason": reason,
                    "power_source": lv.get("power_source", "none"),
                    "last_seen_at": dev.last_seen_at.isoformat() if dev and dev.last_seen_at else None,
                }
            )

    areas = []
    for area_id, a in area_map.items():
        total = a["total_sites"]
        avail_pct = (a["available_sites"] / total * 100.0) if total else 0.0
        severity_order = {"stale": 0, "power_unavailable": 1, "no_power_source": 2}
        contributors = sorted(a["contributors"], key=lambda c: severity_order.get(c["reason"], 99))[:5]
        areas.append(
            {
                "area_id": area_id,
                "total_sites": total,
                "available_sites": a["available_sites"],
                "stale_sites": a["stale_sites"],
                "availability_pct": round(avail_pct, 2),
                "contributors": contributors,
            }
        )
    areas.sort(key=lambda x: x["area_id"])
    region_availability = (region_available / region_total * 100.0) if region_total else 0.0
    summary = {
        "region": region or (sites[0].region if sites else ""),
        "total_sites": region_total,
        "available_sites": region_available,
        "stale_sites": region_stale,
        "availability_pct": round(region_availability, 2),
        "area_count": len(areas),
    }
    return {"ok": True, "region": summary["region"], "summary": summary, "areas": areas, "viewer": user.email}


@app.get("/fleet/sites/{site_id}/latest")
def fleet_site_latest(site_id: str, user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> dict:
    device = db.query(DeviceRegistry).filter(DeviceRegistry.site_id == site_id).first()
    if not device:
        return {"ok": True, "item": None}
    return {
        "ok": True,
        "item": {
            "device_id": device.device_id,
            "site_id": device.site_id,
            "fw_version": device.fw_version,
            "transport_status": device.transport_status,
            "last_error": device.last_error,
            "last_seen_at": device.last_seen_at.isoformat() if device.last_seen_at else None,
            "viewer": user.email,
        },
    }


@app.get("/fleet/sites/{site_id}/live")
def fleet_site_live(
    site_id: str,
    user: User = Depends(get_current_user),
) -> dict:
    fields = [
        "grid_voltage",
        "grid_current",
        "grid_power",
        "grid_frequency",
        "grid_power_factor",
        "grid_energy_kwh",
        "grid_online",
        "fuel_percent",
        "fuel_liters",
        "fuel_raw",
        "fuel_online",
        "fuel_sensor_online",
        "genset_online_count",
        "genset_any_alarm",
        "genset_count_configured",
        "genset_mode",
        "genset_alarm",
        "genset_voltage",
        "genset_battery_voltage",
        "genset_current",
        "genset_run_hours",
        "battery_online_count",
        "battery_discharging_count",
        "battery_charging_count",
        "battery_discharging_active",
        "cfg_pzem_enabled",
        "cfg_generator_enabled",
        "cfg_battery_enabled",
        "cfg_fuel_enabled",
        "battery_bank_count_configured",
        "battery_low_soc_count",
        "rs1_online_count",
        "rs1_avg_soc",
        "rs2_online_count",
        "rs2_avg_soc",
        "rs3_online_count",
        "rs3_avg_soc",
        "rs4_online_count",
        "rs4_avg_soc",
        "network_online",
        "site_power_available",
        "power_supply_grid",
        "power_supply_genset",
        "power_supply_battery",
        "power_source",
        "queue_pending",
        "rssi",
    ]
    for i in range(1, 17):
        fields.extend(
            [
                f"bank_{i}_online",
                f"bank_{i}_voltage",
                f"bank_{i}_current",
                f"bank_{i}_soc",
                f"bank_{i}_soh",
                f"bank_{i}_alarm",
            ]
        )
    field_filter = " or ".join([f'r["_field"] == "{f}"' for f in fields])
    query = f"""
from(bucket: "{settings.influxdb_bucket}")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "telemetry")
  |> filter(fn: (r) => r["site_id"] == "{site_id}")
  |> filter(fn: (r) => {field_filter})
  |> last()
"""
    client = get_influx_client()
    tables = client.query_api().query(query=query)
    values: dict[str, float | int | bool | str] = {}
    last_ts: datetime | None = None
    for table in tables:
        for rec in table.records:  # type: FluxRecord
            field = str(rec.get_field())
            value = rec.get_value()
            values[field] = value
            ts = rec.get_time()
            if ts is not None and (last_ts is None or ts > last_ts):
                last_ts = ts

    now_utc = datetime.now(timezone.utc)
    age_sec: int | None = None
    if last_ts is not None:
        age_sec = int((now_utc - last_ts).total_seconds())
        if age_sec < 0:
            age_sec = 0

    return {
        "ok": True,
        "site_id": site_id,
        "last_telemetry_at": last_ts.isoformat() if last_ts else None,
        "last_telemetry_age_sec": age_sec,
        "network_heartbeat_online": bool(values.get("network_online", False)) and (age_sec is not None and age_sec <= NETWORK_HEARTBEAT_STALE_SEC),
        "values": values,
        "viewer": user.email,
    }


@app.get("/fleet/sites/{site_id}/timeseries")
def fleet_site_timeseries(
    site_id: str,
    hours: int = Query(6, ge=1, le=168),
    start: str = Query("", description="ISO8601 UTC start time"),
    end: str = Query("", description="ISO8601 UTC end time"),
    user: User = Depends(get_current_user),
) -> dict:
    fields = [
        "grid_voltage",
        "grid_current",
        "grid_power",
        "grid_frequency",
        "grid_power_factor",
        "grid_energy_kwh",
        "fuel_percent",
        "fuel_liters",
        "genset_online_count",
        "battery_online_count",
        "battery_discharging_count",
        "battery_charging_count",
        "battery_discharging_active",
        "power_supply_grid",
        "power_supply_genset",
        "power_supply_battery",
        "site_power_available",
    ]
    field_filter = " or ".join([f'r["_field"] == "{f}"' for f in fields])
    safe_start = start.replace('"', "").strip()
    safe_end = end.replace('"', "").strip()
    if safe_start:
        range_clause = f'|> range(start: time(v: "{safe_start}")'
        if safe_end:
            range_clause += f', stop: time(v: "{safe_end}")'
        range_clause += ")"
    else:
        range_clause = f"|> range(start: -{hours}h)"

    query = f"""
from(bucket: "{settings.influxdb_bucket}")
  {range_clause}
  |> filter(fn: (r) => r["_measurement"] == "telemetry")
  |> filter(fn: (r) => r["site_id"] == "{site_id}")
  |> filter(fn: (r) => {field_filter})
"""
    client = get_influx_client()
    tables = client.query_api().query(query=query)
    rows: list[dict] = []
    for table in tables:
        for rec in table.records:  # type: FluxRecord
            rows.append({"time": rec.get_time().isoformat(), "field": rec.get_field(), "value": rec.get_value()})
    return {"ok": True, "items": rows, "count": len(rows), "viewer": user.email}


@app.get("/fleet/sites/{site_id}/events")
def fleet_site_events(
    site_id: str,
    hours: int = Query(24, ge=1, le=168),
    start: str = Query("", description="ISO8601 UTC start time"),
    end: str = Query("", description="ISO8601 UTC end time"),
    user: User = Depends(get_current_user),
) -> dict:
    alarm_field_filter = " or ".join([f'r["_field"] == "{f}"' for f in ALARM_FIELDS.keys()])
    safe_start = start.replace('"', "").strip()
    safe_end = end.replace('"', "").strip()
    if safe_start:
        history_range = f'|> range(start: time(v: "{safe_start}")'
        if safe_end:
            history_range += f', stop: time(v: "{safe_end}")'
        history_range += ")"
    else:
        history_range = f"|> range(start: -{hours}h)"

    history_query = f"""
from(bucket: "{settings.influxdb_bucket}")
  {history_range}
  |> filter(fn: (r) => r["_measurement"] == "telemetry")
  |> filter(fn: (r) => r["site_id"] == "{site_id}")
  |> filter(fn: (r) => {alarm_field_filter})
"""
    active_query = f"""
from(bucket: "{settings.influxdb_bucket}")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "telemetry")
  |> filter(fn: (r) => r["site_id"] == "{site_id}")
  |> filter(fn: (r) => {alarm_field_filter})
  |> last()
"""

    client = get_influx_client()
    history_tables = client.query_api().query(query=history_query)
    active_tables = client.query_api().query(query=active_query)

    by_field: dict[str, list[dict]] = {k: [] for k in ALARM_FIELDS.keys()}
    for table in history_tables:
        for rec in table.records:  # type: FluxRecord
            field = str(rec.get_field())
            if field not in ALARM_FIELDS:
                continue
            by_field[field].append(
                {
                    "time": rec.get_time(),
                    "value": bool(rec.get_value()),
                }
            )

    historical: list[dict] = []
    active_alarms: list[dict] = []
    for field, rows in by_field.items():
        if not rows:
            continue
        rows.sort(key=lambda x: x["time"])
        in_alarm = False
        alarm_start: datetime | None = None
        for row in rows:
            current = bool(row["value"])
            ts = row["time"]
            if (not in_alarm) and current:
                in_alarm = True
                alarm_start = ts
            elif in_alarm and (not current):
                historical.append(
                    {
                        "alarm_time": alarm_start.isoformat() if alarm_start else ts.isoformat(),
                        "clear_time": ts.isoformat(),
                        "alarm_key": field,
                        "alarm_label": ALARM_FIELDS[field],
                        "severity": ALARM_SEVERITY.get(field, "major"),
                    }
                )
                in_alarm = False
                alarm_start = None
        if in_alarm:
            active_alarms.append(
                {
                    "alarm_time": alarm_start.isoformat() if alarm_start else rows[-1]["time"].isoformat(),
                    "alarm_key": field,
                    "alarm_label": ALARM_FIELDS[field],
                    "severity": ALARM_SEVERITY.get(field, "major"),
                    "active": True,
                }
            )

    active_map: dict[str, bool] = {}
    for table in active_tables:
        for rec in table.records:  # type: FluxRecord
            field = str(rec.get_field())
            if field in ALARM_FIELDS:
                active_map[field] = bool(rec.get_value())
    # Keep only alarms that are currently active in latest point.
    active_alarms = [a for a in active_alarms if bool(active_map.get(a["alarm_key"], False))]
    historical.sort(key=lambda x: x["clear_time"], reverse=True)
    active_alarms.sort(key=lambda x: x["alarm_time"], reverse=True)

    return {
        "ok": True,
        "site_id": site_id,
        "active_alarms": active_alarms,
        "history": historical,
        "count": len(historical),
        "viewer": user.email,
    }


@app.get("/fleet/sites/{site_id}/subsystem-config")
def get_site_subsystem_config(
    site_id: str,
    user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
) -> dict:
    site = db.query(Site).filter(Site.site_id == site_id).first()
    if not site:
        raise HTTPException(status_code=404, detail="site_not_found")
    item = db.query(SiteSubsystemConfig).filter(SiteSubsystemConfig.site_id == site_id).first()
    if not item:
        return {"ok": True, "site_id": site_id, "config": {}, "updated_by": None, "updated_at": None, "viewer": user.email}
    return {
        "ok": True,
        "site_id": site_id,
        "config": item.config_json or {},
        "updated_by": item.updated_by,
        "updated_at": item.updated_at.isoformat() if item.updated_at else None,
        "viewer": user.email,
    }


@app.put("/fleet/sites/{site_id}/subsystem-config")
def put_site_subsystem_config(
    site_id: str,
    payload: SiteSubsystemConfigIn,
    user: User = Depends(require_role("admin")),
    db: Session = Depends(get_db),
) -> dict:
    site = db.query(Site).filter(Site.site_id == site_id).first()
    if not site:
        raise HTTPException(status_code=404, detail="site_not_found")

    cfg = payload.config or {}
    blocked = sorted([k for k in cfg.keys() if k in BLOCKED_REMOTE_CONFIG_KEYS])
    if blocked:
        raise HTTPException(
            status_code=400,
            detail=f"blocked_keys: {','.join(blocked)} (identity/cloud cannot be changed remotely)",
        )
    invalid = sorted([k for k in cfg.keys() if k not in ALLOWED_REMOTE_CONFIG_KEYS])
    if invalid:
        raise HTTPException(
            status_code=400,
            detail=f"invalid_keys: {','.join(invalid)} (allowed: rs485,fuel,alarms,power_availability)",
        )

    item = db.query(SiteSubsystemConfig).filter(SiteSubsystemConfig.site_id == site_id).first()
    if not item:
        item = SiteSubsystemConfig(site_id=site_id, config_json=cfg, updated_by=user.email, updated_at=datetime.utcnow())
        db.add(item)
    else:
        item.config_json = cfg
        item.updated_by = user.email
        item.updated_at = datetime.utcnow()
    db.commit()
    db.refresh(item)
    write_audit(db, user.email, "site.subsystem_config.update", "site", site_id, detail=str(cfg))
    return {
        "ok": True,
        "site_id": site_id,
        "config": item.config_json or {},
        "updated_by": item.updated_by,
        "updated_at": item.updated_at.isoformat() if item.updated_at else None,
    }


@app.post("/ota/firmware")
def create_firmware_release(payload: FirmwareReleaseIn, user: User = Depends(require_role("admin")), db: Session = Depends(get_db)) -> dict:
    exists = db.query(FirmwareRelease).filter(FirmwareRelease.version == payload.version).first()
    if exists:
        raise HTTPException(status_code=409, detail="version_exists")
    key = f"firmware/{payload.version}/{payload.filename}"
    upload_url = make_presigned_put_url(key)
    rel = FirmwareRelease(
        version=payload.version,
        s3_key=key,
        sha256=payload.sha256,
        notes=payload.notes,
        created_by=user.email,
    )
    db.add(rel)
    db.commit()
    db.refresh(rel)
    write_audit(db, user.email, "ota.release.create", "firmware_release", payload.version)
    return {"ok": True, "item": rel, "upload_url": upload_url}


@app.get("/ota/firmware")
def list_firmware(user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> dict:
    rows = db.query(FirmwareRelease).order_by(FirmwareRelease.created_at.desc()).all()
    items: list[dict] = []
    for rel in rows:
        meta = get_s3_object_meta(rel.s3_key)
        items.append(
            {
                "id": rel.id,
                "version": rel.version,
                "s3_key": rel.s3_key,
                "sha256": rel.sha256,
                "notes": rel.notes,
                "created_by": rel.created_by,
                "created_at": rel.created_at,
                "uploaded": bool(meta),
                "size_bytes": int(meta["size_bytes"]) if meta else 0,
                "uploaded_at": meta["last_modified"] if meta else None,
            }
        )
    return {"ok": True, "items": items, "count": len(items), "viewer": user.email}


@app.get("/ota/firmware/{version}/upload-status")
def ota_firmware_upload_status(version: str, user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> dict:
    rel = db.query(FirmwareRelease).filter(FirmwareRelease.version == version).first()
    if not rel:
        raise HTTPException(status_code=404, detail="firmware_not_found")
    meta = get_s3_object_meta(rel.s3_key)
    return {
        "ok": True,
        "version": version,
        "uploaded": bool(meta),
        "size_bytes": int(meta["size_bytes"]) if meta else 0,
        "uploaded_at": meta["last_modified"] if meta else None,
        "viewer": user.email,
    }


@app.post("/ota/jobs")
def create_ota_job(payload: OtaJobIn, user: User = Depends(require_role("admin")), db: Session = Depends(get_db)) -> dict:
    if payload.target_scope != "all" and not payload.target_value.strip():
        raise HTTPException(status_code=400, detail="target_value_required_for_scope")
    release = db.query(FirmwareRelease).filter(FirmwareRelease.version == payload.firmware_version).first()
    if not release:
        raise HTTPException(status_code=404, detail="firmware_not_found")
    if not get_s3_object_meta(release.s3_key):
        raise HTTPException(status_code=400, detail="firmware_binary_not_uploaded")
    job = OtaJob(
        firmware_version=payload.firmware_version,
        target_scope=payload.target_scope,
        target_value=payload.target_value,
        created_by=user.email,
        status="scheduled",
    )
    db.add(job)
    db.commit()
    db.refresh(job)
    devices = resolve_target_devices(db, payload.target_scope, payload.target_value)
    assign_ota_targets(db, job, devices)
    write_audit(db, user.email, "ota.job.create", "ota_job", str(job.id), detail=f"targets={len(devices)}")
    return {"ok": True, "item": job, "target_count": len(devices)}


@app.get("/ota/jobs")
def list_ota_jobs(user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> dict:
    items = db.query(OtaJob).order_by(OtaJob.created_at.desc()).all()
    out = []
    for job in items:
        total = db.query(OtaTarget).filter(OtaTarget.job_id == job.id).count()
        done = db.query(OtaTarget).filter(OtaTarget.job_id == job.id, OtaTarget.status.in_(["success", "failed"])).count()
        out.append({"job": job, "total_targets": total, "completed_targets": done})
    return {"ok": True, "items": out, "count": len(out), "viewer": user.email}


@app.get("/ota/jobs/{job_id}")
def get_ota_job(job_id: int, user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> dict:
    job = db.query(OtaJob).filter(OtaJob.id == job_id).first()
    if not job:
        raise HTTPException(status_code=404, detail="job_not_found")
    targets = db.query(OtaTarget).filter(OtaTarget.job_id == job_id).all()
    return {"ok": True, "item": job, "targets": targets, "viewer": user.email}


@app.get("/ota/jobs/{job_id}/reports")
def get_ota_job_reports(job_id: int, user: User = Depends(get_current_user), db: Session = Depends(get_db)) -> dict:
    job = db.query(OtaJob).filter(OtaJob.id == job_id).first()
    if not job:
        raise HTTPException(status_code=404, detail="job_not_found")
    items = db.query(OtaReport).filter(OtaReport.job_id == job_id).order_by(OtaReport.reported_at.desc()).limit(500).all()
    return {"ok": True, "items": items, "count": len(items), "viewer": user.email}


@app.post("/ota/jobs/{job_id}/cancel")
def cancel_ota_job(job_id: int, user: User = Depends(require_role("admin")), db: Session = Depends(get_db)) -> dict:
    job = db.query(OtaJob).filter(OtaJob.id == job_id).first()
    if not job:
        raise HTTPException(status_code=404, detail="job_not_found")
    job.status = "cancelled"
    db.commit()
    write_audit(db, user.email, "ota.job.cancel", "ota_job", str(job_id))
    return {"ok": True, "item": job}


@app.get("/ota/check")
def ota_check(
    device_id: str,
    site_id: str,
    fw_version: str = "",
    db: Session = Depends(get_db),
) -> dict:
    active_target = (
        db.query(OtaTarget, OtaJob)
        .join(OtaJob, OtaJob.id == OtaTarget.job_id)
        .filter(
            OtaTarget.device_id == device_id,
            OtaTarget.site_id == site_id,
            OtaTarget.status.in_(["pending", "in_progress"]),
            OtaJob.status.in_(["scheduled", "running"]),
        )
        .order_by(OtaJob.created_at.desc())
        .first()
    )
    if not active_target:
        return {"ok": True, "has_update": False}
    target, job = active_target
    rel = db.query(FirmwareRelease).filter(FirmwareRelease.version == job.firmware_version).first()
    if not rel:
        return {"ok": True, "has_update": False}
    if fw_version and fw_version == rel.version:
        return {"ok": True, "has_update": False}

    target.status = "in_progress"
    target.updated_at = datetime.utcnow()
    if job.status == "scheduled":
        job.status = "running"
    db.commit()

    return {
        "ok": True,
        "has_update": True,
        "job_id": job.id,
        "version": rel.version,
        "sha256": rel.sha256,
        "url": make_presigned_get_url(rel.s3_key),
    }


@app.post("/ota/report")
def ota_report(payload: OtaReportIn, job_id: int | None = None, db: Session = Depends(get_db)) -> dict:
    report = upsert_ota_report(db, payload.device_id, payload.site_id, payload.status, payload.detail, job_id=job_id)
    return {"ok": True, "item": report}
