# Luma::Shader - Shader system with dual language support (GLSL + HLSL)

add_library(LumaShader STATIC
    src/Shader.cpp
    src/ShaderParameterStruct.cpp
    src/GlobalShader.cpp
    src/ShaderCompiler.cpp)

add_library(Luma::Shader ALIAS LumaShader)

target_include_directories(LumaShader
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(LumaShader
    PUBLIC Luma::Core Luma::RHI Luma::Math)

set_target_properties(LumaShader PROPERTIES FOLDER "Engine")