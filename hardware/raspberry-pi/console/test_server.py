import json
import sys
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from tempfile import TemporaryDirectory
from urllib.error import HTTPError
from urllib.request import urlopen

sys.path.insert(0, str(Path(__file__).parent))
from server import ConsoleHandler  # noqa: E402


class FakeBridge(BaseHTTPRequestHandler):
    agents_error = None

    def log_message(self, *_args):
        pass

    def do_GET(self):  # noqa: N802
        if self.path == "/api/v1/agents" and self.agents_error is not None:
            self.send_error(self.agents_error)
            return
        payloads = {
            "/api/v1/status": {"agent": "agent-b", "state": "working", "message": "Testing"},
            "/api/v1/agents": [
                {"agent": "agent-a", "state": "idle", "message": "Ready"},
                {"agent": "agent-b", "state": "working", "message": "Testing"},
            ],
            "/api/v1/attention": [{"id": "1", "agent": "agent-b", "reason": "user_input", "message": "Choose"}],
        }
        if self.path not in payloads:
            self.send_error(404)
            return
        body = json.dumps(payloads[self.path]).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class ConsoleServerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bridge = ThreadingHTTPServer(("127.0.0.1", 0), FakeBridge)
        ConsoleHandler.bridge_url = f"http://127.0.0.1:{cls.bridge.server_port}"
        cls.console = ThreadingHTTPServer(("127.0.0.1", 0), ConsoleHandler)
        cls.threads = [
            threading.Thread(target=cls.bridge.serve_forever, daemon=True),
            threading.Thread(target=cls.console.serve_forever, daemon=True),
        ]
        for thread in cls.threads:
            thread.start()
        cls.base = f"http://127.0.0.1:{cls.console.server_port}"

    @classmethod
    def tearDownClass(cls):
        cls.console.shutdown()
        cls.bridge.shutdown()

    def get_json(self, path):
        with urlopen(self.base + path) as response:
            return response.status, json.load(response)

    def test_health(self):
        self.assertEqual(self.get_json("/healthz"), (200, {"status": "ok"}))

    def test_status_proxy(self):
        status, payload = self.get_json("/api/console/status")
        self.assertEqual(status, 200)
        self.assertEqual(payload["agent"], "agent-b")
        self.assertEqual(payload["state"], "working")

    def test_agents_proxy(self):
        status, payload = self.get_json("/api/console/agents")
        self.assertEqual(status, 200)
        self.assertEqual([item["agent"] for item in payload], ["agent-a", "agent-b"])

    def test_agents_proxy_preserves_bridge_404_for_old_bridge_fallback(self):
        FakeBridge.agents_error = 404
        try:
            with self.assertRaises(HTTPError) as caught:
                urlopen(self.base + "/api/console/agents")
            self.assertEqual(caught.exception.code, 404)
            self.assertEqual(json.load(caught.exception)["status"], 404)
        finally:
            FakeBridge.agents_error = None

    def test_agents_proxy_maps_other_bridge_errors_to_bad_gateway(self):
        FakeBridge.agents_error = 503
        try:
            with self.assertRaises(HTTPError) as caught:
                urlopen(self.base + "/api/console/agents")
            self.assertEqual(caught.exception.code, 502)
            self.assertEqual(json.load(caught.exception)["status"], 503)
        finally:
            FakeBridge.agents_error = None

    def test_attention_proxy(self):
        status, payload = self.get_json("/api/console/attention")
        self.assertEqual(status, 200)
        self.assertEqual(payload[0]["reason"], "user_input")

    def test_host_status(self):
        status, payload = self.get_json("/api/console/host")
        self.assertEqual(status, 200)
        self.assertIn("hostname", payload)
        self.assertIn("memory", payload)
        self.assertIn("disk", payload)

    def test_static_ui(self):
        with urlopen(self.base + "/") as response:
            body = response.read().decode()
        self.assertIn("StateLamp Console", body)
        self.assertIn("WHO IS DOING WHAT?", body)
        self.assertIn("NEEDS YOU", body)
        self.assertIn("STATE CHANGES", body)

    def test_unknown_route_cannot_select_an_upstream_url(self):
        with self.assertRaises(HTTPError) as caught:
            urlopen(self.base + "/api/console/proxy?url=http://example.com")
        self.assertEqual(caught.exception.code, 404)


if __name__ == "__main__":
    unittest.main()
