# Luma::RHI - abstract rendering interface (header-only).

add_library(LumaRHI INTERFACE)
add_library(Luma::RHI ALIAS LumaRHI)

target_include_directories(LumaRHI INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaRHI INTERFACE Luma::Core)
