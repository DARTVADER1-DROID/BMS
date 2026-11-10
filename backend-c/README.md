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

## Rapid Battery Health Estimator (RBHE)

A standalone, fixed-memory C diagnostic module is available in `include/rbhe.h` and `src/rbhe.c`. It consumes timestamped voltage/current/temperature samples from a controlled discharge pulse followed by an unloaded recovery period. It extracts:
- step resistance (baseline-to-first-loaded-sample voltage change / mean discharge current),
- steady pulse resistance (baseline-to-mean-loaded-voltage / mean discharge current),
- loaded voltage slope,
- voltage recovery fraction and recovery elapsed time,
- temperature rise and temperature slope.

The estimator does **not** claim that these pulse features directly measure capacity SOH. Capacity SOH is withheld unless a calibration object is supplied. Calibration coefficients must be trained and independently validated against cells of the same model/chemistry using capacity measurements from controlled reference tests. The output includes the calibration's validation error as a confidence indicator; it is not a statistical confidence interval.

RUL is withheld until at least three chronological SOH observations paired with equivalent full-cycle counts are available. A least-squares degradation slope is then extrapolated to an 80% SOH end-of-life threshold by the convenience result path. The lower-level RUL function accepts an explicit threshold. This linear extrapolation is only a provisional forecast and must not be used for safety decisions.

RBHE uses fixed-size buffers (no heap allocation). Default diagnostic limits are conservative software placeholders, **not manufacturer-approved charge/discharge limits**. Confirm the exact cell datasheet and hardware protection thresholds before testing. The current 1S HONGLI cell's maximum discharge current is not yet known; the default pulse ceiling is 1 A only as a conservative diagnostic design cap, not a verified cell rating. Do not run a pulse unless the actual load, wiring, sensor range, cell condition and protection hardware have been checked.

The current firmware's generic ADC formulas are not ACS712/voltage-divider/NTC calibration. In particular, the ACS712-20A's nominal sensitivity is 100 mV/A, but its output offset, supply, any output divider, ESP32 ADC calibration and zero-current drift must be measured. The selected 10 kOhm B3950 NTC is a planned replacement until its installation is confirmed. The estimator is a standalone library at this stage; it is not yet exposed through the HTTP API or connected to relay control.

Sampling should be faster than the legacy ~300 ms acquisition interval for reliable step-resistance identification. Use timestamped samples, a stable rest baseline, a controlled constant-current load, and several unloaded recovery samples. The initial loaded sample must be captured promptly after load application. Tests must be run on a protected test setup, not unattended.
