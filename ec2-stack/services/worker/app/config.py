from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str
    influxdb_url: str
    influxdb_org: str
    influxdb_bucket: str
    influxdb_admin_token: str

    mqtt_broker_host: str = "emqx"
    mqtt_broker_port: int = 1883
    mqtt_username: str = ""
    mqtt_password: str = ""
    mqtt_topic_telemetry: str = "powereye/+/+/telemetry"
    mqtt_topic_status: str = "powereye/+/+/status"


settings = Settings()
