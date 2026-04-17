# RadioLib as ESP-IDF component
#
# RadioLib source must be available at RADIOLIB_SRC_DIR.
# Default: ../../.pio/libdeps/crowpanel_advance_35/RadioLib/src
# Override with: cmake -DRADIOLIB_SRC_DIR=/path/to/RadioLib/src

if(NOT DEFINED RADIOLIB_SRC_DIR)
    set(RADIOLIB_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../.pio/libdeps/crowpanel_advance_35/RadioLib/src")
endif()

# Collect source files (only what we need: core + SX126x)
file(GLOB RADIOLIB_CORE
    "${RADIOLIB_SRC_DIR}/*.cpp"
    "${RADIOLIB_SRC_DIR}/utils/*.cpp"
)

file(GLOB RADIOLIB_SX126X
    "${RADIOLIB_SRC_DIR}/modules/SX126x/SX126x.cpp"
    "${RADIOLIB_SRC_DIR}/modules/SX126x/SX1262.cpp"
    "${RADIOLIB_SRC_DIR}/modules/SX126x/SX126x_LR_FHSS.cpp"
)

# Protocols (APRS is handled by us, but RadioLib needs at least the base)
file(GLOB RADIOLIB_PROTOCOLS
    "${RADIOLIB_SRC_DIR}/protocols/PhysicalLayer/PhysicalLayer.cpp"
)

idf_component_register(
    SRCS ${RADIOLIB_CORE} ${RADIOLIB_SX126X} ${RADIOLIB_PROTOCOLS}
    INCLUDE_DIRS "${RADIOLIB_SRC_DIR}"
    REQUIRES driver
)

# RadioLib build flags for ESP-IDF (non-Arduino)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    RADIOLIB_CUSTOM_ARDUINO=0
    RADIOLIB_GODMODE=1
    RADIOLIB_LOW_LEVEL=1
)
