# Luma::Gizmo - viewport transform gizmos (geometry + manipulation math).

add_library(LumaGizmo STATIC
    src/TranslateGizmo.cpp)

add_library(Luma::Gizmo ALIAS LumaGizmo)

target_include_directories(LumaGizmo
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaGizmo PUBLIC Luma::Core Luma::Math Luma::RHI)

set_target_properties(LumaGizmo PROPERTIES FOLDER "Editor")
