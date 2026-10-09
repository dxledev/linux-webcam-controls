cmake_minimum_required(VERSION 3.24)
project(WebcamControls VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

add_library(webcam_core
    src/core/storage.cpp
    src/core/theme.cpp
    src/core/theme_source.cpp
    src/core/controls.cpp
    src/core/camera_service.cpp
)
target_include_directories(webcam_core PUBLIC src)
target_link_libraries(webcam_core PUBLIC Qt6::Core Qt6::Gui)
target_compile_options(webcam_core PRIVATE -Wall -Wextra -Wpedantic)

add_library(webcam_ui
    src/ui/theme_watcher.cpp
    src/ui/icons.cpp
    src/ui/modal.cpp
    src/ui/preview.cpp
    src/ui/control_row.cpp
    src/ui/window.cpp
)
target_link_libraries(webcam_ui PUBLIC webcam_core Qt6::Widgets Qt6::Multimedia)
target_compile_options(webcam_ui PRIVATE -Wall -Wextra -Wpedantic)

add_executable(webcam-controls src/main.cpp src/cli.cpp)
target_link_libraries(webcam-controls PRIVATE webcam_ui)
target_compile_options(webcam-controls PRIVATE -Wall -Wextra -Wpedantic)

include(GNUInstallDirs)
install(TARGETS webcam-controls RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/io.github.webcam_controls.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/io.github.webcam_controls.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE webcam_core Qt6::Test)
    target_compile_definitions(core_tests PRIVATE TEST_SOURCE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_test(NAME core COMMAND core_tests)
    add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE webcam_ui Qt6::Test)
    target_compile_definitions(ui_tests PRIVATE TEST_SOURCE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_test(NAME ui COMMAND ui_tests)
    set_tests_properties(ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME cli COMMAND /usr/bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.sh $<TARGET_FILE:webcam-controls>)
    add_test(NAME install COMMAND /usr/bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/install_test.sh ${CMAKE_CURRENT_SOURCE_DIR} $<TARGET_FILE_DIR:webcam-controls>)
endif()
