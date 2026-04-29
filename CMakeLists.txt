# cmake_minimum_required(VERSION 3.16)
# project(VPTreePlagiarismDetector VERSION 1.0.0 LANGUAGES CXX)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_AUTOMOC ON)        # Qt Meta-Object Compiler
# set(CMAKE_AUTORCC ON)
# set(CMAKE_AUTOUIC ON)

# # ---------------------------------------------------------------------------
# # Find Qt — tries Qt6 first, falls back to Qt5
# # ---------------------------------------------------------------------------
# find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
# find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# message(STATUS "Using Qt ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}")

# # ---------------------------------------------------------------------------
# # Sources
# # ---------------------------------------------------------------------------
# set(SOURCES
#     src/main.cpp
#     src/ngram.cpp
#     src/vptree.cpp
#     src/engine.cpp
#     src/mainwindow.cpp
# )

# set(HEADERS
#     src/rawbuffer.h
#     src/ngram.h
#     src/vptree.h
#     src/engine.h
#     src/mainwindow.h
# )

# # ---------------------------------------------------------------------------
# # Executable
# # ---------------------------------------------------------------------------
# add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# target_include_directories(${PROJECT_NAME} PRIVATE src)
# target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# # Threading support (for QThread / std::thread)
# find_package(Threads REQUIRED)
# target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# # ---------------------------------------------------------------------------
# # Platform-specific settings
# # ---------------------------------------------------------------------------
# if(WIN32)
#     set_target_properties(${PROJECT_NAME} PROPERTIES
#         WIN32_EXECUTABLE TRUE
#     )
# endif()

cmake_minimum_required(VERSION 3.10)
project(VPTreePlagiarismDetector)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Target 1: The GUI Application
add_executable(PlagiarismApp
    main.cpp
    mainwindow.cpp
    mainwindow.h
    engine.cpp
    engine.h
    vptree.cpp
    vptree.h
    ngram.cpp
    ngram.h
    rawbuffer.h
)
target_link_libraries(PlagiarismApp PRIVATE Qt6::Widgets)

# Target 2: The Unit Tests
add_executable(UnitTests
    test_engine.cpp
    engine.cpp
    engine.h
    vptree.cpp
    vptree.h
    ngram.cpp
    ngram.h
    rawbuffer.h
)