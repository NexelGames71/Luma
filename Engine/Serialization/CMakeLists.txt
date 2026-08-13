# Luma::Serialization - the SerialValue intermediate representation plus concrete
# formats (JSON now; binary later). Depends only on Luma::Core. It is the shared
# foundation for scene/asset/project persistence and reflection-driven save/load.

add_library(LumaSerialization STATIC
    src/SerialValue.cpp
    src/Json.cpp)

add_library(Luma::Serialization ALIAS LumaSerialization)

target_include_directories(LumaSerialization
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaSerialization PUBLIC Luma::Core)

set_target_properties(LumaSerialization PROPERTIES FOLDER "Engine")
