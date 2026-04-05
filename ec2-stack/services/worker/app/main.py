import json
from datetime import datetime, timezone

import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient, Point, WritePrecision
from sqlalchemy import create_engine, text

from .config import settings

engine = create_engine(settings.database_url, pool_pre_ping=True)
influx = InfluxDBClient(url=settings.influxdb_url, token=settings.influxdb_admin_token, org=settings.influxdb_org)
write_api = influx.write_api()


def parse_topic(topic: str) -> tuple[str, str, str]:
    parts = topic.split("/")
    if len(parts) < 4:
        return "", "", ""
    return parts[1], parts[2], parts[3]


def upsert_device(device_id: str, site_id: str, fw_version: str = "", transport: str = "mqtt", last_error: str = "") -> None:
    now = datetime.now(timezone.utc).replace(tzinfo=None)
    with engine.begin() as conn:
        conn.execute(
            text(
                """
                INSERT INTO device_registry (device_id, site_id, fw_version, transport_status, last_error, last_seen_at)
                VALUES (:device_id, :site_id, :fw_version, :transport_status, :last_error, :last_seen_at)
                ON CONFLICT (device_id) DO UPDATE SET
                  site_id = EXCLUDED.site_id,
                  fw_version = EXCLUDED.fw_version,
                  transport_status = EXCLUDED.transport_status,
                  last_error = EXCLUDED.last_error,
                  last_seen_at = EXCLUDED.last_seen_at
                """
            ),
            {
                "device_id": device_id,
                "site_id": site_id,
                "fw_version": fw_version,
                "transport_status": transport,
                "last_error": last_error,
                "last_seen_at": now,
            },
        )


def write_telemetry(site_id: str, device_id: str, data: dict) -> None:
    now = datetime.now(timezone.utc)
    point = (
        Point("telemetry")
        .tag("site_id", site_id)
        .tag("device_id", device_id)
        .tag("area_id", str(data.get("area_id", "")))
        .tag("region", str(data.get("region", "")))
        .time(now, WritePrecision.S)
    )

    energy = data.get("energy", {}) if isinstance(data.get("energy"), dict) else {}
    fuel = data.get("fuel", {}) if isinstance(data.get("fuel"), dict) else {}

    point.field("grid_voltage", float(energy.get("voltage", 0.0) or 0.0))
    point.field("grid_power", float(energy.get("power", 0.0) or 0.0))
    point.field("fuel_percent", float(fuel.get("percent", 0.0) or 0.0))
    point.field("fuel_liters", float(fuel.get("liters", 0.0) or 0.0))
    point.field("genset_online_count", int(data.get("genset_online_count", 0) or 0))
    point.field("battery_online_count", int(data.get("battery_online_count", 0) or 0))
    point.field("network_online", bool(data.get("network_online", False)))
    point.field("site_power_available", bool(data.get("site_power_available", False)))

    write_api.write(bucket=settings.influxdb_bucket, org=settings.influxdb_org, record=point)


def on_connect(client, userdata, flags, rc, properties=None):
    print(f"[worker] connected rc={rc}")
    client.subscribe(settings.mqtt_topic_telemetry, qos=1)
    client.subscribe(settings.mqtt_topic_status, qos=1)


def on_message(client, userdata, msg):
    site_id, device_id, msg_type = parse_topic(msg.topic)
    if not site_id or not device_id:
        return

    if msg_type == "status":
        payload = msg.payload.decode("utf-8", errors="ignore").strip().lower()
        upsert_device(device_id, site_id, transport="mqtt", last_error="" if payload == "online" else payload)
        return

    try:
        data = json.loads(msg.payload.decode("utf-8"))
        if not isinstance(data, dict):
            return
    except json.JSONDecodeError:
        upsert_device(device_id, site_id, transport="mqtt", last_error="json_decode_error")
        return

    fw = str(data.get("fw_version", "") or data.get("fw", ""))
    err = str(data.get("last_error", "") or "")
    upsert_device(device_id, site_id, fw_version=fw, transport="mqtt", last_error=err)
    write_telemetry(site_id, device_id, data)


def run() -> None:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if settings.mqtt_username:
        client.username_pw_set(settings.mqtt_username, settings.mqtt_password)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(settings.mqtt_broker_host, settings.mqtt_broker_port, 60)
    client.loop_forever()


if __name__ == "__main__":
    run()
