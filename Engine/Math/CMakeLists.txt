# Luma::Math - header-only math (vectors, matrices).

add_library(LumaMath INTERFACE)
add_library(Luma::Math ALIAS LumaMath)

target_include_directories(LumaMath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(LumaMath INTERFACE Luma::Core)
