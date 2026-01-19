cmake_minimum_required(VERSION 3.16)
project(n02-linux VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Core library (networking, protocol, game logic)
add_library(n02_core STATIC
    src/common/k_socket.cpp
    src/common/k_framecache.cpp
    src/common/settings.cpp
    src/common/timer.cpp
)
target_include_directories(n02_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(n02_core PUBLIC Qt6::Core)

# Shared library for emulator integration (kailleraclient.so)
# add_library(kailleraclient SHARED
#     src/kailleraclient.cpp
# )
# target_link_libraries(kailleraclient PRIVATE n02_core Qt6::Core Qt6::Widgets)
# set_target_properties(kailleraclient PROPERTIES
#     VERSION ${PROJECT_VERSION}
#     SOVERSION 1
#     PUBLIC_HEADER include/kailleraclient.h
# )

# GUI application
add_executable(n02-gui
    src/main.cpp
)
target_link_libraries(n02-gui PRIVATE n02_core Qt6::Core Qt6::Widgets Qt6::Network)

# Tests
enable_testing()
# add_subdirectory(tests)

# Installation
# install(TARGETS kailleraclient
#     LIBRARY DESTINATION lib
#     PUBLIC_HEADER DESTINATION include
# )
install(TARGETS n02-gui RUNTIME DESTINATION bin)
