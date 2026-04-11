set(TARGET_NAME "03-weather")
set(COVERAGE_NAME "03-coverage")
set(COVERAGE_TARGET "03-coverage-weather")
set(SOURCE_DIR_MAIN "${03-weather_SOURCE_DIR}")
set(SOURCES ${03-source-files})

message(INFO ${SOURCES})

file(GLOB_RECURSE RESP_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "data/resp*.txt")

foreach(RESP_FILE IN LISTS RESP_FILES)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${RESP_FILE}
        ${CMAKE_BINARY_DIR}/${RESP_FILE}
        COPYONLY
    )
endforeach()


set(TEST_SOURCES WeatherTestCase.h WeatherTestCase.cpp WeatherMock.h WeatherMock.cpp)
run_coverage(
        "${TEST_SOURCES}"
        "${SOURCES}"
        "${COVERAGE_TARGET}"
        "${TARGET_NAME}"
        "${COVERAGE_NAME}"
)
