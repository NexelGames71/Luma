# Luma::Asset - read-only asset indexing. Pure data: no Slate, no editor.
# Used by the Content Browser panel and any other system that needs to
# enumerate / filter / look up content files.

add_library(LumaAsset STATIC
    src/AssetId.cpp
    src/AssetType.cpp
    src/AssetData.cpp
    src/AssetRegistry.cpp
    src/AssetScanner.cpp)

add_library(Luma::Asset ALIAS LumaAsset)

target_include_directories(LumaAsset
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaAsset
    PUBLIC Luma::Core)

set_target_properties(LumaAsset PROPERTIES FOLDER "Engine/Asset")
