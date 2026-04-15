# src/CMakeLists.txt
add_library(problems STATIC)

# If you have QObject-derived classes with Q_OBJECT in this library,
# make sure AUTOMOC runs for this target:
set_target_properties(problems PROPERTIES
    AUTOMOC ON
    AUTOUIC OFF
    AUTORCC OFF
)

target_sources(problems PRIVATE
    monkey.h monkey.cpp
    MonkeyDomain.h
)

target_compile_features(problems PUBLIC cxx_std_17)

target_include_directories(problems PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link the Qt modules needed by code in src
target_link_libraries(problems PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)
