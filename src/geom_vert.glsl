#version 330 core

layout(location = 0) in vec2 VERT_IN_POSITION;
layout(location = 1) in vec2 VERT_IN_TRANSLATE;
layout(location = 2) in vec2 VERT_IN_SCALE;
layout(location = 3) in vec4 VERT_IN_COLOR;

uniform mat4 PROJECTION;

out vec4 VERT_OUT_COLOR;

void main() {
    vec2 position = (VERT_IN_SCALE * VERT_IN_POSITION) + VERT_IN_TRANSLATE;
    gl_Position = PROJECTION * vec4(position, 0.0f, 1.0f);
    VERT_OUT_COLOR = VERT_IN_COLOR;
}
