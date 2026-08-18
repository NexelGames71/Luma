# Luma::Asset - read-only asset indexing. Pure data: no Slate, no editor.
# Used by the Content Browser panel and any other system that needs to
# enumerate / filter / look up content files.

luma_require_assimp()

# Add stb_image for texture loading
FetchContent_Declare(
    stb_image
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb_image)

add_library(LumaAsset STATIC
    src/AssetId.cpp
    src/AssetType.cpp
    src/AssetData.cpp
    src/AssetRegistry.cpp
    src/AssetScanner.cpp
    src/LumaMesh.cpp
    src/AssetMetadata.cpp
    src/AssetImportManager.cpp
    src/MeshImporter.cpp
    src/Factory.cpp
    src/TextureImporter.cpp
    src/ImportDialog.cpp
    src/BatchImportManager.cpp
    src/AssetDependencyManager.cpp
    src/ImportValidator.cpp
    src/ThumbnailRenderingInfo.cpp
    src/ThumbnailRenderer.cpp
    src/TextureThumbnailRenderer.cpp
    src/MeshThumbnailRenderer.cpp
    src/MaterialThumbnailRenderer.cpp
)

target_compile_options(LumaAsset PRIVATE /wd4100 /wd4189)

add_library(Luma::Asset ALIAS LumaAsset)

target_include_directories(LumaAsset
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(LumaAsset
    PUBLIC Luma::Core Luma::Math Luma::Serialization
    PRIVATE assimp
    PRIVATE Luma::Renderer)

# stb_image is header-only, just include the path
target_include_directories(LumaAsset
    PRIVATE ${stb_image_SOURCE_DIR})

set_target_properties(LumaAsset PROPERTIES FOLDER "Engine/Asset")
