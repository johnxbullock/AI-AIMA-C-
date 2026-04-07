# src/CMakeLists.txt
add_library(domain STATIC)

# If you have QObject-derived classes with Q_OBJECT in this library,
# make sure AUTOMOC runs for this target:
set_target_properties(domain PROPERTIES
    AUTOMOC ON
    AUTOUIC OFF
    AUTORCC OFF
)

target_sources(domain PRIVATE
    #PlanningProblem.h
    state.h
    action.h
)

target_compile_features(domain PUBLIC cxx_std_17)

target_include_directories(domain PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link the Qt modules needed by code in src
target_link_libraries(domain PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)
