from datetime import datetime
from pydantic import BaseModel, Field
from typing import Any


class LoginRequest(BaseModel):
    email: str
    password: str


class TokenResponse(BaseModel):
    access_token: str
    refresh_token: str
    token_type: str = "bearer"


class RefreshRequest(BaseModel):
    refresh_token: str


class UserMe(BaseModel):
    email: str
    roles: list[str]


class SiteIn(BaseModel):
    site_id: str
    site_name: str
    area_id: str
    region: str
    city: str = ""
    province: str = ""
    lat: float | None = None
    lng: float | None = None
    criticality_weight: float = 1.0
    is_active: bool = True


class SiteOut(SiteIn):
    updated_at: datetime


class FirmwareReleaseIn(BaseModel):
    version: str
    filename: str
    sha256: str
    notes: str = ""


class FirmwareReleaseOut(BaseModel):
    version: str
    s3_key: str
    sha256: str
    notes: str
    created_by: str
    created_at: datetime


class OtaJobIn(BaseModel):
    firmware_version: str
    target_scope: str = Field(pattern="^(site|area|region|all)$")
    target_value: str = ""


class OtaReportIn(BaseModel):
    device_id: str
    site_id: str
    status: str
    detail: str = ""


class OtaCheckRequest(BaseModel):
    device_id: str
    site_id: str
    fw_version: str = ""


class SiteSubsystemConfigIn(BaseModel):
    config: dict[str, Any]


class AdminUserCreateIn(BaseModel):
    email: str
    password: str
    role: str = Field(pattern="^(admin|viewer)$")
    is_active: bool = True


class AdminUserUpdateIn(BaseModel):
    role: str | None = Field(default=None, pattern="^(admin|viewer)$")
    password: str | None = None
    is_active: bool | None = None
