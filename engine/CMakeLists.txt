# src/CMakeLists.txt
add_library(engine STATIC)

# If you have QObject-derived classes with Q_OBJECT in this library,
# make sure AUTOMOC runs for this target:
set_target_properties(engine PROPERTIES
    AUTOMOC ON
    AUTOUIC OFF
    AUTORCC OFF
)

target_sources(engine PRIVATE
    PlanningProblem.h PlanningProblem.cpp
    orchestrator.h orchestrator.cpp
    logic/expr.h   logic/expr.cpp
    logic/utils.h  logic/utils.cpp
    logic/cnf.h    logic/cnf.cpp
    logic/kb.h     logic/kb.cpp
    logic/dpll.h   logic/dpll.cpp
    logic/graph.h  logic/graph.cpp
    logic/cdcl.h   logic/cdcl.cpp
    logic/unify.h  logic/unify.cpp
    logic/fol_kb.h logic/fol_kb.cpp
    planning/action.h          planning/action.cpp
    planning/planning_problem.h planning/planning_problem.cpp
    planning/plan_search.h      planning/plan_search.cpp
    search/problem.h
    search/search.h
)

target_compile_features(engine PUBLIC cxx_std_17)

target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link the Qt modules needed by code in src
target_link_libraries(engine PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)
