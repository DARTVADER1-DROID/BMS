# BMS C backend

Native C implementation of the BMS backend. The frontend remains available and communicates with this API.

## Build

Install build-essential, cmake, pkg-config, libmicrohttpd-dev, libcjson-dev and libssl-dev.
Run: cmake -S backend-c -B build -DCMAKE_BUILD_TYPE=Release
Then: cmake --build build --parallel
Then: ctest --test-dir build --output-on-failure
Run: ./build/bms-server

Docker: docker build -t bms-c . && docker run --rm -p 8080:8080 bms-c

## Interactive API documentation

Start the backend, then open `http://localhost:8080/docs` (or replace the host and port with your deployment URL) to use Swagger UI. The OpenAPI 3.0 specification is served at `/openapi.json`.

Swagger UI supports **Try it out** for the implemented routes:

- Read-only endpoints: health, telemetry, state, hardware connection, SOC, SOH and RUL.
- Control endpoints: charge, discharge, stop and cycle-count update.
- Device endpoint: submit voltage, current and temperature using a JSON request body.

Human dashboard access uses `POST /auth/login`. The browser receives an HttpOnly, Secure, host-only session cookie and a separate CSRF cookie. `GET /auth/me` reports the signed-in account and `POST /auth/logout` clears the browser session. All dashboard data and control endpoints require a valid session; state-changing browser requests must include the CSRF token.

Swagger UI assets are loaded from jsDelivr, so the browser needs internet access to render the interactive interface. The OpenAPI document itself is served by the BMS backend.

## API compatibility

GET / and /health: liveness.
GET /data: voltage/current/temperature and telemetry timestamp.
GET /state: state and warning message.
GET /state-hardware: hardware-facing state.
GET /hardware_connection: heartbeat status.
GET /s_o_c: provisional voltage-interpolation SOC estimate.
GET /s_o_h: legacy empirical SOH estimate.
GET /r_u_l: rated-cycle subtraction estimate.
POST /charge, /discharge, /stop: control commands.
POST /cycles?count=N: set cycle count.
POST /update: submit telemetry JSON using the separate device bearer token.
POST /auth/login: authenticate with the configured dashboard account.
GET /auth/me: inspect the current login session.
POST /auth/logout: end the browser session.

Existing dashboard response field names are retained. Rejected state transitions now return meaningful HTTP error codes.

## Commissioning and safety

Control is disabled by default. The server accepts telemetry but /charge and /discharge return HTTP 503 until commissioned.

Set BMS_ENABLE_CONTROL=1 and explicitly configure BMS_VOLTAGE_MIN, BMS_VOLTAGE_MAX, BMS_CURRENT_MIN, BMS_CURRENT_MAX, BMS_TEMPERATURE_MIN and BMS_TEMPERATURE_MAX using actual battery and sensor specifications. SOC bounds are configurable with BMS_SOC_VOLTAGE_MIN and BMS_SOC_VOLTAGE_MAX. Generic defaults are not battery calibration.

Configure these required Render environment variables before starting the service:

- `BMS_AUTH_USERNAME`: the dashboard login name (letters, digits, dot, underscore and hyphen only).
- `BMS_AUTH_PASSWORD`: the dashboard password (at least 12 characters). It is PBKDF2-HMAC-SHA256 hashed with 600,000 iterations and a fresh random salt at startup; the plaintext is read only from the server environment.
- `BMS_SESSION_SECRET`: a persistent random secret of 32–64 characters. Generate one with `openssl rand -hex 32`. Keep it secret and stable; the backend combines it with a random process-start nonce to derive its active signing key.
- `BMS_DEVICE_TOKEN`: a separate, required bearer token for device telemetry ingestion. An unset token now prevents `POST /update`.

For local development, the Vue frontend uses Vite's `/api/*` proxy, configured by `VITE_API_PROXY_TARGET`. For production deployments that call the backend directly, set `VITE_API_BASE_URL` to the backend origin and configure `BMS_CORS_ORIGIN` to the exact frontend origin. Credentialed CORS must never use a wildcard origin.

This no-database mode supports one configured login account. Password changes and multiple accounts require persistent storage and are intentionally not implemented. Sessions expire after eight hours. The signing key is derived from `BMS_SESSION_SECRET` plus a fresh random nonce at process startup, so every backend restart invalidates all existing sessions even when the configured secret is unchanged. The current implementation is single-instance only: separate replicas derive different signing keys and maintain separate revocation registries. Do not horizontally scale this backend until signing and revocation state are moved to shared infrastructure. Logout revokes the session token in a bounded, in-memory registry (4,096 entries) for the lifetime of that process; it rejects replay on that instance, but the registry is not shared or persisted. If the registry has no free or expired slot, logout fails with HTTP 503 rather than claiming the session was revoked.

The safety worker checks telemetry freshness every 250 ms. A heartbeat older than 7 seconds marks hardware disconnected and returns the requested state to idle. Invalid/non-finite telemetry is rejected. Shared state is mutex-protected.

This is supervisory software, not a certified protection device. Independent local hardware protection must remain on the ESP32/controller and de-energize outputs on communication loss, watchdog expiry, sensor fault or unsafe measurements. Do not connect to a live battery until commissioning and hardware-in-the-loop/fault-injection testing are complete.

## Estimator status

The legacy empirical SOH curve is retained for API compatibility but is not validated for a particular chemistry or cell. SOC is simple voltage interpolation, not coulomb counting or a chemistry-compensated estimator. RUL is basic rated-cycle subtraction. These are provisional indicators, not safety inputs. A production estimator requires battery chemistry, cell configuration, capacity, sensor calibration and measured ageing data.

## Deployment

The root Dockerfile builds the native service and runs core tests in the build stage. Configure PORT, safety limits and the required authentication/device environment variables as deployment environment variables. The legacy Python API has been removed; deploy and maintain the native C service.

## Rapid battery diagnostic modules

The diagnostic code is split by responsibility so each part can be read and tested independently:

- `include/diagnostics/diagnostic.h`, `src/diagnostics/diagnostic.c`: test sequence, sample validation, and abort conditions.
- `include/diagnostics/battery_types.h`: shared, plain-data structures.
- `include/diagnostics/battery_features.h`, `src/diagnostics/battery_features.c`: pure calculations from a completed pulse test.
- `include/diagnostics/battery_health.h`, `src/diagnostics/battery_health.c`: calibrated SOH regression and historical RUL trend.

The flow is: initialize diagnostic -> begin with a stable resting measurement -> feed timestamped samples -> finish after load removal and recovery -> calculate features -> estimate SOH/RUL if calibration/history exists.

The estimator does **not** infer capacity SOH directly from a short pulse. SOH is withheld until a model is calibrated against independent capacity measurements from representative HONGLI cells. RUL is withheld until at least three chronological SOH observations paired with equivalent full-cycle counts are available. Linear RUL extrapolation is provisional, not a safety function.

Default diagnostic thresholds are software placeholders, not manufacturer-approved cell limits. The 1 A pulse ceiling is a conservative test cap, not the cell's rated discharge current. Confirm cell datasheet, protection, wiring, load, sensor calibration and thermal installation before testing. The legacy ADC conversions are not valid ACS712/voltage-divider/NTC calibration. The 10 kOhm B3950 NTC remains a planned replacement until installation is confirmed. These modules are not yet connected to relay control or the HTTP API.

Use a faster timestamped acquisition rate than the legacy 300 ms sensor loop for resistance-step capture. A safe experiment requires a controlled constant-current load, prompt first loaded sample, multiple unloaded recovery samples and hardware-level overcurrent/overtemperature protection.


## Code formatting

The repository includes one formatter entry point for native firmware/backend code and frontend assets.

Requirements:
- `clang-format` for C, C++ and Arduino `.ino` files.
- Node.js/npm for the pinned Prettier 3.3.3 frontend formatter.

Run from the repository root:

```sh
bash scripts/format.sh
bash scripts/format.sh --check
```

The default mode formats files in place. The check mode reports formatting drift without changing files. The formatter settings live in `.clang-format` and `.prettierrc.json`.

## Automated tests

Configure and run the complete native test suite:

```sh
cmake -S backend-c -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The suite contains:
- `bms_core_tests`: BMS state transitions, telemetry validation, safety and heartbeat behavior.
- `battery_diagnostics_tests`: pulse diagnostic flow, estimator availability and unsafe sample handling.
- `authentication_tests`: password verification and session/CSRF token creation.
- `bms_edge_case_tests`: null arguments, non-finite values, invalid sample timing, excessive current, invalid calibration and malformed ageing history.
- `http_integration_tests`: starts the actual C server with disposable credentials and tests health, OpenAPI, login, session access, device-token rejection, telemetry ingestion, invalid payloads, CSRF enforcement, disabled controls, unknown routes and HTTP methods.

The HTTP integration tests use only Python's standard library. They run as part of CTest and do not require a separately running backend.


## Battery domain entity

The initial `Battery` entity is implemented in:

- `include/domain/entities/battery.h`: the UML-aligned data structure and public operations.
- `src/domain/entities/battery.c`: initialization, validation and field updates.
- `tests/unit/domain/test_domain_models.c`: Battery entity unit tests.

The implementation retains the five UML attributes: `battery_id`, `fleet_id`, `name`, `status` and `configuration_json`. UUIDs must use the canonical 36-character hexadecimal-and-hyphen representation. Name and status must be non-empty (whitespace-only values are rejected). Configuration must be a complete valid JSON object; an empty object (`{}`) is accepted.

The current status field intentionally remains a non-empty string because the permitted lifecycle values have not yet been finalized. The entity does not check whether a fleet exists or whether its owner matches the authenticated user; those checks belong to the application/repository boundary. It also does not own the memory referenced by its string fields: callers must keep those strings alive while the entity uses them. Configuration updates replace the complete JSON document rather than merging it.

The domain target links to the existing cJSON dependency for configuration validation. The Battery entity is not yet wired into HTTP routes or persistence.

## Backend module boundaries

The native backend keeps battery-domain logic independent of HTTP and authentication:

- `src/core/bms.c` / `include/core/bms.h`: BMS state, telemetry, interlocks and estimates.
- `src/diagnostics/diagnostic.c`: diagnostic state machine and sample safety checks.
- `src/diagnostics/battery_features.c`: pure feature calculations from completed tests.
- `src/diagnostics/battery_health.c`: calibrated SOH and history-based RUL calculations.
- `src/security/auth.c` / `include/security/auth.h`: login throttling, password verification, sessions and CSRF.
- `src/http/api_docs.c` / `include/http/api_docs.h`: OpenAPI document and Swagger UI page.
- `src/http/server.c`: HTTP transport, route dispatch, request lifecycle and process startup.

Keep domain modules free of HTTP types. Keep API documentation out of request-routing code. New battery behavior belongs in the domain modules with unit tests; endpoint behavior belongs in the HTTP integration tests.


## HTTP server module boundaries

HTTP transport and API behavior are separated into small modules:

- `src/http/server.c`: process startup, request-body lifecycle, HTTP method dispatch, shutdown and safety-monitor thread.
- `src/http/http_response.c` / `include/http/http_response.h`: JSON/HTML response creation, common security headers, bearer-token parsing and monotonic timestamps.
- `src/http/auth_routes.c` / `include/http/auth_routes.h`: login, session inspection and logout.
- `src/http/telemetry_routes.c` / `include/http/telemetry_routes.h`: device-token validation and telemetry ingestion.
- `src/http/control_routes.c` / `include/http/control_routes.h`: charging/discharging/stop commands and cycle-count updates.
- `src/http/read_routes.c` / `include/http/read_routes.h`: health, telemetry, state, hardware connection and estimate read endpoints.
- `include/http/server_context.h`: explicit dependencies shared with route modules; avoids hidden route-level globals.
- `src/http/api_docs.c`: OpenAPI and Swagger UI assets.

Route modules receive a read-only server context pointer. They do not own process startup, sockets, request buffers or global server lifecycle. Keep authentication policy in the dispatcher and authentication module, and keep endpoint-specific behavior inside the corresponding route module.

## Backend directory layout

```text
backend-c/
├── include/
│   ├── core/             # BMS state and public core API
│   ├── diagnostics/      # Diagnostic data, algorithms and estimators
│   ├── domain/
│   │   ├── entities/     # Battery and fleet entities
│   │   └── value_objects/
│   ├── http/             # HTTP server, routes, response and API docs
│   └── security/         # Authentication and session API
├── src/
│   ├── core/
│   ├── diagnostics/
│   ├── http/
│   └── security/
└── tests/
    ├── unit/
    │   ├── core/
    │   ├── diagnostics/
    │   ├── domain/
    │   └── security/
    └── integration/
```

Keep new files in the folder matching their responsibility. The HTTP layer may depend on core and security interfaces; core and diagnostics must not depend on HTTP transport. Keep unit tests beside their logical test category and end-to-end HTTP tests under `tests/integration`.
