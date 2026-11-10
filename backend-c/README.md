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

## Rapid battery diagnostic modules

The diagnostic code is split by responsibility so each part can be read and tested independently:

- `include/diagnostic.h`, `src/diagnostic.c`: test sequence, sample validation, and abort conditions.
- `include/battery_types.h`: shared, plain-data structures.
- `include/battery_features.h`, `src/battery_features.c`: pure calculations from a completed pulse test.
- `include/battery_health.h`, `src/battery_health.c`: calibrated SOH regression and historical RUL trend.

The flow is: initialize diagnostic -> begin with a stable resting measurement -> feed timestamped samples -> finish after load removal and recovery -> calculate features -> estimate SOH/RUL if calibration/history exists.

The estimator does **not** infer capacity SOH directly from a short pulse. SOH is withheld until a model is calibrated against independent capacity measurements from representative HONGLI cells. RUL is withheld until at least three chronological SOH observations paired with equivalent full-cycle counts are available. Linear RUL extrapolation is provisional, not a safety function.

Default diagnostic thresholds are software placeholders, not manufacturer-approved cell limits. The 1 A pulse ceiling is a conservative test cap, not the cell's rated discharge current. Confirm cell datasheet, protection, wiring, load, sensor calibration and thermal installation before testing. The legacy ADC conversions are not valid ACS712/voltage-divider/NTC calibration. The 10 kOhm B3950 NTC remains a planned replacement until installation is confirmed. These modules are not yet connected to relay control or the HTTP API.

Use a faster timestamped acquisition rate than the legacy 300 ms sensor loop for resistance-step capture. A safe experiment requires a controlled constant-current load, prompt first loaded sample, multiple unloaded recovery samples and hardware-level overcurrent/overtemperature protection.
