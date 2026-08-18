#pragma once

#include "Luma/Math/Math.h"
#include "Luma/RHI/Renderer.h"

namespace Luma {
namespace Rendering {

// std140 layout matching `struct AtmosphereParams` in sky_atmosphere.glsl.
// 8 x vec4 = 128 bytes, aligned to 16 bytes.
struct AtmosphereParams {
    f32 sunDirection[4];  // xyz dir TO sun (normalized), w = below-horizon fade
    f32 sunColor[4];      // rgb sun color, w = sun intensity (lighting scale)
    f32 sunDisk[4];       // x = angular diameter (deg), y = disk intensity
    f32 rayleigh[4];      // rgb rayleigh scattering coeff (1/m), w = scale height (m)
    f32 mie[4];           // x = scattering (1/m), y = absorption (1/m),
                          // z = scale height (m), w = phase anisotropy g
    f32 ozoneSky[4];      // x = ozone scale, y = sky intensity, z = saturation,
                          // w = exposure
    f32 tint[4];          // rgb sky tint
    f32 ground[4];        // rgb ground color (below the horizon)
};

// Fills the UBO-ready params from the RHI SkyParams. Shared by the forward sky
// pass (VulkanSkyPass) and the deferred lighting pass (VulkanDeferredRenderer)
// so both render the same sky.
inline void FillAtmosphereParams(AtmosphereParams& out, const SkyParams& sky) {
    Math::Vec3 sunDir = Math::Normalize(sky.sunDirection);
    // Fade the analytic sky out as the sun drops below the horizon (the
    // physical model handles occlusion, but the sun disk still needs the fade).
    f32 fade = (sunDir.y - (-0.02f)) / (0.12f - (-0.02f));
    fade = fade < 0.0f ? 0.0f : (fade > 1.0f ? 1.0f : fade);

    out.sunDirection[0] = sunDir.x;
    out.sunDirection[1] = sunDir.y;
    out.sunDirection[2] = sunDir.z;
    out.sunDirection[3] = fade;

    out.sunColor[0] = sky.sunColor.x;
    out.sunColor[1] = sky.sunColor.y;
    out.sunColor[2] = sky.sunColor.z;
    out.sunColor[3] = sky.sunIntensity;

    out.sunDisk[0] = sky.sunDiskSizeDeg;
    out.sunDisk[1] = sky.sunDiskIntensity;
    out.sunDisk[2] = 0.0f;
    out.sunDisk[3] = 0.0f;

    out.rayleigh[0] = sky.rayleighScattering.x;
    out.rayleigh[1] = sky.rayleighScattering.y;
    out.rayleigh[2] = sky.rayleighScattering.z;
    out.rayleigh[3] = sky.rayleighScaleHeight;

    out.mie[0] = sky.mieScattering;
    out.mie[1] = sky.mieAbsorption;
    out.mie[2] = sky.mieScaleHeight;
    out.mie[3] = sky.mieAnisotropy;

    out.ozoneSky[0] = sky.ozoneScale;
    out.ozoneSky[1] = sky.skyIntensity;
    out.ozoneSky[2] = sky.saturation;
    out.ozoneSky[3] = sky.exposure;

    out.tint[0] = sky.skyTint.x;
    out.tint[1] = sky.skyTint.y;
    out.tint[2] = sky.skyTint.z;
    out.tint[3] = 1.0f;

    out.ground[0] = sky.groundColor.x;
    out.ground[1] = sky.groundColor.y;
    out.ground[2] = sky.groundColor.z;
    out.ground[3] = 1.0f;
}

}  // namespace Rendering
}  // namespace Luma
