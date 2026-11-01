cmake_minimum_required(VERSION 3.25)
project(util VERSION 0.1.0 DESCRIPTION "small shared utility classes (os3::util)" LANGUAGES CXX)

option(BUILD_TESTING "Build the testing tree." ${PROJECT_IS_TOP_LEVEL})
include(CTest)

if(NOT CMAKE_GENERATOR)
    set(CMAKE_GENERATOR "Ninja" CACHE STRING "Force Ninja generator" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "Profile" "Coverage")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG)
else()
    add_compile_definitions(NDEBUG)
endif()

# warnings are carried by an interface target so dependencies are not -Werror'd
add_library(util_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(util_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror -Wnarrowing)
    target_link_options(util_warnings INTERFACE -fno-omit-frame-pointer)
endif()

# sanitizers and coverage are mutually exclusive, coverage wins
option(ENABLE_SANITIZERS "Enable Address/Undefined/Leak sanitizers in Debug builds" ${PROJECT_IS_TOP_LEVEL})
option(ENABLE_COVERAGE "Instrument os3_util/util_test with gcov and add a 'coverage' target" OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENABLE_SANITIZERS AND NOT ENABLE_COVERAGE)
    add_compile_options(-fsanitize=address,undefined,leak -fno-omit-frame-pointer -g3 -O1)
    add_link_options(-fsanitize=address,undefined,leak)
endif()

if(ENABLE_COVERAGE)
    if(NOT BUILD_TESTING)
        message(FATAL_ERROR "ENABLE_COVERAGE requires BUILD_TESTING")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "ENABLE_COVERAGE needs GCC's gcov - reconfigure with CXX=g++")
    endif()
    set(UTIL_COVERAGE_COMPILE_FLAGS --coverage -O0 -g)
    set(UTIL_COVERAGE_LINK_FLAGS --coverage)
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
include(cmake/CPM.cmake)

CPMAddPackage(
    NAME fmt
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_TAG 12.2.0
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
    NAME logger
    GITHUB_REPOSITORY ostri/logger
    GIT_TAG v0.1.14
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
)

if(BUILD_TESTING)
    CPMAddPackage(
        NAME Catch2
        GITHUB_REPOSITORY catchorg/Catch2
        GIT_TAG v3.15.3
        SYSTEM YES
        EXCLUDE_FROM_ALL YES
    )
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)
endif()

# ===================================================================
# util library - every class of os3::util in one static library
# ===================================================================
add_library(os3_util STATIC
    src/os3/util/sf-node.cpp
    src/os3/util/sf-number.cpp
    src/os3/util/t2-calendar.cpp
    src/os3/util/unique-id.cpp
)
add_library(os3::util ALIAS os3_util)

target_include_directories(os3_util PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_link_libraries(os3_util
    PUBLIC  fmt::fmt logger::logger
    PRIVATE util_warnings
)
if(CMAKE_BUILD_TYPE STREQUAL "Profile")
    target_compile_options(os3_util PRIVATE -O3 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls)
endif()
if(ENABLE_COVERAGE)
    target_compile_options(os3_util PRIVATE ${UTIL_COVERAGE_COMPILE_FLAGS})
    target_link_options(os3_util PRIVATE ${UTIL_COVERAGE_LINK_FLAGS})
endif()

# ===================================================================
# util_xml library - os3::util classes on top of Xerces-C (x_str, xerces_mgr)
# ===================================================================
find_package(XercesC QUIET)
if(XercesC_FOUND)
    add_library(os3_util_xml STATIC
        src/os3/util/x-str.cpp
        src/os3/util/xerces-mgr.cpp
    )
    add_library(os3::util_xml ALIAS os3_util_xml)

    target_include_directories(os3_util_xml PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
    target_link_libraries(os3_util_xml
        PUBLIC  XercesC::XercesC fmt::fmt
        PRIVATE util_warnings
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Profile")
        target_compile_options(os3_util_xml PRIVATE -O3 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls)
    endif()
    if(ENABLE_COVERAGE)
        target_compile_options(os3_util_xml PRIVATE ${UTIL_COVERAGE_COMPILE_FLAGS})
        target_link_options(os3_util_xml PRIVATE ${UTIL_COVERAGE_LINK_FLAGS})
    endif()
endif()

# ===================================================================
# clang-tidy
# ===================================================================
find_program(RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy run-clang-tidy.py)
if(RUN_CLANG_TIDY_EXECUTABLE AND PROJECT_IS_TOP_LEVEL)
    add_custom_target(tidy
        COMMAND ${RUN_CLANG_TIDY_EXECUTABLE}
            -p ${CMAKE_CURRENT_BINARY_DIR}
            -config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy
            -quiet
            -header-filter "^${CMAKE_CURRENT_SOURCE_DIR}/src/.*\\.hpp$"
            -source-filter "^${CMAKE_CURRENT_SOURCE_DIR}/src/.*\\.cpp$"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy (parallel)"
        VERBATIM
    )
endif()

# ===================================================================
# tests
# ===================================================================
if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_executable(util_test
        test/test-sf-node-init.cpp
        test/test-sf-number.cpp
        test/test-t2-calendar.cpp
        test/test-unique-id.cpp
    )
    target_link_libraries(util_test PRIVATE os3::util Catch2::Catch2WithMain util_warnings)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENABLE_SANITIZERS)
        target_compile_options(util_test PRIVATE -fsanitize=address,undefined,leak -fno-omit-frame-pointer -g3 -O1)
        target_link_options(util_test PRIVATE -fsanitize=address,undefined,leak)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Profile")
        target_compile_options(util_test PRIVATE -O3 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls)
    endif()
    if(ENABLE_COVERAGE)
        target_compile_options(util_test PRIVATE ${UTIL_COVERAGE_COMPILE_FLAGS})
        target_link_options(util_test PRIVATE ${UTIL_COVERAGE_LINK_FLAGS})
    endif()

    catch_discover_tests(util_test
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PROPERTIES LABELS "unit"
    )

    if(XercesC_FOUND)
        add_executable(util_xml_test
            test/test-x-str.cpp
            test/test-x-str-utf.cpp
            test/test-xerces-mgr.cpp
        )
        target_link_libraries(util_xml_test PRIVATE os3::util_xml Catch2::Catch2WithMain util_warnings)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENABLE_SANITIZERS)
            target_compile_options(util_xml_test PRIVATE -fsanitize=address,undefined,leak -fno-omit-frame-pointer -g3 -O1)
            target_link_options(util_xml_test PRIVATE -fsanitize=address,undefined,leak)
        elseif(CMAKE_BUILD_TYPE STREQUAL "Profile")
            target_compile_options(util_xml_test PRIVATE -O3 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls)
        endif()
        if(ENABLE_COVERAGE)
            target_compile_options(util_xml_test PRIVATE ${UTIL_COVERAGE_COMPILE_FLAGS})
            target_link_options(util_xml_test PRIVATE ${UTIL_COVERAGE_LINK_FLAGS})
        endif()
        catch_discover_tests(util_xml_test
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            PROPERTIES LABELS "unit"
        )
    endif()

    if(ENABLE_COVERAGE)
        find_program(GCOVR_EXECUTABLE gcovr REQUIRED)
        set(COVERAGE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/coverage)
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_OUTPUT_DIR}
            COMMAND $<TARGET_FILE:util_test> || ${CMAKE_COMMAND} -E true
            COMMAND ${GCOVR_EXECUTABLE}
                --root ${CMAKE_CURRENT_SOURCE_DIR}
                --filter ${CMAKE_CURRENT_SOURCE_DIR}/src
                --exclude-unreachable-branches
                --exclude-throw-branches
                --html --html-details
                --output ${COVERAGE_OUTPUT_DIR}/index.html
                --print-summary
                ${CMAKE_CURRENT_BINARY_DIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS util_test
            COMMENT "Running util_test and rendering an HTML coverage report to ${COVERAGE_OUTPUT_DIR}/index.html"
            VERBATIM
        )
    endif()
endif()
