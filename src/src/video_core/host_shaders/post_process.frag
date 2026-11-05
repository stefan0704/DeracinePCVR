// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D texSampler;

layout (push_constant) uniform settings {
    float gamma;
    bool hdr;
    // Above 0, the picture is sharpened by so much (up to 1): for one that is shown larger
    // than it was drawn, which blurs it.
    float sharpen;
    // The target encodes what it is given for display by itself (an sRGB image): it has to be
    // given linear light, or the picture would be encoded twice.
    bool linear_out;
    // The texture gives back what it holds, already encoded for display (an sRGB image read
    // through a UNORM view, gamma 1): nothing to encode.
    bool encoded_in;
    // Within centre_radius of the middle (a share of the way to the edge, on both axes) the
    // picture is sharpened by sharpen_centre instead: where FSR 1 enlarged the title's scene.
    float sharpen_centre;
    float centre_radius;
    bool antialias;
} pp;

const float cutoff = 0.0031308, a = 1.055, b = 0.055, d = 12.92;
vec3 gamma(vec3 rgb) {
    return mix(
        a * pow(rgb, vec3(1.0 / (2.4 + 1.0 - pp.gamma))) - b,
        d * rgb / pp.gamma,
        lessThan(rgb, vec3(cutoff))
    );
}

// What goes to the display for the picture at a place.
vec3 shown(vec2 at) {
    vec3 rgb = textureLod(texSampler, at, 0.0).rgb;
    return pp.hdr || pp.encoded_in ? rgb : gamma(rgb);
}

float Brightness(vec3 rgb) {
    const float light = min(dot(max(rgb, vec3(0.0)), vec3(0.299, 0.587, 0.114)), 65000.0);
    return sqrt(light / (1.0 + light));
}

vec4 SampleFXAA(vec2 at) {
    const vec2 texel = 1.0 / vec2(textureSize(texSampler, 0));
    const vec4 centre = textureLod(texSampler, at, 0.0);
    const float m = Brightness(centre.rgb);
    const float n = Brightness(textureLodOffset(texSampler, at, 0.0, ivec2(0, -1)).rgb);
    const float s = Brightness(textureLodOffset(texSampler, at, 0.0, ivec2(0, 1)).rgb);
    const float w = Brightness(textureLodOffset(texSampler, at, 0.0, ivec2(-1, 0)).rgb);
    const float e = Brightness(textureLodOffset(texSampler, at, 0.0, ivec2(1, 0)).rgb);
    const float brightest = max(max(max(n, s), max(w, e)), m);
    const float darkest = min(min(min(n, s), min(w, e)), m);
    const float contrast = brightest - darkest;
    if (contrast < max(0.0625, brightest * 0.166)) {
        return centre;
    }
    const float nw = Brightness(textureLodOffset(texSampler, at, 0.0, ivec2(-1, -1)).rgb);
    const float ne = Brightness(textureLodOffset(texSampler, at, 0.0, ivec2(1, -1)).rgb);
    const float sw = Brightness(textureLodOffset(texSampler, at, 0.0, ivec2(-1, 1)).rgb);
    const float se = Brightness(textureLodOffset(texSampler, at, 0.0, ivec2(1, 1)).rgb);

    const float across_rows = abs(nw + sw - 2.0 * w) + 2.0 * abs(n + s - 2.0 * m) + abs(ne + se - 2.0 * e);
    const float across_columns = abs(nw + ne - 2.0 * n) + 2.0 * abs(w + e - 2.0 * m) + abs(sw + se - 2.0 * s);
    const bool along_rows = across_rows >= across_columns;

    const float before = along_rows ? n : w;
    const float after = along_rows ? s : e;
    const float rise_before = abs(before - m);
    const float rise_after = abs(after - m);
    const bool steeper_before = rise_before >= rise_after;
    const float rise = max(rise_before, rise_after) * 0.25;
    const float side = steeper_before ? -1.0 : 1.0;
    const float middle = 0.5 * ((steeper_before ? before : after) + m);
    const bool centre_below = m < middle;

    const vec2 across = along_rows ? vec2(0.0, texel.y) : vec2(texel.x, 0.0);
    const vec2 along = along_rows ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);
    const vec2 start = at + across * (side * 0.5);
    float back = 0.0;
    float forth = 0.0;
    float end_back = 0.0;
    float end_forth = 0.0;
    bool done_back = false;
    bool done_forth = false;
    const float StepLength[7] = float[7](1.0, 1.5, 2.0, 2.0, 2.0, 4.0, 8.0);
    for (int i = 0; i < 7; ++i) {
        if (!done_back) {
            back += StepLength[i];
            end_back = Brightness(textureLod(texSampler, start - along * back, 0.0).rgb) - middle;
            done_back = abs(end_back) >= rise;
        }
        if (!done_forth) {
            forth += StepLength[i];
            end_forth = Brightness(textureLod(texSampler, start + along * forth, 0.0).rgb) - middle;
            done_forth = abs(end_forth) >= rise;
        }
        if (done_back && done_forth) break;
    }
    const bool nearer_back = back < forth;
    const float nearer = min(back, forth);
    const bool sticks_out = ((nearer_back ? end_back : end_forth) < 0.0) != centre_below;
    const float by_edge = sticks_out ? 0.5 - nearer / (back + forth) : 0.0;

    const float around = (2.0 * (n + s + w + e) + nw + ne + sw + se) / 12.0;
    float speck = clamp(abs(around - m) / contrast, 0.0, 1.0);
    speck = speck * speck * (3.0 - 2.0 * speck);
    const float share = max(by_edge, speck * speck * 0.5);

    const vec4 other = textureLod(texSampler, at + across * side, 0.0);
    const float weight_centre = 1.0 - share;
    const float weight_other = share;
    return centre * weight_centre + other * weight_other;
}

void main() {
    vec4 color_linear = pp.antialias ? SampleFXAA(uv) : texture(texSampler, uv);
    vec3 here = pp.hdr || pp.encoded_in ? color_linear.rgb : gamma(color_linear.rgb);
    vec2 from_middle = uv * 2.0 - 1.0;
    float sharpen = dot(from_middle, from_middle) < pp.centre_radius * pp.centre_radius
                        ? pp.sharpen_centre
                        : pp.sharpen;
    if (sharpen > 0.0) {
        // Contrast adaptive sharpening: a pixel is pushed away from the four next to it, the
        // more the less they differ already, and never beyond black or white.
        // Sample at output pixel pitch (from screen derivatives) to prevent low-res staircase amplification.
        vec2 out_texel = vec2(abs(dFdx(uv).x), abs(dFdy(uv).y));
        vec2 texel = out_texel.x > 0.0 ? out_texel : 1.0 / vec2(textureSize(texSampler, 0));
        vec3 above = shown(uv - vec2(0.0, texel.y));
        vec3 below = shown(uv + vec2(0.0, texel.y));
        vec3 left = shown(uv - vec2(texel.x, 0.0));
        vec3 right = shown(uv + vec2(texel.x, 0.0));
        vec3 darkest = min(min(min(above, below), min(left, right)), here);
        vec3 brightest = max(max(max(above, below), max(left, right)), here);
        vec3 room = sqrt(clamp(min(darkest, 1.0 - brightest) / max(brightest, vec3(1e-5)),
                               0.0, 1.0));
        vec3 weight = room * (-1.0 / mix(8.0, 5.0, clamp(sharpen, 0.0, 1.0)));
        here = clamp(((above + below + left + right) * weight + here) / (1.0 + 4.0 * weight),
                     0.0, 1.0);
    }
    if (pp.linear_out && !pp.hdr) {
        // Back to linear light, by the curve the target encodes with.
        here = mix(pow((here + 0.055) / 1.055, vec3(2.4)), here / 12.92,
                   lessThanEqual(here, vec3(0.04045)));
    }
    color = vec4(here, color_linear.a);
}
