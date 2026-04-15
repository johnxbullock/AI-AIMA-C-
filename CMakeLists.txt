cmake_minimum_required(VERSION 3.16)

project(ClassicalPlanning LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_executable(ClassicalPlanning
  #main.cpp
  MonkeyMain.cpp
)

add_subdirectory(domain)
add_subdirectory(engine)
add_subdirectory(problems)

target_link_libraries(ClassicalPlanning Qt${QT_VERSION_MAJOR}::Core domain engine)

include(GNUInstallDirs)
install(TARGETS ClassicalPlanning
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
