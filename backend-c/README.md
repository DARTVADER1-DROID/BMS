# BMS C backend

Native C replacement for the original FastAPI service. The Python implementation and frontend remain available for comparison and rollback during validation.

## Build

Install build-essential, cmake, pkg-config, libmicrohttpd-dev and libcjson-dev.
Run: cmake -S backend-c -B build -DCMAKE_BUILD_TYPE=Release
Then: cmake --build build --parallel
Then: ctest --test-dir build --output-on-failure
Run: ./build/bms-server

Docker: docker build -t bms-c . && docker run --rm -p 8080:8080 bms-c

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
POST /update: submit telemetry JSON.

Existing dashboard response field names are retained. Rejected state transitions now return meaningful HTTP error codes.

## Commissioning and safety

Control is disabled by default. The server accepts telemetry but /charge and /discharge return HTTP 503 until commissioned.

Set BMS_ENABLE_CONTROL=1 and explicitly configure BMS_VOLTAGE_MIN, BMS_VOLTAGE_MAX, BMS_CURRENT_MIN, BMS_CURRENT_MAX, BMS_TEMPERATURE_MIN and BMS_TEMPERATURE_MAX using actual battery and sensor specifications. SOC bounds are configurable with BMS_SOC_VOLTAGE_MIN and BMS_SOC_VOLTAGE_MAX. Generic defaults are not battery calibration.

If BMS_API_TOKEN is set, control and cycle-update requests require Authorization: Bearer token. If BMS_DEVICE_TOKEN is set, telemetry updates require the device token. Unset tokens preserve old unauthenticated API compatibility; do not expose that configuration publicly. Set BMS_CORS_ORIGIN to the exact dashboard origin in production.

The safety worker checks telemetry freshness every 250 ms. A heartbeat older than 7 seconds marks hardware disconnected and returns the requested state to idle. Invalid/non-finite telemetry is rejected. Shared state is mutex-protected.

This is supervisory software, not a certified protection device. Independent local hardware protection must remain on the ESP32/controller and de-energize outputs on communication loss, watchdog expiry, sensor fault or unsafe measurements. Do not connect to a live battery until commissioning and hardware-in-the-loop/fault-injection testing are complete.

## Estimator status

The legacy empirical SOH curve is retained for API compatibility but is not validated for a particular chemistry or cell. SOC is simple voltage interpolation, not coulomb counting or a chemistry-compensated estimator. RUL is basic rated-cycle subtraction. These are provisional indicators, not safety inputs. A production estimator requires battery chemistry, cell configuration, capacity, sensor calibration and measured ageing data.

## Deployment

The root Dockerfile builds the native service and runs core tests in the build stage. Configure PORT, safety limits, and tokens as deployment environment variables. Keep the Python backend available until API compatibility and hardware-in-the-loop acceptance tests pass.
