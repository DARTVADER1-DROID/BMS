#!/usr/bin/env python3
"""Black-box HTTP integration tests for the BMS C backend.

The test starts a real server process with disposable credentials and exercises
its public HTTP interface. No third-party Python packages are required.
"""

import json
import os
import socket
import subprocess
import time
import unittest
import urllib.error
import urllib.request
from pathlib import Path


USERNAME = "integration_user"
PASSWORD = "integration_test_password_123"
DEVICE_TOKEN = "integration_device_token"
SESSION_SECRET = "integration_session_secret_12345678901234567890"


def reserve_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


class BmsHttpIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        binary = os.environ.get("BMS_SERVER_BINARY")
        if not binary:
            raise RuntimeError("BMS_SERVER_BINARY must point to the built bms-server executable")

        cls.port = reserve_port()
        cls.base_url = f"http://127.0.0.1:{cls.port}"
        environment = os.environ.copy()
        environment.update(
            {
                "PORT": str(cls.port),
                "BMS_AUTH_USERNAME": USERNAME,
                "BMS_AUTH_PASSWORD": PASSWORD,
                "BMS_SESSION_SECRET": SESSION_SECRET,
                "BMS_DEVICE_TOKEN": DEVICE_TOKEN,
                "BMS_VOLTAGE_MIN": "0",
                "BMS_VOLTAGE_MAX": "5",
                "BMS_CURRENT_MIN": "-2",
                "BMS_CURRENT_MAX": "2",
                "BMS_TEMPERATURE_MIN": "-40",
                "BMS_TEMPERATURE_MAX": "85",
                "BMS_ENABLE_CONTROL": "0",
            }
        )

        cls.log_file = open(Path(os.devnull), "w", encoding="utf-8")
        cls.server = subprocess.Popen(
            [binary],
            env=environment,
            stdout=cls.log_file,
            stderr=subprocess.STDOUT,
        )

        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if cls.server.poll() is not None:
                raise RuntimeError(f"BMS server exited during startup: {cls.server.returncode}")
            try:
                status, body, _ = cls.request("GET", "/health")
                if status == 200 and body.get("implementation") == "c":
                    break
            except (OSError, urllib.error.URLError, ValueError):
                time.sleep(0.2)
        else:
            cls.tearDownClass()
            raise RuntimeError("BMS server did not become healthy within 15 seconds")

        status, login, headers = cls.request(
            "POST",
            "/auth/login",
            {"username": USERNAME, "password": PASSWORD},
        )
        if status != 200:
            cls.tearDownClass()
            raise RuntimeError(f"Integration login failed: HTTP {status}")

        session_cookie = None
        for cookie in headers.get_all("Set-Cookie", []):
            if cookie.startswith("__Host-bms_session="):
                session_cookie = cookie.split(";", 1)[0]
                break

        if not session_cookie or len(login.get("csrf_token", "")) != 64:
            cls.tearDownClass()
            raise RuntimeError("Login response did not contain expected session and CSRF tokens")

        cls.csrf_token = login["csrf_token"]
        cls.cookie_header = f"{session_cookie}; __Host-bms_csrf={cls.csrf_token}"

    @classmethod
    def tearDownClass(cls):
        server = getattr(cls, "server", None)
        if server is not None and server.poll() is None:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=5)

        log_file = getattr(cls, "log_file", None)
        if log_file is not None:
            log_file.close()

    @classmethod
    def request(cls, method, path, payload=None, headers=None):
        request_headers = dict(headers or {})
        data = None
        if payload is not None:
            data = json.dumps(payload).encode("utf-8")
            request_headers["Content-Type"] = "application/json"

        request = urllib.request.Request(
            cls.base_url + path,
            data=data,
            headers=request_headers,
            method=method,
        )

        try:
            with urllib.request.urlopen(request, timeout=3) as response:
                body = response.read().decode("utf-8")
                parsed = json.loads(body) if body else {}
                return response.status, parsed, response.headers
        except urllib.error.HTTPError as error:
            body = error.read().decode("utf-8")
            error.close()
            try:
                parsed = json.loads(body) if body else {}
            except json.JSONDecodeError:
                parsed = {"raw": body}
            return error.code, parsed, error.headers

    def dashboard_headers(self):
        return {"Cookie": self.cookie_header}

    def test_health_and_openapi_are_public(self):
        status, health, _ = self.request("GET", "/health")
        self.assertEqual(status, 200)
        self.assertEqual(health["implementation"], "c")

        status, spec, _ = self.request("GET", "/openapi.json")
        self.assertEqual(status, 200)
        self.assertEqual(spec["openapi"], "3.0.3")

    def test_dashboard_data_requires_authentication(self):
        status, body, _ = self.request("GET", "/data")
        self.assertEqual(status, 401)
        self.assertEqual(body["error"], "unauthenticated")

    def test_bad_and_missing_credentials_are_rejected(self):
        status, _, _ = self.request(
            "POST",
            "/auth/login",
            {"username": USERNAME, "password": "wrong-password"},
        )
        self.assertIn(status, (401, 429))

        status, _, _ = self.request("POST", "/auth/login", {"username": USERNAME})
        self.assertIn(status, (400, 429))

    def test_authenticated_session_reads_dashboard(self):
        status, body, _ = self.request("GET", "/auth/me", headers=self.dashboard_headers())
        self.assertEqual(status, 200)
        self.assertEqual(body.get("username"), USERNAME)

        status, _, _ = self.request("GET", "/data", headers=self.dashboard_headers())
        self.assertEqual(status, 200)

    def test_tampered_session_cookie_is_rejected(self):
        session_name, session_value = self.cookie_header.split(";", 1)[0].split("=", 1)
        replacement = "A" if session_value[0] != "A" else "B"
        tampered_cookie = f"{session_name}={replacement}{session_value[1:]}"

        status, _, _ = self.request(
            "GET",
            "/data",
            headers={"Cookie": tampered_cookie},
        )
        self.assertEqual(status, 401)

    def test_device_endpoint_rejects_missing_and_invalid_tokens(self):
        payload = {"voltage": 3.7, "current": 0.1, "temperature": 25, "time": 1}
        status, _, _ = self.request("POST", "/update", payload)
        self.assertEqual(status, 401)

        status, _, _ = self.request(
            "POST",
            "/update",
            payload,
            {"Authorization": "Bearer definitely-wrong"},
        )
        self.assertEqual(status, 401)

    def test_valid_device_update_is_visible_to_dashboard(self):
        payload = {
            "voltage": 3.71,
            "current": 0.12,
            "temperature": 25.5,
            "time": 1,
            "hardware_connection": True,
        }
        status, _, _ = self.request(
            "POST",
            "/update",
            payload,
            {"Authorization": f"Bearer {DEVICE_TOKEN}"},
        )
        self.assertEqual(status, 200)

        status, data, _ = self.request("GET", "/data", headers=self.dashboard_headers())
        self.assertEqual(status, 200)
        self.assertAlmostEqual(data["voltage"], 3.71, places=3)
        self.assertAlmostEqual(data["current"], 0.12, places=3)

    def test_malformed_and_out_of_range_telemetry_are_rejected(self):
        headers = {"Authorization": f"Bearer {DEVICE_TOKEN}"}
        status, _, _ = self.request("POST", "/update", {"voltage": "not-a-number"}, headers)
        self.assertEqual(status, 400)

        status, _, _ = self.request(
            "POST",
            "/update",
            {"voltage": 10000000, "current": 0.1, "temperature": 25},
            headers,
        )
        self.assertEqual(status, 400)

    def test_control_stays_disabled_without_commissioning(self):
        status, _, _ = self.request(
            "POST",
            "/charge",
            headers={
                **self.dashboard_headers(),
                "X-CSRF-Token": self.csrf_token,
            },
        )
        self.assertEqual(status, 503)

    def test_state_changing_request_requires_valid_csrf(self):
        status, _, _ = self.request(
            "POST",
            "/charge",
            headers=self.dashboard_headers(),
        )
        self.assertEqual(status, 403)

        status, _, _ = self.request(
            "POST",
            "/charge",
            headers={
                **self.dashboard_headers(),
                "X-CSRF-Token": "invalid-csrf-token",
            },
        )
        self.assertEqual(status, 403)

    def test_all_authenticated_read_routes_return_valid_json(self):
        for path in ("/state", "/state-hardware", "/hardware_connection", "/s_o_c", "/s_o_h", "/r_u_l"):
            with self.subTest(path=path):
                status, body, _ = self.request("GET", path, headers=self.dashboard_headers())
                self.assertEqual(status, 200)
                self.assertIsInstance(body, dict)
                self.assertGreater(len(body), 0)

    def test_authenticated_state_and_hardware_shapes(self):
        status, state, _ = self.request("GET", "/state", headers=self.dashboard_headers())
        self.assertEqual(status, 200)
        self.assertIn(state.get("state"), ("idle", "charging", "discharging"))
        self.assertIn("warning_messages", state)

        status, hardware, _ = self.request(
            "GET", "/hardware_connection", headers=self.dashboard_headers()
        )
        self.assertEqual(status, 200)
        self.assertIsInstance(hardware.get("hardware_connection"), bool)

    def test_telemetry_requires_all_numeric_fields(self):
        headers = {"Authorization": f"Bearer {DEVICE_TOKEN}"}
        invalid_payloads = (
            {},
            {"voltage": 3.7, "current": 0.1},
            {"voltage": "3.7", "current": 0.1, "temperature": 25},
            {"voltage": 3.7, "current": None, "temperature": 25},
            {"voltage": 3.7, "current": 0.1, "temperature": "warm"},
        )
        for payload in invalid_payloads:
            with self.subTest(payload=payload):
                status, body, _ = self.request("POST", "/update", payload, headers)
                self.assertEqual(status, 400)
                self.assertEqual(body.get("error"), "invalid_telemetry")

    def test_telemetry_rejects_each_out_of_range_sensor(self):
        headers = {"Authorization": f"Bearer {DEVICE_TOKEN}"}
        invalid_payloads = (
            {"voltage": 5.1, "current": 0.0, "temperature": 25.0},
            {"voltage": 3.7, "current": 2.1, "temperature": 25.0},
            {"voltage": 3.7, "current": 0.0, "temperature": 85.1},
        )
        for payload in invalid_payloads:
            with self.subTest(payload=payload):
                status, body, _ = self.request("POST", "/update", payload, headers)
                self.assertEqual(status, 400)
                self.assertEqual(body.get("error"), "telemetry_out_of_range")

    def test_cycle_count_rejects_missing_malformed_and_overflow_values(self):
        headers = {
            **self.dashboard_headers(),
            "X-CSRF-Token": self.csrf_token,
        }
        for path in ("/cycles", "/cycles?count=", "/cycles?count=-1",
                     "/cycles?count=not-a-number", "/cycles?count=1000000001",
                     "/cycles?count=999999999999999999999999"):
            with self.subTest(path=path):
                status, _, _ = self.request("POST", path, headers=headers)
                self.assertEqual(status, 400)

    def test_cycle_count_accepts_documented_upper_boundary(self):
        headers = {
            **self.dashboard_headers(),
            "X-CSRF-Token": self.csrf_token,
        }
        status, body, _ = self.request("POST", "/cycles?count=1000000000", headers=headers)
        self.assertEqual(status, 200)
        self.assertEqual(body.get("message"), "Cycle count updated")

    def test_logout_requires_csrf_and_clears_session(self):
        status, _, _ = self.request(
            "POST", "/auth/logout", headers=self.dashboard_headers()
        )
        self.assertEqual(status, 403)

        status, _, headers = self.request(
            "POST",
            "/auth/logout",
            headers={**self.dashboard_headers(), "X-CSRF-Token": self.csrf_token},
        )
        self.assertEqual(status, 200)
        self.assertFalse(headers.get_all("Set-Cookie") is None)

        status, _, _ = self.request("GET", "/data", headers=self.dashboard_headers())
        self.assertEqual(status, 401)

    def test_request_body_limit_is_enforced(self):
        status, _, _ = self.request(
            "POST",
            "/update",
            {"padding": "x" * 9000},
            {"Authorization": f"Bearer {DEVICE_TOKEN}"},
        )
        self.assertEqual(status, 413)

    def test_unknown_route_and_wrong_method_are_rejected(self):
        status, _, _ = self.request("GET", "/this-route-does-not-exist", headers=self.dashboard_headers())
        self.assertEqual(status, 404)

        status, _, _ = self.request("PUT", "/data", headers=self.dashboard_headers())
        self.assertEqual(status, 405)


if __name__ == "__main__":
    unittest.main(verbosity=2)
