cmake_minimum_required(VERSION 3.22)
project(felitronics_appkit VERSION 0.7.0 LANGUAGES C CXX)   # C: juce_graphics compiles Sheenbidi (.c) in the JUCE test tier

# ----------------------------------------------------------------------------
# felitronics-appkit — APP-SIDE (non-DSP) JUCE infrastructure shared by the Darwin's Cat product
# family (OrbitCab / TabbyEQ / OrbitCapture). Deliberately a SEPARATE repo from felitronics-core:
# core stays conceptually pure DSP with zero JUCE anywhere in its tree; everything here assumes a
# JUCE app/plugin around it.
#
# Header-only. The interface target adds an include path and NOTHING else — the CONSUMER supplies
# JUCE (see the README table for each header's modules). UpdateCompare.h is JUCE-free and covered
# by this repo's own ctest; DeviceSpec.h (juce_core) gets its behaviour unit in the JUCE tier; the
# JUCE headers are compile-gated here against a fetched JUCE (ConsumerCompileCheck) and
# behaviour-covered by the consumers' CI.
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

    add_executable(appkit_level_history_falsify_tests tests/LevelHistoryFalsifyTests.cpp)
    target_link_libraries(appkit_level_history_falsify_tests PRIVATE
        felitronics::appkit
        juce::juce_audio_basics juce::juce_gui_basics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_level_history_falsify_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_level_history_falsify_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_level_history_falsify_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_level_history_falsify_tests COMMAND appkit_level_history_falsify_tests)

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

    # Chrome layer — theory-driven falsification (headless render + pixel counts + direct gesture
    # handlers). juce_gui_basics only, like the other GUI Falsify suites.
    add_executable(appkit_chrome_falsify_tests tests/ChromeFalsifyTests.cpp)
    target_link_libraries(appkit_chrome_falsify_tests PRIVATE
        felitronics::appkit
        juce::juce_gui_basics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_chrome_falsify_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_chrome_falsify_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_chrome_falsify_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_chrome_falsify_tests COMMAND appkit_chrome_falsify_tests)

    # Chrome layer — the à-la-carte MINIMAL consumer: ONE chrome header first, linked against the
    # SMALLEST module set (juce_gui_basics only), a shell with blister + underline and NO compare/
    # preset. Proves the composition is optional and each header is self-contained.
    add_executable(appkit_chrome_minimal_consumer tests/ChromeMinimalConsumer.cpp)
    target_link_libraries(appkit_chrome_minimal_consumer PRIVATE
        felitronics::appkit
        juce::juce_gui_basics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_chrome_minimal_consumer PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_chrome_minimal_consumer PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_chrome_minimal_consumer PRIVATE -Werror)
    endif()
    add_test(NAME appkit_chrome_minimal_consumer COMMAND appkit_chrome_minimal_consumer)

    # Chrome layer — per-header self-containment gate: ONE TU per public header, each including its
    # header FIRST and ALONE, so a header that forgot an include (relying on a sibling's prelude)
    # fails HERE rather than only when a consumer includes it first. The value is the COMPILE of each
    # sibling TU under the consumer flag set; the executable just needs an entry point.
    add_executable(appkit_chrome_selfcontained
        tests/chrome_selfcontained/main.cpp
        tests/chrome_selfcontained/ChromeMetrics.cpp
        tests/chrome_selfcontained/ChromeBar.cpp
        tests/chrome_selfcontained/BrandBlister.cpp
        tests/chrome_selfcontained/ChromeUnderline.cpp
        tests/chrome_selfcontained/FlatButtons.cpp
        tests/chrome_selfcontained/CompareCell.cpp
        tests/chrome_selfcontained/PresetCell.cpp)
    target_link_libraries(appkit_chrome_selfcontained PRIVATE
        felitronics::appkit
        juce::juce_gui_basics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_chrome_selfcontained PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_chrome_selfcontained PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_chrome_selfcontained PRIVATE -Werror)
    endif()
    add_test(NAME appkit_chrome_selfcontained COMMAND appkit_chrome_selfcontained)

    add_executable(appkit_badges_falsify_tests tests/BadgesFalsifyTests.cpp)
    target_link_libraries(appkit_badges_falsify_tests PRIVATE
        felitronics::appkit
        juce::juce_events juce::juce_data_structures juce::juce_gui_basics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_badges_falsify_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_badges_falsify_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_badges_falsify_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_badges_falsify_tests COMMAND appkit_badges_falsify_tests)

    add_executable(appkit_brand_falsify_tests tests/BrandFalsifyTests.cpp)
    target_link_libraries(appkit_brand_falsify_tests PRIVATE
        felitronics::appkit
        juce::juce_gui_basics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_brand_falsify_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_brand_falsify_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_brand_falsify_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_brand_falsify_tests COMMAND appkit_brand_falsify_tests)
    # DeviceSpec.h behaviour unit — registered exactly like appkit_update_compare_tests, but the
    # model parses with juce::String (juce_core tier), so it lives HERE, not in the JUCE-free tier.
    # JUCE module sources compile into this target, so it takes the recommended flags (+ -Werror)
    # rather than APPKIT_STRICT_FLAGS.
    add_executable(appkit_device_spec_tests tests/DeviceSpecTests.cpp)
    target_link_libraries(appkit_device_spec_tests PRIVATE
        felitronics::appkit
        juce::juce_core
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_device_spec_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0)
    target_compile_features(appkit_device_spec_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_device_spec_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_device_spec_tests COMMAND appkit_device_spec_tests)

    # Theory-first falsification suites (registered exactly like appkit_device_spec_tests): the
    # DeviceSpec grammar property-fuzz and the Flicker shimmer kernel (bounds + phase-freeze fix).
    add_executable(appkit_device_spec_theory_tests tests/DeviceSpecTheoryTests.cpp)
    target_link_libraries(appkit_device_spec_theory_tests PRIVATE
        felitronics::appkit
        juce::juce_core
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_device_spec_theory_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0)
    target_compile_features(appkit_device_spec_theory_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_device_spec_theory_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_device_spec_theory_tests COMMAND appkit_device_spec_theory_tests)

    add_executable(appkit_flicker_theory_tests tests/FlickerTheoryTests.cpp)
    target_link_libraries(appkit_flicker_theory_tests PRIVATE
        felitronics::appkit
        juce::juce_core
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_flicker_theory_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0)
    target_compile_features(appkit_flicker_theory_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_flicker_theory_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_flicker_theory_tests COMMAND appkit_flicker_theory_tests)

    add_executable(appkit_device_glyph_falsify_tests tests/DeviceGlyphFalsifyTests.cpp)
    target_link_libraries(appkit_device_glyph_falsify_tests PRIVATE
        felitronics::appkit
        juce::juce_gui_basics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_device_glyph_falsify_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_compile_features(appkit_device_glyph_falsify_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_device_glyph_falsify_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_device_glyph_falsify_tests COMMAND appkit_device_glyph_falsify_tests)

    add_executable(appkit_compare_history_tests tests/CompareHistoryTests.cpp)
    target_link_libraries(appkit_compare_history_tests PRIVATE
        felitronics::appkit
        juce::juce_events juce::juce_data_structures
        juce::juce_recommended_warning_flags)
    target_compile_definitions(appkit_compare_history_tests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0)
    target_compile_features(appkit_compare_history_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(appkit_compare_history_tests PRIVATE -Werror)
    endif()
    add_test(NAME appkit_compare_history_tests COMMAND appkit_compare_history_tests)

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
