#pragma once

#include "Luma/Core/Types.h"
#include "Luma/Asset/AssetType.h"
#include <string>

namespace Luma {

/**
 * Types of primitives for drawing thumbnails of resources
 */
enum class ThumbnailPrimType : u32 {
    None = 0,
    Sphere,
    Cube,
    Plane,
    Cylinder,
    ShaderBall,
    MAX
};

/**
 * Holds the settings for an asset type that needs a thumbnail renderer
 */
struct ThumbnailRenderingInfo {
    /**
     * The asset type this thumbnail is for
     */
    AssetType assetType = AssetType::Unknown;
    
    /**
     * The name of the renderer class
     */
    std::string rendererName;
    
    /**
     * Should we use a default primitive when rendering?
     */
    bool useDefaultPrimitive = false;
    
    /**
     * The default primitive type to use
     */
    ThumbnailPrimType defaultPrimitive = ThumbnailPrimType::Cube;
    
    /**
     * Whether to use a checkerboard background for transparent textures
     */
    bool useCheckerboard = true;
    
    /**
     * Default thumbnail size
     */
    u32 defaultWidth = 128;
    u32 defaultHeight = 128;
    
    ThumbnailRenderingInfo() = default;
};

} // namespace Luma