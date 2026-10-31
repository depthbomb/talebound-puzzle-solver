cmake_minimum_required(VERSION 4.4)

include(cmake/Dependencies.cmake)
project(TaleboundPuzzleSolver VERSION 0.2.1 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Talebound Puzzle Solver requires Windows and MSVC.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The generator expression keeps Visual Studio from adding a second configuration directory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<LOWER_CASE:$<CONFIG>>")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

include(cmake/Metadata.cmake)
find_package(wxWidgets 3.3 CONFIG REQUIRED)

add_library(project_options INTERFACE)
target_compile_options(project_options INTERFACE /W4 /permissive- /utf-8 /EHsc)
target_compile_definitions(project_options INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_include_directories(project_options INTERFACE src "${PROJECT_BINARY_DIR}/generated")

add_library(mushroom STATIC src/puzzles/mushroom/board.cc src/puzzles/mushroom/solver.cc)
target_link_libraries(mushroom PUBLIC project_options)

add_library(flower STATIC
    src/puzzles/flower/board.cc
    src/puzzles/flower/solver.cc
    src/puzzles/flower/constraint_solver.cc
)
target_link_libraries(flower PUBLIC project_options)

add_library(tetris STATIC src/puzzles/tetris/board.cc src/puzzles/tetris/pieces.cc src/puzzles/tetris/solver.cc)
target_link_libraries(tetris PUBLIC project_options)

add_library(puzzle_ui STATIC
    src/ui/main_frame.cc
    src/puzzles/mushroom/mushroom_panel.cc
    src/puzzles/flower/flower_grid.cc
    src/puzzles/flower/flower_panel.cc
    src/puzzles/tetris/tetris_grid.cc
    src/puzzles/tetris/tetris_panel.cc
)
target_link_libraries(puzzle_ui PUBLIC mushroom flower tetris wx::core wx::base)
target_precompile_headers(puzzle_ui PRIVATE <wx/wxprec.h>)

add_executable(talebound-puzzle-solver WIN32
    src/app.cc
    src/resources/app.manifest
    "${PROJECT_BINARY_DIR}/generated/version.rc"
)
target_link_libraries(talebound-puzzle-solver PRIVATE puzzle_ui)

include(CTest)
if(BUILD_TESTING)
    foreach(puzzle IN ITEMS mushroom flower tetris)
        add_executable(${puzzle}_tests tests/${puzzle}_tests.cc)
        target_link_libraries(${puzzle}_tests PRIVATE ${puzzle})
        add_test(NAME ${puzzle}_solver COMMAND ${puzzle}_tests)
        set_tests_properties(${puzzle}_solver PROPERTIES TIMEOUT 30)
    endforeach()
    set_tests_properties(tetris_solver PROPERTIES TIMEOUT 120)

    add_executable(flower_benchmark tests/flower_benchmark.cc)
    target_link_libraries(flower_benchmark PRIVATE flower)

    add_executable(solver_benchmark tests/solver_benchmark.cc)
    target_link_libraries(solver_benchmark PRIVATE mushroom tetris)

    add_executable(puzzle_ui_tests
        tests/ui_test_main.cc
        tests/mushroom_ui_tests.cc
        tests/flower_ui_tests.cc
        tests/tetris_ui_tests.cc
        src/resources/app.manifest
    )
    target_link_libraries(puzzle_ui_tests PRIVATE puzzle_ui)
    add_test(NAME mushroom_ui_workflow COMMAND puzzle_ui_tests mushroom)
    add_test(NAME flower_ui_workflow COMMAND puzzle_ui_tests flower)
    add_test(NAME tetris_ui_workflow COMMAND puzzle_ui_tests tetris)
    set_tests_properties(mushroom_ui_workflow flower_ui_workflow tetris_ui_workflow PROPERTIES TIMEOUT 30)
endif()

install(TARGETS talebound-puzzle-solver RUNTIME DESTINATION .)
