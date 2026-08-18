# Luma::Material - flat PBR material properties + .lmat serializer.
# Node graph removed; to be rebuilt later.

add_library(LumaMaterial STATIC
    src/MaterialSerializer.cpp)

add_library(Luma::Material ALIAS LumaMaterial)

target_include_directories(LumaMaterial
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaMaterial
    PUBLIC Luma::Core Luma::Math Luma::Asset Luma::Serialization)

set_target_properties(LumaMaterial PROPERTIES FOLDER "Engine")
