// sky_atmosphere.glsl — physically based single-scattering sky, shared by the
// forward sky pass (sky.frag) and the deferred lighting pass
// (deferred_lighting.frag) so both stay pixel-identical.
//
// Modeled on Unreal's SkyAtmosphere parameter set: Rayleigh/Mie scattering
// coefficients, density scale heights, phase anisotropy, ozone layer, sun disk
// and exposure. Scattering is integrated per pixel along the view ray with a
// linearized exponential optical depth (no LUTs), in a camera-relative planet
// frame so it renders correctly at arbitrary world positions.
//
// All lengths are expressed in planet radii (1.0 = 6360 km) so the sphere
// intersection math stays float-precise; the caller-supplied parameters are in
// SI units (meters / per-meter) and converted here.

#ifndef LUMA_SKY_ATMOSPHERE_GLSL
#define LUMA_SKY_ATMOSPHERE_GLSL

const float SKY_PLANET_RADIUS_METERS = 6360000.0;
const float SKY_SCALE = 1.0 / SKY_PLANET_RADIUS_METERS;  // meters -> planet radii
const float SKY_PLANET_RADIUS = 1.0;
const float SKY_ATMOSPHERE_HEIGHT = 60000.0 * SKY_SCALE;  // 60 km shell
const float SKY_TOP_RADIUS = SKY_PLANET_RADIUS + SKY_ATMOSPHERE_HEIGHT;
const float SKY_PI = 3.14159265358979323846;
const int   SKY_MARCH_SAMPLES = 16;

// Scales the dimensionless sun intensity into radiance units that produce a
// pleasing sky with the default coefficients (tune to taste).
const float SKY_SUN_RADIANCE = 3.0;

// Ozone absorption coefficients per meter at the layer's peak (Chappuis band).
const vec3 SKY_OZONE_ABSORPTION = vec3(0.650e-6, 1.881e-6, 0.085e-6);

struct AtmosphereParams {
    vec4 sunDirection;   // xyz dir TO sun (normalized), w = below-horizon fade
    vec4 sunColor;       // rgb sun color, w = sun intensity (lighting scale)
    vec4 sunDisk;        // x = angular diameter (degrees), y = disk intensity
    vec4 rayleigh;       // rgb rayleigh scattering coeff (1/m), w = scale height (m)
    vec4 mie;            // x = scattering (1/m), y = absorption (1/m),
                         // z = scale height (m), w = phase anisotropy g
    vec4 ozoneSky;       // x = ozone scale, y = sky intensity, z = saturation,
                         // w = exposure
    vec4 tint;           // rgb sky tint
    vec4 ground;         // rgb ground color (below the horizon)
};

// ---- Phase functions --------------------------------------------------------

float SkyPhaseRayleigh(float cosTheta) {
    return 3.0 / (16.0 * SKY_PI) * (1.0 + cosTheta * cosTheta);
}

float SkyPhaseHG(float cosTheta, float g) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * SKY_PI * pow(max(denom, 1e-4), 1.5));
}

// Ozone density: soft Gaussian layer peaking ~25 km above the surface (h in
// planet-radius units).
float SkyOzoneDensity(float h) {
    float x = (h - 25000.0 * SKY_SCALE) / (15000.0 * SKY_SCALE);
    return exp(-x * x);
}

// ---- Geometry ---------------------------------------------------------------

// Forward distance along `dir` from a point at height h above the planet (up =
// unit outward normal at that point) to the sphere of the given radius
// (planet-radius units). Returns -1 when the ray misses the sphere.
float SkyDistToRadius(float h, vec3 up, vec3 dir, float radius) {
    float r = SKY_PLANET_RADIUS + h;
    float b = r * dot(up, dir);
    float c = r * r - radius * radius;
    float disc = b * b - c;
    return disc > 0.0 ? (-b + sqrt(disc)) : -1.0;
}

// ---- Optical depth ----------------------------------------------------------

// Optical depth of an exponential density exp(-h/H) over a segment of length d
// starting at height h, where mu = dot(rayDir, up) is the upward cosine. The
// path is linearized (density ~= exp(-(h + s*mu)/H)), which integrates exactly
// for both upward (mu > 0) and downward (mu < 0) segments. h, H and d are in
// planet-radius units.
float SkyOpticalDepth(float h, float mu, float scaleHeight, float d) {
    float base = exp(-h / scaleHeight);
    if (abs(mu) < 1e-4) return base * d;
    float k = scaleHeight / mu;
    return base * k * (1.0 - exp(-d * mu / scaleHeight));
}

// Optical depth from a sample to the top of the atmosphere along `dir`. Rays
// pointing below the local horizon are blocked by the opaque planet.
float SkySunOpticalDepth(float h, vec3 up, vec3 dir, float scaleHeight) {
    float mu = dot(dir, up);
    if (mu <= 0.0) return 1e9;
    float d = SkyDistToRadius(h, up, dir, SKY_TOP_RADIUS);
    if (d <= 0.0) return 1e9;
    return SkyOpticalDepth(h, mu, scaleHeight, d);
}

// ---- Single-scattering integration -----------------------------------------

// Integrates in-scattered sunlight along the view ray. `camPos` is the world
// camera position; the planet is placed directly below the camera so the sky
// is valid anywhere in the world. Returns linear HDR sky color.
vec3 SkyAtmosphereScatter(vec3 camPos, vec3 rd, AtmosphereParams p) {
    // Local planet frame: planet center on the Y axis below the camera.
    float camHeight = (max(camPos.y, 0.0) + 1.0) * SKY_SCALE;
    vec3 ro = vec3(0.0, SKY_PLANET_RADIUS + camHeight, 0.0);
    vec3 up0 = vec3(0.0, 1.0, 0.0);

    float tMax = SkyDistToRadius(camHeight, up0, rd, SKY_TOP_RADIUS);
    if (tMax <= 0.0) return vec3(0.0);  // ray misses the atmosphere

    // Below-horizon rays hit the planet; stop there (the ground blend in
    // RenderSkyColor handles those pixels).
    float tGround = SkyDistToRadius(camHeight, up0, rd, SKY_PLANET_RADIUS);
    if (tGround > 0.0 && tGround < tMax) tMax = tGround;

    // Convert SI parameters to planet-radius units.
    vec3 betaR = p.rayleigh.rgb / SKY_SCALE;   // per scaled-length
    float Hr = p.rayleigh.w * SKY_SCALE;
    float betaM = p.mie.x / SKY_SCALE;
    float betaME = (p.mie.x + p.mie.y) / SKY_SCALE;
    float Hm = p.mie.z * SKY_SCALE;
    float g = p.mie.w;
    float ozoneScale = p.ozoneSky.x;
    vec3 ozoneAbs = SKY_OZONE_ABSORPTION / SKY_SCALE;

    vec3 sunDir = normalize(p.sunDirection.xyz);
    vec3 sunLight = p.sunColor.rgb * p.sunColor.w * SKY_SUN_RADIANCE;

    float cosTheta = dot(rd, sunDir);
    float phaseR = SkyPhaseRayleigh(cosTheta);
    float phaseM = SkyPhaseHG(cosTheta, g);

    float dt = tMax / float(SKY_MARCH_SAMPLES);
    vec3 inscatter = vec3(0.0);
    vec3 transmittance = vec3(1.0);
    float t = dt * 0.5;

    for (int i = 0; i < SKY_MARCH_SAMPLES; ++i, t += dt) {
        vec3 pos = ro + rd * t;
        float h = length(pos) - SKY_PLANET_RADIUS;
        if (h < 0.0) break;  // inside the planet

        vec3 up = pos / length(pos);

        float densityR = exp(-h / Hr);
        float densityM = exp(-h / Hm);
        float densityO = SkyOzoneDensity(h) * ozoneScale;

        // Transmittance sample -> sun (per channel).
        vec3 sunT;
        float muSun = dot(sunDir, up);
        if (muSun <= 0.0) {
            sunT = vec3(0.0);
        } else {
            float odR = SkySunOpticalDepth(h, up, sunDir, Hr);
            float odM = SkySunOpticalDepth(h, up, sunDir, Hm);
            float dSun = SkyDistToRadius(h, up, sunDir, SKY_TOP_RADIUS);
            float odO = densityO * max(dSun, 0.0);
            sunT = exp(-(odR * betaR + vec3(betaME * odM) +
                         vec3(ozoneAbs * odO)));
        }

        // In-scattered light for this segment, attenuated by the transmittance
        // from the camera to the sample.
        vec3 segmentScatter =
            (densityR * betaR * phaseR + vec3(betaM * densityM * phaseM)) *
            sunLight;
        inscatter += segmentScatter * sunT * transmittance * dt;

        // Accumulate camera -> sample transmittance (extinction per channel).
        vec3 extinction =
            densityR * betaR + vec3(betaME * densityM) + vec3(ozoneAbs * densityO);
        transmittance *= exp(-extinction * dt);
    }

    return inscatter;
}

// ---- Full sky rendering -----------------------------------------------------

// Computes the final tonemapped + gamma-encoded sky color for a world-space
// point on the far plane: single scattering, ground half-space, sun disk, tint,
// exposure and saturation. Safe to call for any fragment.
vec3 RenderSkyColor(vec3 worldPos, vec3 camPos, AtmosphereParams p) {
    vec3 rd = normalize(worldPos - camPos);
    vec3 sunDir = normalize(p.sunDirection.xyz);
    float fade = p.sunDirection.w;

    vec3 color = SkyAtmosphereScatter(camPos, rd, p);

    // Dusk fade: as the sun dips below the horizon the scattered sunlight goes
    // to zero, which would leave a harsh black sky. Blend toward a deep
    // twilight blue (scaled by sky intensity) so the sky dims smoothly.
    vec3 nightSky = vec3(0.006, 0.010, 0.028) * p.ozoneSky.y;
    // A touch brighter near the horizon (moon/city haze) so the dome reads
    // naturally at night instead of being uniformly black.
    nightSky *= mix(1.35, 1.0, smoothstep(-0.10, 0.10, rd.y));
    color = mix(nightSky, color, fade);

    // Ground half-space below the horizon (planet surface is opaque).
    float horizon = smoothstep(-0.015, 0.015, rd.y);
    color = mix(p.ground.rgb, color, horizon);

    // Sun disk (only above the horizon).
    float radius = p.sunDisk.x * 0.5 * (SKY_PI / 180.0);
    float edge = cos(radius);
    float core = mix(edge, 1.0, 0.5);
    float disk = smoothstep(edge, core, dot(rd, sunDir));
    vec3 diskColor = vec3(1.0, 0.96, 0.88);
    color += diskColor * disk * p.sunDisk.y * p.sunColor.w * fade * 4.0;

    // Sky tint, exposure and saturation (post).
    color *= p.tint.rgb;
    color *= p.ozoneSky.w;
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, p.ozoneSky.z);
    color = max(color, vec3(0.0));

    // Tonemap (Reinhard) + gamma for the UNORM target.
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    return color;
}

#endif  // LUMA_SKY_ATMOSPHERE_GLSL
