# Luma::Project - project (.luma) creation, loading and discovery.

add_library(LumaProject STATIC
    src/Project/Project.cpp)

add_library(Luma::Project ALIAS LumaProject)

target_include_directories(LumaProject
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaProject PUBLIC Luma::Core)

set_target_properties(LumaProject PROPERTIES FOLDER "Engine")
