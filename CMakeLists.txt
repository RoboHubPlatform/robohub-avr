#
# robohub-avr — the AVR platform for the RoboHub Platform.
#
# This builds the implementations of the RoboHub HAL interfaces on an 8-bit AVR
# part. It is a static library: a board build links it, and the linker discards
# the parts of it a project does not use.
#
# There is no vendor HAL here, which is the whole difference from a platform
# that sits on one. The part's registers are the interface, the toolchain carries
# the startup and the linker script, and what is here is the drivers and the
# board.
#
# @project RoboHub Platform
# @license MIT
#

cmake_minimum_required(VERSION 3.16)

project(robohub-avr LANGUAGES C VERSION 0.1.0)

include(CTest)

# ---------------------------------------------------------------------------
#  robohub-common
# ---------------------------------------------------------------------------
#
# The interfaces this platform implements are declared by robohub-common, so its
# headers are needed to compile anything here. A build supplies them either as an
# installed package or, in a checkout of the ecosystem, as a sibling directory.
# Which one is not this module's decision, so both are offered and the first that
# exists is used.

set(ROBOHUB_COMMON_INCLUDE_DIR "" CACHE PATH
    "Path to robohub-common's include directory")

if(NOT ROBOHUB_COMMON_INCLUDE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
endif()

if(NOT ROBOHUB_COMMON_INCLUDE_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../robohub-common/include")
        set(ROBOHUB_COMMON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../robohub-common/include")
        message(STATUS "robohub-common: using the sibling checkout")
    endif()
endif()

if(NOT ROBOHUB_COMMON_INCLUDE_DIR)
    message(FATAL_ERROR
        "robohub-common's headers were not found.\n"
        "  - check out robohub-common beside this repository, or\n"
        "  - build this project as a Rho dependency, so rho-target writes deps.cmake from\n"
        "    the manifests, or\n"
        "  - pass -DROBOHUB_COMMON_INCLUDE_DIR=<path>.")
endif()

message(STATUS "robohub-common headers: ${ROBOHUB_COMMON_INCLUDE_DIR}")

get_filename_component(ROBOHUB_COMMON_ROOT "${ROBOHUB_COMMON_INCLUDE_DIR}" DIRECTORY)

# Only for a build that did not come through the dependency resolver. One that did
# has already had robohub-common added by the generated project — after this file
# is read, which is why `TARGET robohub-common` cannot answer this — and adding it
# again fails on a name already taken.
if(PROJECT_IS_TOP_LEVEL
   AND NOT TARGET robohub-common AND NOT TARGET rho::robohub-common)
    if(EXISTS "${ROBOHUB_COMMON_ROOT}/CMakeLists.txt")
        message(STATUS "[robohub-avr] building robohub-common from ${ROBOHUB_COMMON_ROOT}")
        # The vendored project is added outside the default target, so a test it
        # registers would be a test this build never builds — and a test that is
        # registered and not built is a test that fails for saying nothing.
        set(RHO_BUILD_HOST_TESTS_SAVED ${RHO_BUILD_HOST_TESTS})
        set(RHO_BUILD_HOST_TESTS OFF)
        add_subdirectory("${ROBOHUB_COMMON_ROOT}" "${CMAKE_BINARY_DIR}/_robohub-common" EXCLUDE_FROM_ALL)
        set(RHO_BUILD_HOST_TESTS ${RHO_BUILD_HOST_TESTS_SAVED})
    else()
        message(FATAL_ERROR
            "robohub-common's headers were found at ${ROBOHUB_COMMON_INCLUDE_DIR}, but\n"
            "  there is no build to link against beside them.")
    endif()
endif()
if(TARGET robohub-common AND NOT TARGET rho::robohub-common)
    add_library(rho::robohub-common ALIAS robohub-common)
endif()

# ---------------------------------------------------------------------------
#  Sources
# ---------------------------------------------------------------------------
#
# Whether this is a host build, decided once.
#
# A bare-metal build links a C library with no `_kill` or `_getpid` for a host
# test to resolve against. So the same two facts decide both whether this library
# is compiled for a part and whether there are any tests to register, and they are
# read in one place because a build that decided them differently in two places
# would compile a host library with a part's sources in it.
set(ROBOHUB_AVR_HOSTED OFF)
if(PROJECT_IS_TOP_LEVEL AND RHO_BUILD_HOST_TESTS AND NOT RHO_TARGET_IS_BAREMETAL)
    set(ROBOHUB_AVR_HOSTED ON)
endif()

# The resolved configuration lives in the generated project, which is where the
# build that produced it put it. This directory is a subdirectory of that one, so
# the path is handed down rather than guessed from here.
set(ROBOHUB_AVR_CONFIG_KNOWN OFF)
if(RHO_ROBO_CONFIG AND EXISTS "${RHO_ROBO_CONFIG}")
    include("${RHO_ROBO_CONFIG}")
    set(ROBOHUB_AVR_CONFIG_KNOWN ON)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/robo-config.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/robo-config.cmake")
    set(ROBOHUB_AVR_CONFIG_KNOWN ON)
endif()

# Always compiled: the platform itself — the console and the clock every
# driver's delay is counted in. None of it is a peripheral a program chooses.
set(ROBOHUB_AVR_BAREMETAL_SOURCES
    src/platform/avr_console.c
    src/platform/avr_platform.c
)

# And the driver for each symbol the project set. A symbol nobody set contributes
# nothing, so the image holds the drivers it asked for and no others.
#
# With no configuration to consult, every driver is built: this module is being
# compiled on its own rather than for a program, and the answer to "which of these
# does that program want" has not been asked yet. Guessing "none" would compile a
# module that cannot work.
set(ROBOHUB_AVR_CORE_SOURCES ${ROBOHUB_AVR_BAREMETAL_SOURCES})

if(NOT ROBOHUB_AVR_HOSTED)
    foreach(driver
            "GPIO:src/hal/avr_gpio.c"
            "UART:src/hal/avr_uart.c"
            "TIMER:src/hal/avr_timer.c"
            "ADC:src/hal/avr_adc.c"
            "PWM:src/hal/avr_pwm.c"
            "I2C:src/hal/avr_i2c.c"
            "SPI:src/hal/avr_spi.c"
            "ONEWIRE:src/hal/avr_onewire.c"
            "DHT:src/hal/avr_dht.c")
        string(REPLACE ":" ";" parts "${driver}")
        list(GET parts 0 symbol)
        list(GET parts 1 source)
        if(NOT ROBOHUB_AVR_CONFIG_KNOWN
           OR (DEFINED RHO_CONFIG_RH_HAL_${symbol} AND RHO_CONFIG_RH_HAL_${symbol}))
            list(APPEND ROBOHUB_AVR_CORE_SOURCES ${source})
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
#  Configuration
# ---------------------------------------------------------------------------
#
# The values a project declared, resolved by rho-kconfig into
# generated/autoconf.h.

if(EXISTS ${CMAKE_BINARY_DIR}/generated/autoconf.h)
    add_compile_definitions(HAVE_ROBOHUB_AUTOCONF=1)
endif()

include_directories(${CMAKE_BINARY_DIR}/generated)

# ---------------------------------------------------------------------------
#  Library
# ---------------------------------------------------------------------------

add_library(robohub-avr STATIC ${ROBOHUB_AVR_CORE_SOURCES})

# The interfaces this module implements live in robohub-common, so this library
# cannot be linked without it. PUBLIC, not PRIVATE: a consumer of this one needs
# the interfaces too.
target_link_libraries(robohub-avr PUBLIC rho::robohub-common)

target_include_directories(robohub-avr PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/drivers
    ${ROBOHUB_COMMON_INCLUDE_DIR}
    ${CMAKE_BINARY_DIR}/generated
    $<INSTALL_INTERFACE:include>
)

# The runtime's output is the platform's console. A Rho program prints through
# the C library, and the C library's stdout has to go somewhere: a target whose
# console is its own says which function writes to it, and this is that.
target_compile_definitions(robohub-avr PUBLIC RHO_PROGRAM_WRITE=rho_avr_write)

# And the runtime's stop is the platform's too. A panic on a part this small ends
# in the C library's `exit` otherwise, which is a firmware carrying an exit path
# it has nowhere to go from; here it ends where a debugger can see it.
target_compile_definitions(robohub-avr PUBLIC RHO_PROGRAM_HALT=rho_avr_halt)

target_compile_features(robohub-avr PUBLIC c_std_11)

# ---------------------------------------------------------------------------
#  Tests
# ---------------------------------------------------------------------------
#
# The chip-independent half is testable on the host, which is the only way these
# run without a board on the bench.
if(ROBOHUB_AVR_HOSTED)
    message(STATUS "[robohub-avr] registering host unit tests")
    enable_testing()
    add_executable(robohub-avr-test-error tests/test_error.c)
    target_link_libraries(robohub-avr-test-error PRIVATE robohub-avr)
    add_test(NAME robohub-avr-test-error COMMAND robohub-avr-test-error)
endif()

# ---------------------------------------------------------------------------
#  Install
# ---------------------------------------------------------------------------

install(FILES module.yml DESTINATION share/robohub-avr)
install(DIRECTORY include/ DESTINATION include)
install(TARGETS robohub-avr
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
