cmake_minimum_required(VERSION 3.16)
project(hoptics_sim C)

set(CMAKE_C_STANDARD 11)

# LVGL 9.2.2, downloaded at configure time (pinned version).
include(FetchContent)
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v9.2.2
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(lvgl)

add_executable(hoptics_sim
    main.c
    fake_mesh.c
    ../ui/bs_model.c
    ../ui/bs_ui.c)
target_include_directories(hoptics_sim PRIVATE ../ui ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(hoptics_sim PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(hoptics_sim PRIVATE lvgl m)

# Open a real window when SDL2 is installed; otherwise screenshots only.
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    message(STATUS "SDL2 found: building window mode")
    target_compile_definitions(hoptics_sim PRIVATE BS_SIM_SDL)
    target_compile_definitions(lvgl PUBLIC BS_SIM_SDL)
    target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(hoptics_sim PRIVATE ${SDL2_LIBRARIES})
else()
    message(STATUS "SDL2 not found: screenshot mode only (--shots <dir>)")
endif()
