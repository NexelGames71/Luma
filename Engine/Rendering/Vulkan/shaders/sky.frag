#version 450

// Preetham/Shirley/Smits 1999 analytic daylight sky (ported from Esoterica's
// box3d preetham.glsl), plus a sun disk and a ground half-space. Evaluated per
// pixel for the reconstructed view ray. Output is tonemapped + gamma-encoded for
// the UNORM viewport target.

layout(push_constant) uniform SkyPush {
    mat4 invViewProj;
    vec4 cameraPos;
    vec4 sunDir;   // xyz = dir TO sun (normalized), w = below-horizon fade
    vec4 params;   // x=turbidity, y=sunIntensity, z=cosSunRadius, w=skyIntensity
    vec4 ground;   // rgb = ground color
} pc;

layout(location = 0) in vec4 vWorldHomog;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265358979323846;
const float LUMINANCE_SCALE = 0.06;

float perez(float cosTheta, float cosGamma, float gamma,
            float A, float B, float C, float D, float E) {
    float t1 = 1.0 + A * exp(B / max(cosTheta, 0.01));
    float t2 = 1.0 + C * exp(D * gamma) + E * cosGamma * cosGamma;
    return t1 * t2;
}

vec3 xyYtoXYZ(float x, float y, float Y) {
    float yy = max(y, 1.0e-5);
    return vec3(Y * x / yy, Y, Y * (1.0 - x - y) / yy);
}

vec3 XYZtoLinearSRGB(vec3 c) {
    return vec3(dot(c, vec3(3.2404542, -1.5371385, -0.4985314)),
                dot(c, vec3(-0.9692660, 1.8760108, 0.0415560)),
                dot(c, vec3(0.0556434, -0.2040259, 1.0572252)));
}

vec3 preethamSky(vec3 viewDir, vec3 sunDir, float turbidity) {
    float sunY = clamp(sunDir.y, 0.0, 1.0);
    vec3 v = normalize(vec3(viewDir.x, max(viewDir.y, 0.0) + 0.01, viewDir.z));

    float cosTheta = max(v.y, 0.0);
    float cosGamma = clamp(dot(sunDir, v), -1.0, 1.0);
    float gamma = acos(cosGamma);
    float sunTheta = acos(sunY);

    float T = max(turbidity, 1.0);
    float T2 = T * T;

    float A_Y = 0.1787 * T - 1.4630;
    float B_Y = -0.3554 * T + 0.4275;
    float C_Y = -0.0227 * T + 5.3251;
    float D_Y = 0.1206 * T - 2.5771;
    float E_Y = -0.0670 * T + 0.3703;
    float A_x = -0.0193 * T - 0.2592;
    float B_x = -0.0665 * T + 0.0008;
    float C_x = -0.0004 * T + 0.2125;
    float D_x = -0.0641 * T - 0.8989;
    float E_x = -0.0033 * T + 0.0452;
    float A_y = -0.0167 * T - 0.2608;
    float B_y = -0.0950 * T + 0.0092;
    float C_y = -0.0079 * T + 0.2102;
    float D_y = -0.0441 * T - 1.6537;
    float E_y = -0.0109 * T + 0.0529;

    float ts = sunTheta, ts2 = ts * ts, ts3 = ts2 * ts;
    float x_z = (0.00166 * ts3 - 0.00375 * ts2 + 0.00209 * ts) * T2 +
                (-0.02903 * ts3 + 0.06377 * ts2 - 0.03202 * ts + 0.00394) * T +
                (0.11693 * ts3 - 0.21196 * ts2 + 0.06052 * ts + 0.25886);
    float y_z = (0.00275 * ts3 - 0.00610 * ts2 + 0.00317 * ts) * T2 +
                (-0.04214 * ts3 + 0.08970 * ts2 - 0.04153 * ts + 0.00516) * T +
                (0.15346 * ts3 - 0.26756 * ts2 + 0.06670 * ts + 0.26688);

    float chi = (4.0 / 9.0 - T / 120.0) * (PI - 2.0 * sunTheta);
    float Y_z = (4.0453 * T - 4.9710) * tan(chi) - 0.2155 * T + 2.4192;

    float cosZenGamma = sunY;
    float pY_v = perez(cosTheta, cosGamma, gamma, A_Y, B_Y, C_Y, D_Y, E_Y);
    float px_v = perez(cosTheta, cosGamma, gamma, A_x, B_x, C_x, D_x, E_x);
    float py_v = perez(cosTheta, cosGamma, gamma, A_y, B_y, C_y, D_y, E_y);
    float pY_z = perez(1.0, cosZenGamma, sunTheta, A_Y, B_Y, C_Y, D_Y, E_Y);
    float px_z = perez(1.0, cosZenGamma, sunTheta, A_x, B_x, C_x, D_x, E_x);
    float py_z = perez(1.0, cosZenGamma, sunTheta, A_y, B_y, C_y, D_y, E_y);

    float Y = max(Y_z * pY_v / max(pY_z, 1.0e-5), 0.0);
    float x = x_z * px_v / max(px_z, 1.0e-5);
    float y = y_z * py_v / max(py_z, 1.0e-5);

    return max(XYZtoLinearSRGB(xyYtoXYZ(x, y, Y)), vec3(0.0));
}

void main() {
    vec3 worldPos = vWorldHomog.xyz / vWorldHomog.w;
    vec3 dir = normalize(worldPos - pc.cameraPos.xyz);
    vec3 sunDir = normalize(pc.sunDir.xyz);
    float fade = pc.sunDir.w;

    vec3 sky = preethamSky(dir, sunDir, pc.params.x) * LUMINANCE_SCALE *
               pc.params.w * fade;

    // Ground half-space below the horizon.
    float horizon = smoothstep(-0.015, 0.015, dir.y);
    vec3 color = mix(pc.ground.rgb, sky, horizon);

    // Sun disk (only above the horizon).
    float cosAngle = dot(dir, sunDir);
    float edge = pc.params.z;
    float core = mix(edge, 1.0, 0.5);
    float disk = smoothstep(edge, core, cosAngle);
    vec3 sunColor = vec3(1.0, 0.96, 0.88);
    color += sunColor * disk * pc.params.y * horizon * fade * 4.0;

    // Exposure + a gentle saturation lift so the daylight sky reads vivid.
    color *= 2.4;
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, 1.25);

    // Tonemap (Reinhard) + gamma for the UNORM target.
    color = max(color, vec3(0.0));
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
