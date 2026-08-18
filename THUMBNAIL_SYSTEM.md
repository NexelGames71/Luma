# Luma Engine Thumbnail Rendering System

## Overview

The thumbnail rendering system in Luma Engine is based on Unreal Engine 5's thumbnail architecture, providing extensible asset preview generation and caching.

## Architecture

### Core Components

#### 1. ThumbnailRenderer (Base Class)
- **Location**: `Engine/Asset/include/Luma/Asset/ThumbnailRenderer.h`
- **Purpose**: Abstract base class for all thumbnail renderers
- **Key Methods**:
  - `GetAssetType()`: Returns the asset type this renderer handles
  - `CanRender()`: Checks if renderer can generate thumbnail for given asset
  - `RenderThumbnail()`: Main rendering method
  - `GetPreferredSize()`: Returns default thumbnail size for asset type

#### 2. ThumbnailManager
- **Location**: `Engine/Asset/include/Luma/Asset/ThumbnailRenderer.h`
- **Purpose**: Central manager for thumbnail generation and caching
- **Responsibilities**:
  - Manages renderer registration and lookup
  - Handles thumbnail caching in `Content/Thumbnails/`
  - Provides thumbnail generation API
  - Manages rendering info for each asset type
  - Maintains checkerboard texture for transparent thumbnails
- **Key Methods**:
  - `Initialize()`: Sets up default renderers and resources
  - `RegisterRenderer()`: Registers a renderer for an asset type
  - `GetThumbnail()`: Retrieves or generates thumbnail for an asset
  - `GenerateThumbnail()`: Generates and saves thumbnail to disk
  - `DirtyThumbnail()`: Marks thumbnail for regeneration
  - `ClearCache()`: Clears all cached thumbnails

#### 3. ThumbnailRenderingInfo
- **Location**: `Engine/Asset/include/Luma/Asset/ThumbnailRenderingInfo.h`
- **Purpose**: Configuration for thumbnail rendering per asset type
- **Fields**:
  - `assetType`: The asset type this info applies to
  - `rendererName`: Name of the renderer class
  - `useDefaultPrimitive`: Whether to use a default primitive
  - `defaultPrimitive`: Type of primitive (Cube, Sphere, etc.)
  - `useCheckerboard`: Whether to use checkerboard for transparency
  - `defaultWidth/Height`: Default thumbnail dimensions

### Asset-Specific Renderers

#### TextureThumbnailRenderer
- **Location**: `Engine/Asset/src/TextureThumbnailRenderer.cpp`
- **Capabilities**:
  - Loads `.ltex` files and standard image formats (PNG, JPG, etc.)
  - Resizes to thumbnail dimensions with bilinear interpolation
  - Converts various pixel formats to RGBA8
  - Applies checkerboard background for transparent textures
  - Generates PNG output

#### MeshThumbnailRenderer
- **Location**: `Engine/Asset/src/MeshThumbnailRenderer.cpp`
- **Capabilities**:
  - Loads `.lmesh` files
  - Renders wireframe preview
  - Supports future solid rendering with basic shading
  - Uses orthographic projection for consistent preview
  - Centers camera on mesh bounds

## Thumbnail Pipeline

### Generation Flow

1. **Request**: `ThumbnailManager::GetThumbnail(assetId, nativePath, settings)`
2. **Cache Check**: Checks if thumbnail exists and is up-to-date
3. **Renderer Lookup**: Finds appropriate renderer for asset type
4. **Rendering**: Calls renderer's `RenderThumbnail()` method
5. **Caching**: Saves generated thumbnail to disk
6. **Return**: Returns thumbnail data to caller

### Cache Management

- **Location**: `Content/Thumbnails/`
- **Filename Format**: `{assetId}_thumb.png`
- **Validation**: Compares modification times of source and thumbnail
- **Regeneration**: Automatically regenerated when source is newer

## Integration with Asset Import System

The thumbnail system integrates with the asset import framework:

1. **After Import**: Automatically generates thumbnail after successful import
2. **Reimport**: Regenerates thumbnail when asset is reimported
3. **Dirty Tracking**: Thumbnails are marked dirty when assets are modified

## Comparison with Unreal Engine

### Similarities

- **Renderer Registration**: Both use a registry system for asset-specific renderers
- **Rendering Info**: Configuration per asset type similar to `FThumbnailRenderingInfo`
- **Caching**: Both cache thumbnails on disk for performance
- **Checkerboard**: Both use checkerboard for transparent textures
- **Primitive Types**: Support for default primitives (Cube, Sphere, etc.)

### Differences

- **Simpler API**: Luma uses a more straightforward C++ API vs UE's UObject system
- **No Viewport Pool**: Luma doesn't use viewport pooling (simpler for now)
- **No Texture Pool**: Direct texture loading instead of pooled resources
- **File-based Caching**: Simpler file-based caching vs UE's embedded metadata

## Integration Status

### ✅ Completed Integration

1. **Editor Initialization** - Thumbnail system initialized in `main.cpp`
   - Renderers registered at editor startup
   - Thumbnail manager initialized with default settings
   - Checkerboard texture pre-generated

2. **Asset Import Integration** - Automatic thumbnail generation after import
   - Thumbnails generated in `AssetImportManager::ProcessCompletedJobs()`
   - Thumbnails saved to `Content/Thumbnails/` directory
   - Asset ID-based naming for thumbnails

3. **Content Browser Integration** - Thumbnail display in asset grid
   - Thumbnail cache in `ContentBrowserPanel`
   - Request/retrieve thumbnails for displayed assets
   - Fallback to icons when thumbnails not available
   - Future texture upload support added

4. **CMake Configuration** - All components added to build system
   - ThumbnailRenderingInfo.cpp added to CMakeLists.txt
   - All thumbnail renderers linked into LumaAsset library
   - ContentBrowser links with Luma::Asset for thumbnail support

### 📝 Future Enhancements

To complete the thumbnail system, the following can be added:

1. **GPU Texture Upload** - Convert thumbnail PNG data to GPU textures
   - Upload thumbnail data to renderer
   - Return texture handles for display
   - Cache GPU textures to avoid repeated uploads

2. **Async Thumbnail Generation** - Move generation to background threads
   - Don't block UI during thumbnail generation
   - Show loading state in content browser
   - Progressive thumbnail display

3. **Texture Pool** - Implement thumbnail pool for efficient rendering
   - Limit simultaneous thumbnail generation
   - Prioritize visible thumbnails
   - Reuse thumbnail resources

4. **Solid Mesh Rendering** - Upgrade from wireframe to solid rendering
   - Add proper 3D rendering with lighting
   - Use the Vulkan renderer for mesh previews
   - Add material preview on default primitives

5. **Thumbnail Overlays** - Add asset type overlays
   - Show asset type badge on thumbnails
   - Display status indicators (loading, error, etc.)
   - Add selection indicators

### Planned Features

1. **Solid Mesh Rendering**: Add proper 3D rendering with lighting
2. **Material Preview**: Render materials on default primitives
3. **Animation Preview**: Preview animated meshes
4. **Audio Visualization**: Waveform visualization for audio assets
5. **Viewport Pool**: Add viewport pooling for better performance
6. **HDR Thumbnails**: Support for HDR thumbnail output
7. **Custom Renderers**: Allow users to register custom renderers
8. **Thumbnail Overlays**: Add overlays showing asset type or status

### Rendering Enhancements

1. **PBR Lighting**: Add physically-based lighting for material previews
2. **Environment Maps**: Use HDR environment maps for reflections
3. **Post-Processing**: Add bloom, tone mapping, etc.
4. **Anti-aliasing**: Improve rendering quality
5. **Background Options**: Multiple background options (gradient, solid, checkerboard)

## Usage Example

```cpp
// Initialize thumbnail manager
ThumbnailManager::Get().Initialize();

// Register custom renderer
ThumbnailManager::Get().RegisterRenderer(
    AssetType::Texture,
    std::make_unique<TextureThumbnailRenderer>()
);

// Generate thumbnail for an asset
AssetId assetId = MakeAssetIdFromKey("Content/Textures/test.png");
std::filesystem::path nativePath = "Content/Textures/test.ltex";

ThumbnailSettings settings;
settings.width = 256;
settings.height = 256;
settings.autoRegenerate = true;

auto thumbnail = ThumbnailManager::Get().GetThumbnail(assetId, nativePath, settings);
if (thumbnail) {
    // Use thumbnail data
    std::vector<u8>& pixels = thumbnail->pixels;
    // ... render in UI
}
```

## Performance Considerations

- **Lazy Generation**: Thumbnails generated on-demand
- **Caching**: Disk caching avoids redundant generation
- **Background Generation**: Can be moved to background threads
- **Memory Efficiency**: Thumbnail data loaded only when needed
- **Batch Generation**: Support for batch generation of multiple assets

## Configuration

Thumbnail settings can be configured via:

1. **Default Settings**: Per-asset-type defaults in `ThumbnailRenderingInfo`
2. **Per-Request Settings**: Custom settings per thumbnail request
3. **Global Settings**: Cache directory, default sizes, etc.

## File Formats

- **Output Format**: PNG (lossless, widely supported)
- **Internal Format**: RGBA8 (32-bit per pixel)
- **Future Support**: JPEG (lossy), WebP (modern format), HDR formats

## Error Handling

- **Graceful Degradation**: Returns fallback thumbnail on failure
- **Logging**: Detailed logging for debugging
- **Validation**: Validates asset files before rendering
- **Error Recovery**: Attempts to recover from rendering errors

## References

- **Unreal Engine Source**: `D:\output\UnrealEngine-5.8\Engine\Source\Editor\UnrealEd\Private\ThumbnailRendering\`
- **Unreal Thumbnail Manager**: `Engine\Source\Editor\UnrealEd\Classes\ThumbnailRendering\ThumbnailManager.h`
- **Unreal Texture Renderer**: `Engine\Source\Editor\UnrealEd\Private\ThumbnailRendering\TextureThumbnailRenderer.cpp`
- **Unreal Mesh Renderer**: `Engine\Source\Editor\UnrealEd\Private\ThumbnailRendering\StaticMeshThumbnailRenderer.cpp`
