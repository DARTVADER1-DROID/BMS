cmake_minimum_required(VERSION 3.16)
project(bms_backend C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(MICROHTTPD REQUIRED IMPORTED_TARGET libmicrohttpd)
pkg_check_modules(CJSON REQUIRED IMPORTED_TARGET libcjson)

add_library(
    bms_core STATIC
    src/core/bms.c
    src/diagnostics/diagnostic.c
    src/diagnostics/battery_features.c
    src/diagnostics/battery_health.c
)

target_include_directories(bms_core PUBLIC include)
target_link_libraries(bms_core PUBLIC Threads::Threads m)
target_compile_options(
    bms_core PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow
)

add_executable(
    bms-server
    src/http/server.c
    src/security/auth.c
    src/http/api_docs.c
    src/http/http_response.c
    src/http/auth_routes.c
    src/http/telemetry_routes.c
    src/http/control_routes.c
    src/http/read_routes.c
)
target_link_libraries(
    bms-server PRIVATE
    bms_core
    PkgConfig::MICROHTTPD
    PkgConfig::CJSON
    OpenSSL::Crypto
)
target_compile_options(
    bms-server PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow
)

include(CTest)

add_library(
    bms_domain STATIC
    src/domain/entities/battery.c
    src/domain/entities/battery_fleet.c
)
target_include_directories(bms_domain PUBLIC include)
target_link_libraries(bms_domain PUBLIC PkgConfig::CJSON)
target_compile_options(bms_domain PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

add_library(
    bms_application STATIC
    src/application/battery/battery_repository.c
    src/application/battery/battery_service.c
    src/application/fleet/fleet_repository.c
    src/application/fleet/fleet_service.c
    src/application/telemetry/telemetry_repository.c
    src/application/telemetry/telemetry_service.c
)
target_include_directories(bms_application PUBLIC include)
target_link_libraries(bms_application PUBLIC bms_domain)
target_compile_options(
    bms_application PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow
)

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    add_executable(test_bms tests/unit/core/test_bms.c)
    target_link_libraries(test_bms PRIVATE bms_core)
    add_test(NAME bms_core_tests COMMAND test_bms)

    add_executable(test_battery_diagnostics tests/unit/diagnostics/test_battery_diagnostics.c)
    target_link_libraries(test_battery_diagnostics PRIVATE bms_core)
    add_test(NAME battery_diagnostics_tests COMMAND test_battery_diagnostics)

    add_executable(test_domain_models tests/unit/domain/test_domain_models.c)
    target_link_libraries(test_domain_models PRIVATE bms_domain)
    target_compile_options(test_domain_models PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME battery_domain_tests COMMAND test_domain_models)

    add_executable(test_battery_fleet tests/unit/domain/test_battery_fleet.c)
    target_link_libraries(test_battery_fleet PRIVATE bms_domain)
    target_compile_options(test_battery_fleet PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME battery_fleet_tests COMMAND test_battery_fleet)

    add_executable(
        test_battery_fleet_integration
        tests/integration/test_battery_fleet_integration.c
    )
    target_link_libraries(test_battery_fleet_integration PRIVATE bms_domain)
    target_compile_options(
        test_battery_fleet_integration PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    add_test(
        NAME battery_fleet_domain_integration_tests
        COMMAND test_battery_fleet_integration
    )

    add_executable(test_battery_repository tests/unit/application/test_battery_repository.c)
    target_link_libraries(test_battery_repository PRIVATE bms_application)
    target_compile_options(
        test_battery_repository PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    add_test(NAME battery_repository_tests COMMAND test_battery_repository)

    add_executable(test_battery_service tests/unit/application/test_battery_service.c)
    target_link_libraries(test_battery_service PRIVATE bms_application)
    target_compile_options(
        test_battery_service PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    add_test(NAME battery_service_tests COMMAND test_battery_service)

    add_executable(
        test_battery_service_adversarial
        tests/unit/application/test_battery_service_adversarial.c
    )
    target_link_libraries(test_battery_service_adversarial PRIVATE bms_application)
    target_compile_options(
        test_battery_service_adversarial PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    add_test(
        NAME battery_service_adversarial_tests
        COMMAND test_battery_service_adversarial
    )

    add_executable(test_fleet_service tests/unit/application/test_fleet_service.c)
    target_link_libraries(test_fleet_service PRIVATE bms_application)
    target_compile_options(
        test_fleet_service PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    add_test(NAME fleet_service_tests COMMAND test_fleet_service)

    add_executable(test_auth tests/unit/security/test_auth.c src/security/auth.c)
    target_link_libraries(
        test_auth PRIVATE
        PkgConfig::MICROHTTPD
        OpenSSL::Crypto
        Threads::Threads
    )
    target_include_directories(test_auth PRIVATE include)
    target_compile_options(
        test_auth PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    add_test(NAME authentication_tests COMMAND test_auth)

    add_executable(test_edge_cases tests/unit/cross_module/test_edge_cases.c)
    target_link_libraries(test_edge_cases PRIVATE bms_core)
    target_compile_options(
        test_edge_cases PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    add_test(NAME bms_edge_case_tests COMMAND test_edge_cases)

    add_test(
        NAME http_integration_tests
        COMMAND ${CMAKE_COMMAND} -E env
            BMS_SERVER_BINARY=$<TARGET_FILE:bms-server>
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_http_integration.py
    )
endif()
