#!/usr/bin/env python3
"""StateLamp Linux Console: static UI and same-origin read-only API proxy."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import time
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parent
STATIC = ROOT / "static"
STARTED = time.monotonic()


def read_text(path: Path) -> str | None:
    try:
        return path.read_text().strip()
    except (OSError, UnicodeError):
        return None


def cpu_temperature() -> float | None:
    value = read_text(Path("/sys/class/thermal/thermal_zone0/temp"))
    try:
        return round(int(value) / 1000, 1) if value is not None else None
    except ValueError:
        return None


def memory_status() -> dict[str, int | None]:
    values: dict[str, int] = {}
    text = read_text(Path("/proc/meminfo")) or ""
    for line in text.splitlines():
        key, _, raw = line.partition(":")
        try:
            values[key] = int(raw.strip().split()[0]) * 1024
        except (ValueError, IndexError):
            continue
    total = values.get("MemTotal")
    available = values.get("MemAvailable")
    used = total - available if total is not None and available is not None else None
    return {"total_bytes": total, "used_bytes": used, "available_bytes": available}


def host_status() -> dict[str, Any]:
    disk = shutil.disk_usage("/")
    try:
        load = list(os.getloadavg())
    except OSError:
        load = []
    return {
        "hostname": socket.gethostname(),
        "cpu_temperature_c": cpu_temperature(),
        "load_average": load,
        "memory": memory_status(),
        "disk": {
            "total_bytes": disk.total,
            "used_bytes": disk.used,
            "free_bytes": disk.free,
        },
        "console_uptime_seconds": round(time.monotonic() - STARTED),
        "timestamp": int(time.time()),
    }


class ConsoleHandler(SimpleHTTPRequestHandler):
    bridge_url = "http://127.0.0.1:18480"

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, directory=str(STATIC), **kwargs)

    def log_message(self, fmt: str, *args: Any) -> None:
        print(f"{self.address_string()} - {fmt % args}")

    def send_json(self, status: HTTPStatus, payload: Any) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def proxy_json(self, path: str) -> None:
        target = f"{self.bridge_url.rstrip('/')}{path}"
        try:
            request = Request(target, headers={"Accept": "application/json"})
            with urlopen(request, timeout=2.0) as response:
                payload = json.load(response)
            self.send_json(HTTPStatus.OK, payload)
        except HTTPError as exc:
            status = HTTPStatus.NOT_FOUND if exc.code == HTTPStatus.NOT_FOUND else HTTPStatus.BAD_GATEWAY
            self.send_json(
                status,
                {"error": "bridge_http_error", "status": exc.code},
            )
        except (URLError, TimeoutError, OSError, ValueError) as exc:
            self.send_json(
                HTTPStatus.SERVICE_UNAVAILABLE,
                {"error": "bridge_unavailable", "detail": str(exc)},
            )

    def do_GET(self) -> None:  # noqa: N802 - stdlib handler API
        route = self.path.partition("?")[0]
        if route == "/api/console/status":
            self.proxy_json("/api/v1/status")
            return
        if route == "/api/console/agents":
            self.proxy_json("/api/v1/agents")
            return
        if route == "/api/console/attention":
            self.proxy_json("/api/v1/attention")
            return
        if route == "/api/console/host":
            self.send_json(HTTPStatus.OK, host_status())
            return
        if route == "/healthz":
            self.send_json(HTTPStatus.OK, {"status": "ok"})
            return
        super().do_GET()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=18880)
    parser.add_argument(
        "--bridge-url",
        default=os.environ.get("STATELAMP_BRIDGE_URL", "http://127.0.0.1:18480"),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    ConsoleHandler.bridge_url = args.bridge_url
    server = ThreadingHTTPServer((args.host, args.port), ConsoleHandler)
    print(f"StateLamp Console: http://{args.host}:{args.port}")
    print(f"Bridge: {args.bridge_url}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
