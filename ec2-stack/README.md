# PowerEye Greenfield EC2 Stack

This folder contains a full from-scratch MVP stack for:
- MQTT ingest broker (EMQX)
- Control plane API (FastAPI, JWT, RBAC)
- Time-series DB (InfluxDB)
- Metadata/control DB (Postgres)
- MQTT telemetry ingestor worker
- Web app (login, site list, per-site dashboard, regional dashboard, OTA manager)
- Reverse proxy (Nginx)

## 1) EC2 prerequisites

- Ubuntu 22.04 LTS (`t3.large` minimum)
- Open ports:
  - `22` from your admin IP only
  - `80` and `443` public
  - `8883` public (MQTT TLS for devices)
  - `18083` optional, restrict to admin IP (EMQX dashboard)
- Install Docker and Compose plugin.

## 2) Configure environment

```bash
cd ec2-stack
cp .env.example .env
```

Edit `.env` with secure values:
- Postgres password
- JWT secret
- Influx token/password
- AWS credentials and S3 bucket for OTA firmware
- Seed admin email/password

## 3) Start the stack

```bash
docker compose up -d --build
```

Core endpoints:
- Web app: `http://<ec2-public-ip>/`
- API health: `http://<ec2-public-ip>/api/health`
- EMQX dashboard: `http://<ec2-public-ip>:18083`

## 4) TLS (production)

Use Certbot and update Nginx TLS server blocks, then force HTTPS.  
Current config is HTTP-first for initial bootstrap.

## 5) MQTT topic contract

- Telemetry publish: `powereye/{site_id}/{device_id}/telemetry`
- Status/LWT: `powereye/{site_id}/{device_id}/status`
- Optional command: `powereye/{site_id}/{device_id}/cmd`

## 6) OTA flow

1. Admin creates firmware release in web app.
2. API returns S3 signed upload URL.
3. Upload `.bin` to that URL.
4. Create OTA job (all/region/area/site target).
5. Device polls `/api/ota/check`, downloads signed URL, verifies hash, applies, then reports to `/api/ota/report`.

## Notes

- MVP is single-node, non-HA.
- Keep frequent backups of Postgres and Influx.
- Add EMQX ACL/auth hardening before production scale.
