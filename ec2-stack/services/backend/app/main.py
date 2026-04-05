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
    OtaTarget,
    Role,
    Site,
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
    TokenResponse,
    UserMe,
)
from .security import create_access_token, create_refresh_token, decode_token, hash_password, verify_password
from .services import (
    assign_ota_targets,
    get_influx_client,
    make_presigned_get_url,
    make_presigned_put_url,
    resolve_target_devices,
    upsert_ota_report,
    write_audit,
)

app = FastAPI(title="PowerEye Control Plane", version="1.0.0")

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


@app.get("/fleet/sites/{site_id}/timeseries")
def fleet_site_timeseries(
    site_id: str,
    hours: int = Query(6, ge=1, le=168),
    user: User = Depends(get_current_user),
) -> dict:
    query = f"""
from(bucket: "{settings.influxdb_bucket}")
  |> range(start: -{hours}h)
  |> filter(fn: (r) => r["_measurement"] == "telemetry")
  |> filter(fn: (r) => r["site_id"] == "{site_id}")
  |> filter(fn: (r) => r["_field"] == "grid_voltage" or r["_field"] == "fuel_percent" or r["_field"] == "genset_online_count" or r["_field"] == "battery_online_count")
"""
    client = get_influx_client()
    tables = client.query_api().query(query=query)
    rows: list[dict] = []
    for table in tables:
        for rec in table.records:  # type: FluxRecord
            rows.append({"time": rec.get_time().isoformat(), "field": rec.get_field(), "value": rec.get_value()})
    return {"ok": True, "items": rows, "count": len(rows), "viewer": user.email}


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
    items = db.query(FirmwareRelease).order_by(FirmwareRelease.created_at.desc()).all()
    return {"ok": True, "items": items, "count": len(items), "viewer": user.email}


@app.post("/ota/jobs")
def create_ota_job(payload: OtaJobIn, user: User = Depends(require_role("admin")), db: Session = Depends(get_db)) -> dict:
    release = db.query(FirmwareRelease).filter(FirmwareRelease.version == payload.firmware_version).first()
    if not release:
        raise HTTPException(status_code=404, detail="firmware_not_found")
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
