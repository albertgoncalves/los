#version 330 core

layout(location = 0) out vec4 FRAG_OUT_COLOR;

in vec2 VERT_OUT_POSITION;

uniform sampler2DMS TEXTURE;
uniform sampler2DMS MASK;

uniform float BLEND;
uniform int   MULTISAMPLES_SHADOW;

// NOTE: See `https://stackoverflow.com/a/42882506`.
vec4 texture_multisample(sampler2DMS sampler, ivec2 coord) {
    vec4 color = vec4(0.0);
    for (int i = 0; i < MULTISAMPLES_SHADOW; ++i) {
        color += texelFetch(sampler, coord, i);
    }
    color /= float(MULTISAMPLES_SHADOW);
    return color;
}

void main() {
    // NOTE: Since both textures *need* to be the same size, we can base the
    // `position` off of either of their `textureSize` values.
    ivec2 position = ivec2(VERT_OUT_POSITION * textureSize(TEXTURE));
    FRAG_OUT_COLOR = vec4(texture_multisample(TEXTURE, position).rgb,
                          mix(1.0f, texture_multisample(MASK, position).a, BLEND));
}
