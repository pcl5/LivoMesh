cmake_minimum_required(VERSION 3.16)

project(livomesh LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LIVOMESH_BUILD_TESTS "Enable livomesh unit/integration tests" OFF)

# 启用 QtConcurrent/TBB 以便 CCCoreLib 在滤波时可以多线程。
set(CCCORELIB_USE_QT_CONCURRENT ON CACHE BOOL "" FORCE)
set(CCCORELIB_USE_TBB ON CACHE BOOL "" FORCE)
# Build CCCoreLib as static to simplify linking in downstream tools.
set(CCCORELIB_SHARED OFF CACHE BOOL "" FORCE)

add_subdirectory(3rdparty/CCCoreLib)

file(GLOB_RECURSE LIVOMESH_APP_SOURCES
    CONFIGURE_DEPENDS
    src/*.cc
    src/*.cpp
    src/*.cxx
)

if(LIVOMESH_APP_SOURCES)
    if(CCCORELIB_USE_QT_CONCURRENT)
        find_package(Qt5 COMPONENTS Concurrent REQUIRED)
    endif()
    add_executable(livomesh_app ${LIVOMESH_APP_SOURCES})

    target_include_directories(livomesh_app
        PRIVATE
            include
    )

    target_link_libraries(livomesh_app
        PRIVATE
            CCCoreLib::CCCoreLib
    )

    if(CCCORELIB_USE_QT_CONCURRENT)
        target_link_libraries(livomesh_app PRIVATE Qt5::Concurrent)
    endif()
else()
    message(STATUS "No sources found under src/. Skipping livomesh_app target.")
endif()

if(LIVOMESH_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()
