#version 330 core

layout(location = 0) in vec2 VERT_IN_POSITION;

uniform mat4 PROJECTION;
uniform mat4 VIEW;

out vec2 VERT_OUT_POSITION;

void main() {
    vec4 position = PROJECTION * VIEW * vec4(VERT_IN_POSITION, 0.0f, 1.0f);
    gl_Position = position;
    VERT_OUT_POSITION = (position.xy + vec2(1.0f)) / 2.0f;
}
