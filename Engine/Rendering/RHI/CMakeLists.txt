# Luma::RHI - abstract rendering interface.
# Provides comprehensive RHI abstraction for multiple backends.

add_library(LumaRHI)
add_library(Luma::RHI ALIAS LumaRHI)

target_include_directories(LumaRHI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaRHI PUBLIC Luma::Core Luma::Math)

# Add source files for RHI implementation utilities
target_sources(LumaRHI PRIVATE 
    src/RHI.cpp
)
