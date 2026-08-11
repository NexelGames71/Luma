#version 450

// Analytic infinite grid on the Y=0 plane with screen-space (fwidth) anti-
// aliasing. Minor + major cells and colored X/Z axes; alpha fades with radial
// distance from the camera (the caller scales the fade with zoom). Writes true
// plane depth so scene geometry occludes it correctly.

layout(push_constant) uniform GridPush {
    mat4 invViewProj;
    mat4 viewProj;
} pc;

layout(binding = 0) uniform GridUBO {
    vec4 camPos;      // xyz = world camera position
    vec4 minorColor;
    vec4 majorColor;
    vec4 axisX;       // line where z = 0
    vec4 axisZ;       // line where x = 0
    vec4 params;      // x=cellSize, y=majorEvery, z=fadeStart, w=fadeEnd
} u;

layout(location = 0) in vec3 vNear;
layout(location = 1) in vec3 vFar;
layout(location = 0) out vec4 outColor;

// Line coverage for a grid of unit period around `coord`, AA'd by its own
// screen-space derivative.
float lineCoverage(vec2 coord) {
    vec2 d = fwidth(coord);
    vec2 g = abs(fract(coord - 0.5) - 0.5) / max(d, vec2(1e-6));
    return 1.0 - min(min(g.x, g.y), 1.0);
}

// A smooth, screen-space-constant-width axis line at `pos == 0`. `halfWidthPx`
// is the half-width in pixels, so the axes stay a crisp ~2px regardless of zoom.
float axisCoverage(float pos, float halfWidthPx) {
    float dpx = abs(pos) / max(fwidth(pos), 1e-6);
    return 1.0 - smoothstep(halfWidthPx - 0.5, halfWidthPx + 0.5, dpx);
}

void main() {
    // Intersect the near->far segment with the Y=0 plane.
    float t = -vNear.y / (vFar.y - vNear.y);
    if (t <= 0.0 || t >= 1.0) discard;  // plane behind camera / above horizon
    vec3 P = vNear + t * (vFar - vNear);

    // True depth so cubes etc. occlude the grid.
    vec4 clip = pc.viewProj * vec4(P, 1.0);
    gl_FragDepth = clip.z / clip.w;

    float cell = max(u.params.x, 1e-4);
    float major = max(u.params.y, 1.0);
    float minorCov = lineCoverage(P.xz / cell);
    float majorCov = lineCoverage(P.xz / (cell * major));

    // Colored world axes: smooth ~2px lines where x=0 (Z axis) and z=0 (X axis).
    float axisZcov = axisCoverage(P.x, 1.3);
    float axisXcov = axisCoverage(P.z, 1.3);

    // Compose by priority: minor < major < axes.
    vec4 col = vec4(u.minorColor.rgb, minorCov * 0.6);
    if (majorCov > col.a) col = vec4(u.majorColor.rgb, majorCov);
    if (axisZcov > col.a) col = vec4(u.axisZ.rgb, max(axisZcov, 0.85));
    if (axisXcov > col.a) col = vec4(u.axisX.rgb, max(axisXcov, 0.85));

    float dist = length(P.xz - u.camPos.xz);
    col.a *= 1.0 - smoothstep(u.params.z, u.params.w, dist);
    if (col.a < 0.003) discard;
    outColor = col;
}
