cmake_minimum_required(VERSION 3.16)
project(GuipperTextTests LANGUAGES CXX)
enable_testing()
add_executable(text_edit_tests text_edit_tests.cpp)
target_compile_features(text_edit_tests PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(text_edit_tests PRIVATE /W4 /utf-8)
else()
    target_compile_options(text_edit_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME text_edit COMMAND text_edit_tests)

add_executable(transition_tests transition_tests.cpp)
target_compile_features(transition_tests PRIVATE cxx_std_17)
add_test(NAME transition COMMAND transition_tests)

if(WIN32 AND GUIPPER_WINSPARKLE_SDK)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../release/keys/guipper-windows-ed25519.pub" GUIPPER_PUBLIC_KEY)
    string(STRIP "${GUIPPER_PUBLIC_KEY}" GUIPPER_PUBLIC_KEY)
    add_executable(windows_update_probe windows_update_probe.cpp
        ../src/JPutils/jp_update_platform.cpp ../src/JPutils/jp_app_paths.cpp)
    target_compile_features(windows_update_probe PRIVATE cxx_std_17)
    target_include_directories(windows_update_probe PRIVATE "${GUIPPER_WINSPARKLE_SDK}/include")
    target_compile_definitions(windows_update_probe PRIVATE GUIPPER_UPDATE_TEST GUIPPER_WINSPARKLE=1
        GUIPPER_VERSION="0.1.0-beta.5"
        GUIPPER_WINSPARKLE_PUBLIC_KEY="${GUIPPER_PUBLIC_KEY}"
        GUIPPER_WINSPARKLE_STABLE="https://example.invalid/appcast.xml"
        GUIPPER_WINSPARKLE_BETA="https://example.invalid/appcast.xml")
    target_link_libraries(windows_update_probe PRIVATE "${GUIPPER_WINSPARKLE_SDK}/x64/Release/WinSparkle.lib" shell32 user32)
    add_custom_command(TARGET windows_update_probe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GUIPPER_WINSPARKLE_SDK}/x64/Release/WinSparkle.dll" "$<TARGET_FILE_DIR:windows_update_probe>")
endif()

if(MSVC)
    target_compile_options(transition_tests PRIVATE /W4 /utf-8 /UNDEBUG)
else()
    target_compile_options(transition_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
