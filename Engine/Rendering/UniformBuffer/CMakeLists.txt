# Luma::UniformBuffer - Uniform buffer system for shader constants

add_library(LumaUniformBuffer STATIC
    src/UniformBuffer.cpp)

add_library(Luma::UniformBuffer ALIAS LumaUniformBuffer)

target_include_directories(LumaUniformBuffer
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(LumaUniformBuffer
    PUBLIC Luma::Core Luma::RHI Luma::Math)

set_target_properties(LumaUniformBuffer PROPERTIES FOLDER "Engine")