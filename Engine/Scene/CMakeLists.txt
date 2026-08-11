# Luma::Scene - the ECS world (entities + components), backed by EnTT.

add_library(LumaScene STATIC
    src/Scene.cpp)

add_library(Luma::Scene ALIAS LumaScene)

target_include_directories(LumaScene
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# EnTT as a SYSTEM include so its headers don't trip our /W4 /WX.
target_include_directories(LumaScene SYSTEM
    PUBLIC ${PROJECT_SOURCE_DIR}/ThirdParty/entt/single_include)

target_link_libraries(LumaScene PUBLIC Luma::Core Luma::Math)

set_target_properties(LumaScene PROPERTIES FOLDER "Engine")
