# Site Registry API Lambda

CRUD Lambda for `powereye-site-registry` DynamoDB table.

## Environment Variables

- `TABLE_NAME` (required): DynamoDB table name (PK: `site_id`)
- `CORS_ORIGIN` (optional): defaults to `*`
- `API_TOKEN` (optional): bearer token. If set, requires `Authorization: Bearer <API_TOKEN>`

## Deploy With AWS SAM

From this folder (`aws/site_registry_api`):

```bash
sam build
sam deploy --guided
```

Suggested parameter values during guided deploy:

- `TableName`: `powereye-site-registry`
- `CorsOrigin`: `*` (or your CloudFront URL for production)
- `ApiToken`: your admin bearer token (optional)

After deploy, copy `ApiBaseUrl` output to frontend env:

```env
VITE_API_BASE_URL=<ApiBaseUrl from stack output>
VITE_API_TOKEN=<same token as ApiToken parameter if used>
```

## Routes

- `GET /sites`
- `GET /sites/{site_id}`
- `POST /sites`
- `PUT /sites/{site_id}`
- `DELETE /sites/{site_id}`

## Example Payload

```json
{
  "site_id": "MIN823",
  "site_name": "GINGOO",
  "area_id": "AREA-2",
  "city": "GINGOOG CITY",
  "province": "MISAMIS ORIENTAL",
  "region": "MIN",
  "lat": 8.825,
  "lng": 125.1026,
  "is_active": true
}
```
