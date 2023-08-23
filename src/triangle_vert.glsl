#version 330 core

layout(location = 0) in vec2 VERT_IN_POSITION;
layout(location = 1) in vec4 VERT_IN_COLOR;

uniform mat4 PROJECTION;
uniform mat4 VIEW;

out vec4 VERT_OUT_COLOR;

void main() {
    gl_Position = PROJECTION * VIEW * vec4(VERT_IN_POSITION, 0.0f, 1.0f);
    VERT_OUT_COLOR = VERT_IN_COLOR;
}
