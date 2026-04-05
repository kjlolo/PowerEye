from datetime import datetime
import boto3
from influxdb_client import InfluxDBClient
from sqlalchemy.orm import Session

from .config import settings
from .models import AuditLog, DeviceRegistry, OtaJob, OtaReport, OtaTarget, Site


def get_influx_client() -> InfluxDBClient:
    return InfluxDBClient(url=settings.influxdb_url, token=settings.influxdb_admin_token, org=settings.influxdb_org)


def make_presigned_put_url(s3_key: str, content_type: str = "application/octet-stream") -> str:
    s3 = boto3.client(
        "s3",
        region_name=settings.aws_region,
        aws_access_key_id=settings.aws_access_key_id,
        aws_secret_access_key=settings.aws_secret_access_key,
    )
    return s3.generate_presigned_url(
        "put_object",
        Params={"Bucket": settings.s3_firmware_bucket, "Key": s3_key, "ContentType": content_type},
        ExpiresIn=settings.s3_signed_url_expiry_sec,
    )


def make_presigned_get_url(s3_key: str) -> str:
    s3 = boto3.client(
        "s3",
        region_name=settings.aws_region,
        aws_access_key_id=settings.aws_access_key_id,
        aws_secret_access_key=settings.aws_secret_access_key,
    )
    return s3.generate_presigned_url(
        "get_object",
        Params={"Bucket": settings.s3_firmware_bucket, "Key": s3_key},
        ExpiresIn=settings.s3_signed_url_expiry_sec,
    )


def write_audit(db: Session, actor: str, action: str, object_type: str, object_id: str = "", detail: str = "") -> None:
    db.add(AuditLog(actor=actor, action=action, object_type=object_type, object_id=object_id, detail=detail))
    db.commit()


def resolve_target_devices(db: Session, scope: str, value: str) -> list[DeviceRegistry]:
    q = db.query(DeviceRegistry, Site).join(Site, Site.site_id == DeviceRegistry.site_id)
    if scope == "site":
        q = q.filter(Site.site_id == value)
    elif scope == "area":
        q = q.filter(Site.area_id == value)
    elif scope == "region":
        q = q.filter(Site.region == value)
    pairs = q.all()
    return [pair[0] for pair in pairs]


def assign_ota_targets(db: Session, job: OtaJob, devices: list[DeviceRegistry]) -> None:
    for dev in devices:
        db.add(OtaTarget(job_id=job.id, device_id=dev.device_id, site_id=dev.site_id))
    db.commit()


def upsert_ota_report(db: Session, device_id: str, site_id: str, status: str, detail: str = "", job_id: int | None = None) -> OtaReport:
    report = OtaReport(job_id=job_id, device_id=device_id, site_id=site_id, status=status, detail=detail)
    db.add(report)
    db.commit()
    db.refresh(report)

    target = db.query(OtaTarget).filter(OtaTarget.device_id == device_id, OtaTarget.job_id == job_id).first() if job_id else None
    if target:
        target.status = status
        target.updated_at = datetime.utcnow()
        db.commit()
    return report
