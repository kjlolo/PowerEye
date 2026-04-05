from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str
    influxdb_url: str
    influxdb_org: str
    influxdb_bucket: str
    influxdb_admin_token: str

    jwt_secret: str
    jwt_access_minutes: int = 15
    jwt_refresh_days: int = 7

    aws_region: str
    aws_access_key_id: str
    aws_secret_access_key: str
    s3_firmware_bucket: str
    s3_signed_url_expiry_sec: int = 900

    seed_admin_email: str = "admin@powereye.local"
    seed_admin_password: str = "admin123"


settings = Settings()
