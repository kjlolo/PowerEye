# EC2 Deployment Checklist

1. Provision EC2 Ubuntu 22.04 (`t3.large` minimum).
2. Security Group:
   - `22/tcp` from admin IP only
   - `80/tcp` from 0.0.0.0/0
   - `443/tcp` from 0.0.0.0/0
   - `8883/tcp` from device network ranges
   - optional `18083/tcp` from admin IP only
3. Attach IAM role with `s3:PutObject`, `s3:GetObject`, `s3:ListBucket` to firmware bucket (or use access keys in `.env`).
4. Copy `ec2-stack` to EC2, then:
   - `cp .env.example .env`
   - edit `.env`
   - `docker compose up -d --build`
5. Validate:
   - `curl http://<ec2-ip>/api/health`
   - login web UI with seed admin credentials
   - confirm MQTT publish reaches Influx via worker logs.
