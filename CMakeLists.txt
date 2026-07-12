cmake_minimum_required(VERSION 3.22)
project(felitronics_appkit VERSION 0.1.0 LANGUAGES C CXX)   # C: juce_graphics compiles Sheenbidi (.c) in the JUCE test tier

# ----------------------------------------------------------------------------
# felitronics-appkit — APP-SIDE (non-DSP) JUCE infrastructure shared by the Darwin's Cat product
# family (OrbitCab / TabbyEQ / OrbitCapture). Deliberately a SEPARATE repo from felitronics-core:
# core stays conceptually pure DSP with zero JUCE anywhere in its tree; everything here assumes a
# JUCE app/plugin around it.
#
# Header-only. The interface target adds an include path and NOTHING else — the CONSUMER supplies
# JUCE (juce_events + juce_data_structures for UpdateChecker.h). UpdateCompare.h is JUCE-free and
# covered by this repo's own ctest; UpdateChecker.h is compile-gated here against a fetched JUCE
# (ConsumerCompileCheck) and behaviour-covered by the consumers' CI.
#
# Consume (pinned by tag, exactly like JUCE / felitronics-core):
#   include(FetchContent)
#   FetchContent_Declare(felitronics_appkit GIT_REPOSITORY <url> GIT_TAG <tag>)
#   FetchContent_MakeAvailable(felitronics_appkit)
#   target_link_libraries(app PRIVATE felitronics::appkit)   # + your juce_events/juce_data_structures
# ----------------------------------------------------------------------------

add_library(felitronics_appkit INTERFACE)
add_library(felitronics::appkit ALIAS felitronics_appkit)
target_include_directories(felitronics_appkit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(felitronics_appkit INTERFACE cxx_std_20)

# Tests build when this IS the top-level project (a consumer's FetchContent does not inherit them).
option(FELITRONICS_APPKIT_BUILD_TESTS "Build felitronics-appkit self-tests" "${PROJECT_IS_TOP_LEVEL}")

# The JUCE compile gate is OFF by default so a bare `cmake && ctest` stays offline; CI turns it ON
# (fetches JUCE) — or point FELITRONICS_APPKIT_JUCE_SRC at a local JUCE checkout.
option(FELITRONICS_APPKIT_TESTS_WITH_JUCE "Also build the JUCE consumer-compile gate for UpdateChecker.h" OFF)
set(FELITRONICS_APPKIT_JUCE_SRC "" CACHE PATH "Optional path to a local JUCE source tree (skips the network fetch)")

if(NOT FELITRONICS_APPKIT_BUILD_TESTS)
    return()
endif()

enable_testing()

# The same downstream-grade strict set felitronics-core gates its headers with — consumers compile
# these headers under juce_recommended_warning_flags, so warning-cleanliness is gated HERE.
set(APPKIT_STRICT_FLAGS "")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(APPKIT_STRICT_FLAGS
        -Werror -Wall -Wextra -Wpedantic
        -Wconversion -Wsign-conversion -Wfloat-equal -Wshadow -Wcast-align
        -Wzero-as-null-pointer-constant -Wunreachable-code -Wswitch-enum -Wextra-semi
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wshorten-64-to-32;-Wimplicit-float-conversion;-Wconditional-uninitialized;-Wconstant-conversion;-Wbool-conversion>)
endif()

# --- JUCE-free tier: the version-compare rule ------------------------------------------------------
add_executable(appkit_update_compare_tests tests/UpdateCompareTests.cpp)
target_link_libraries(appkit_update_compare_tests PRIVATE felitronics::appkit)
target_compile_features(appkit_update_compare_tests PRIVATE cxx_std_20)
target_compile_options(appkit_update_compare_tests PRIVATE ${APPKIT_STRICT_FLAGS})
add_test(NAME appkit_update_compare_tests COMMAND appkit_update_compare_tests)

add_executable(appkit_update_compare_falsify_tests tests/UpdateCompareFalsifyTests.cpp)
target_link_libraries(appkit_update_compare_falsify_tests PRIVATE felitronics::appkit)
target_compile_features(appkit_update_compare_falsify_tests PRIVATE cxx_std_20)
target_compile_options(appkit_update_compare_falsify_tests PRIVATE ${APPKIT_STRICT_FLAGS})
add_test(NAME appkit_update_compare_falsify_tests COMMAND appkit_update_compare_falsify_tests)

# --- JUCE tier: UpdateChecker.h through a real consumer's compiler flags ----------------------------
if(FELITRONICS_APPKIT_TESTS_WITH_JUCE)
    include(FetchContent)
    if(FELITRONICS_APPKIT_JUCE_SRC)
        FetchContent_Declare(juce SOURCE_DIR "${FELITRONICS_APPKIT_JUCE_SRC}")
    else()
        FetchContent_Declare(juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.14)   # the family's pinned JUCE
    endif()
    FetchContent_MakeAvailable(juce)

    add_executable(appkit_update_checker_falsify_tests tests/UpdateCheckerFalsifyTests.cpp)
    target_link_libraries(appkit_update_checker_falsify_tests PRIVATE
        felitronics::appkit
        juce::juce_events juce::juce_data_structures
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_update_checker_falsify_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0)
    target_compile_features(appkit_update_checker_falsify_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_update_checker_falsify_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_update_checker_falsify_tests COMMAND appkit_update_checker_falsify_tests)
    # Theory-first falsification suite for LevelMeter.h — reads back the ballistics MATH from paint()
    # via a recording graphics context (no pixels, no hooks). LevelMeter parses/renders with JUCE, so
    # it lives HERE in the JUCE tier; JUCE module sources compile into this target, so it takes the
    # recommended flags (+ -Werror) rather than APPKIT_STRICT_FLAGS.
    add_executable(appkit_level_meter_theory_tests tests/LevelMeterTheoryTests.cpp)
    target_link_libraries(appkit_level_meter_theory_tests PRIVATE
        felitronics::appkit
        juce::juce_audio_basics juce::juce_gui_basics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_level_meter_theory_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_level_meter_theory_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_level_meter_theory_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_level_meter_theory_tests COMMAND appkit_level_meter_theory_tests)

    add_executable(appkit_level_meter_falsify_tests tests/LevelMeterFalsifyTests.cpp)
    target_link_libraries(appkit_level_meter_falsify_tests PRIVATE
        felitronics::appkit
        juce::juce_audio_basics juce::juce_gui_basics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_level_meter_falsify_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_level_meter_falsify_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_level_meter_falsify_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_level_meter_falsify_tests COMMAND appkit_level_meter_falsify_tests)

    add_executable(appkit_icon_button_falsify_tests tests/IconButtonFalsifyTests.cpp)
    target_link_libraries(appkit_icon_button_falsify_tests PRIVATE
        felitronics::appkit
        juce::juce_gui_basics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_icon_button_falsify_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_icon_button_falsify_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_icon_button_falsify_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_icon_button_falsify_tests COMMAND appkit_icon_button_falsify_tests)

    add_executable(appkit_consumer_compile_check tests/ConsumerCompileCheck.cpp)
    target_link_libraries(appkit_consumer_compile_check PRIVATE
        felitronics::appkit
        juce::juce_events juce::juce_data_structures juce::juce_gui_basics juce::juce_audio_utils juce::juce_cryptography
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_consumer_compile_check PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_consumer_compile_check PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_consumer_compile_check PRIVATE -Werror)   # recommended flags must be CLEAN
    endif()
    add_test(NAME appkit_consumer_compile_check COMMAND appkit_consumer_compile_check)   # offline smoke (no --live)
endif()
