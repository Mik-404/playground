set(TARGET_NAME "00-aplusb")
set(COVERAGE_NAME "00-coverage")
set(COVERAGE_TARGET "00-coverage-aplusb")
set(SOURCE_DIR_MAIN "${00-aplusb_SOURCE_DIR}")
set(SOURCES ${00-source-files})

message(INFO ${SOURCES})

set(TEST_SOURCES test.cpp)
run_coverage(
    "${TEST_SOURCES}"
    "${SOURCES}"
    "${COVERAGE_TARGET}"
    "${TARGET_NAME}"
    "${COVERAGE_NAME}"
)
