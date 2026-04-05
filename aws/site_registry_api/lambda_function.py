import base64
import datetime
import json
import os
from decimal import Decimal
from typing import Any, Dict, Optional, Tuple

import boto3
from botocore.exceptions import ClientError

TABLE_NAME = os.environ["TABLE_NAME"]  # powereye-site-registry
CORS_ORIGIN = os.getenv("CORS_ORIGIN", "*")
API_TOKEN = os.getenv("API_TOKEN", "").strip()  # optional bearer token

ddb = boto3.resource("dynamodb")
table = ddb.Table(TABLE_NAME)


class DecimalEncoder(json.JSONEncoder):
    def default(self, obj: Any):
        if isinstance(obj, Decimal):
            if obj % 1 == 0:
                return int(obj)
            return float(obj)
        return super().default(obj)


def _response(status: int, body: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "statusCode": status,
        "headers": {
            "Content-Type": "application/json",
            "Access-Control-Allow-Origin": CORS_ORIGIN,
            "Access-Control-Allow-Headers": "Content-Type,Authorization",
            "Access-Control-Allow-Methods": "OPTIONS,GET,POST,PUT,DELETE",
        },
        "body": json.dumps(body, cls=DecimalEncoder),
    }


def _get_header(event: Dict[str, Any], name: str) -> str:
    headers = event.get("headers") or {}
    for k, v in headers.items():
        if k and k.lower() == name.lower():
            return str(v)
    return ""


def _method(event: Dict[str, Any]) -> str:
    return (
        event.get("requestContext", {}).get("http", {}).get("method")
        or event.get("httpMethod")
        or "GET"
    ).upper()


def _path(event: Dict[str, Any]) -> str:
    raw = (
        event.get("requestContext", {}).get("http", {}).get("path")
        or event.get("path")
        or "/"
    )
    return str(raw)


def _path_param_site_id(event: Dict[str, Any]) -> str:
    params = event.get("pathParameters") or {}
    return str(params.get("site_id", "")).strip()


def _parse_body(event: Dict[str, Any]) -> Dict[str, Any]:
    body = event.get("body")
    if body is None:
        return {}
    if event.get("isBase64Encoded"):
        body = base64.b64decode(body).decode("utf-8")
    data = json.loads(body)
    if not isinstance(data, dict):
        raise ValueError("Body must be a JSON object")
    return data


def _require_auth(event: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    if not API_TOKEN:
        return None
    auth = _get_header(event, "Authorization").strip()
    expected = f"Bearer {API_TOKEN}"
    if auth != expected:
        return _response(401, {"ok": False, "error": "Unauthorized"})
    return None


def _normalize_item(payload: Dict[str, Any], path_site_id: str = "") -> Dict[str, Any]:
    site_id = str(payload.get("site_id", path_site_id)).strip()
    if not site_id:
        raise ValueError("site_id is required")

    item = {
        "site_id": site_id,
        "site_name": str(payload.get("site_name", "")).strip(),
        "area_id": str(payload.get("area_id", "")).strip(),
        "area_name": str(payload.get("area_name", "")).strip(),
        "region": str(payload.get("region", "")).strip(),
        "country": str(payload.get("country", "")).strip(),
        "state": str(payload.get("state", "")).strip(),
        "province": str(payload.get("province", "")).strip(),
        "city": str(payload.get("city", "")).strip(),
        "is_active": bool(payload.get("is_active", True)),
        "updated_at": str(
            payload.get("updated_at", datetime.datetime.now(datetime.timezone.utc).isoformat())
        ),
    }

    if "lat" in payload and payload["lat"] not in (None, ""):
        item["lat"] = Decimal(str(payload["lat"]))
    if "lng" in payload and payload["lng"] not in (None, ""):
        item["lng"] = Decimal(str(payload["lng"]))
    if "criticality_weight" in payload and payload["criticality_weight"] not in (None, ""):
        item["criticality_weight"] = Decimal(str(payload["criticality_weight"]))

    return item


def _list_sites() -> Dict[str, Any]:
    items = []
    scan_kwargs: Dict[str, Any] = {}
    while True:
        resp = table.scan(**scan_kwargs)
        items.extend(resp.get("Items", []))
        lek = resp.get("LastEvaluatedKey")
        if not lek:
            break
        scan_kwargs["ExclusiveStartKey"] = lek
    items.sort(key=lambda x: str(x.get("site_id", "")))
    return _response(200, {"ok": True, "items": items, "count": len(items)})


def _get_site(site_id: str) -> Dict[str, Any]:
    if not site_id:
        return _response(400, {"ok": False, "error": "site_id is required"})
    resp = table.get_item(Key={"site_id": site_id})
    item = resp.get("Item")
    if not item:
        return _response(404, {"ok": False, "error": "Not found"})
    return _response(200, {"ok": True, "item": item})


def _create_site(payload: Dict[str, Any]) -> Dict[str, Any]:
    item = _normalize_item(payload)
    try:
        table.put_item(
            Item=item,
            ConditionExpression="attribute_not_exists(site_id)",
        )
    except ClientError as e:
        code = e.response.get("Error", {}).get("Code", "")
        if code == "ConditionalCheckFailedException":
            return _response(409, {"ok": False, "error": "site_id already exists"})
        raise
    return _response(201, {"ok": True, "item": item})


def _update_site(site_id: str, payload: Dict[str, Any]) -> Dict[str, Any]:
    if not site_id:
        return _response(400, {"ok": False, "error": "site_id is required"})
    item = _normalize_item(payload, path_site_id=site_id)
    item["site_id"] = site_id
    table.put_item(Item=item)
    return _response(200, {"ok": True, "item": item})


def _delete_site(site_id: str) -> Dict[str, Any]:
    if not site_id:
        return _response(400, {"ok": False, "error": "site_id is required"})
    table.delete_item(Key={"site_id": site_id})
    return _response(200, {"ok": True, "site_id": site_id})


def _route(event: Dict[str, Any]) -> Tuple[str, str, str]:
    method = _method(event)
    path = _path(event)
    site_id = _path_param_site_id(event)
    return method, path, site_id


def lambda_handler(event, context):
    try:
        method, path, site_id = _route(event)

        if method == "OPTIONS":
            return _response(200, {"ok": True})

        auth_error = _require_auth(event)
        if auth_error is not None:
            return auth_error

        # Route handling supports:
        # GET /sites
        # GET /sites/{site_id}
        # POST /sites
        # PUT /sites/{site_id}
        # DELETE /sites/{site_id}
        if method == "GET" and path.endswith("/sites") and not site_id:
            return _list_sites()
        if method == "GET" and site_id:
            return _get_site(site_id)
        if method == "POST" and path.endswith("/sites"):
            payload = _parse_body(event)
            return _create_site(payload)
        if method == "PUT" and site_id:
            payload = _parse_body(event)
            return _update_site(site_id, payload)
        if method == "DELETE" and site_id:
            return _delete_site(site_id)

        return _response(404, {"ok": False, "error": "Route not found"})

    except ValueError as e:
        return _response(400, {"ok": False, "error": str(e)})
    except ClientError as e:
        return _response(500, {"ok": False, "error": "AWS client error", "detail": str(e)})
    except Exception as e:
        return _response(500, {"ok": False, "error": "Internal error", "detail": repr(e)})
