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
    gensets = data.get("gensets", []) if isinstance(data.get("gensets"), list) else []
    battery_banks = data.get("battery_banks", []) if isinstance(data.get("battery_banks"), list) else []

    point.field("grid_voltage", float(energy.get("voltage", 0.0) or 0.0))
    point.field("grid_current", float(energy.get("current", 0.0) or 0.0))
    point.field("grid_power", float(energy.get("power", 0.0) or 0.0))
    point.field("grid_frequency", float(energy.get("frequency", 0.0) or 0.0))
    point.field("grid_power_factor", float(energy.get("power_factor", 0.0) or 0.0))
    point.field("grid_energy_kwh", float(energy.get("energy_kwh", 0.0) or 0.0))
    point.field("grid_online", bool(energy.get("online", False)))
    point.field("fuel_percent", float(fuel.get("percent", 0.0) or 0.0))
    point.field("fuel_liters", float(fuel.get("liters", 0.0) or 0.0))
    point.field("fuel_raw", int(fuel.get("raw", 0) or 0))
    point.field("fuel_online", bool(fuel.get("online", False)))
    point.field("fuel_sensor_online", bool(data.get("fuel_sensor_online", False)))
    point.field("genset_online_count", int(data.get("genset_online_count", 0) or 0))
    point.field("genset_any_alarm", bool(data.get("genset_any_alarm", False)))
    point.field("genset_count_configured", int(data.get("genset_count_configured", 0) or 0))
    point.field("battery_online_count", int(data.get("battery_online_count", 0) or 0))
    point.field("battery_bank_count_configured", int(data.get("battery_bank_count_configured", 0) or 0))
    point.field("battery_low_soc_count", int(data.get("battery_low_soc_count", 0) or 0))
    point.field("network_online", bool(data.get("network_online", False)))
    point.field("site_power_available", bool(data.get("site_power_available", False)))
    point.field("queue_pending", int(data.get("queue_pending", 0) or 0))
    point.field("rssi", int(data.get("rssi", -113) or -113))
    point.field("power_source", str(data.get("power_source", "") or ""))

    # Primary generator details (first online; fallback first configured).
    primary_gen = None
    for g in gensets:
        if isinstance(g, dict) and bool(g.get("online", False)):
            primary_gen = g
            break
    if primary_gen is None and gensets and isinstance(gensets[0], dict):
        primary_gen = gensets[0]
    if isinstance(primary_gen, dict):
        point.field("genset_mode", str(primary_gen.get("mode", "") or ""))
        point.field("genset_alarm", bool(primary_gen.get("alarm", False)))
        point.field("genset_voltage", float(primary_gen.get("voltage_a", 0.0) or 0.0))
        point.field("genset_battery_voltage", float(primary_gen.get("battery_voltage", 0.0) or 0.0))
        point.field("genset_current", float(primary_gen.get("current_a", 0.0) or 0.0))
        point.field("genset_run_hours", int(primary_gen.get("run_hours", 0) or 0))

    # Battery per-rectifier summary (4 banks per rectifier, up to RS1..RS4).
    rs_online = {1: 0, 2: 0, 3: 0, 4: 0}
    rs_soc_sum = {1: 0.0, 2: 0.0, 3: 0.0, 4: 0.0}
    rs_soc_count = {1: 0, 2: 0, 3: 0, 4: 0}
    for b in battery_banks:
        if not isinstance(b, dict):
            continue
        index = int(b.get("index", 0) or 0)
        if index < 1:
            continue
        rs = min(4, ((index - 1) // 4) + 1)
        online = bool(b.get("online", False))
        if online:
            rs_online[rs] += 1
        soc = float(b.get("soc", 0.0) or 0.0)
        if online:
            rs_soc_sum[rs] += soc
            rs_soc_count[rs] += 1
    for rs in [1, 2, 3, 4]:
        point.field(f"rs{rs}_online_count", rs_online[rs])
        avg_soc = (rs_soc_sum[rs] / rs_soc_count[rs]) if rs_soc_count[rs] > 0 else 0.0
        point.field(f"rs{rs}_avg_soc", float(avg_soc))

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
