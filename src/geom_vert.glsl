#version 330 core

layout(location = 0) in vec2 VERT_IN_POSITION;
layout(location = 1) in vec2 VERT_IN_TRANSLATE;
layout(location = 2) in vec2 VERT_IN_SCALE;
layout(location = 3) in vec4 VERT_IN_COLOR;
layout(location = 4) in float VERT_IN_ROTATE_RADIANS;

uniform mat4 PROJECTION;
uniform mat4 VIEW;

out vec4 VERT_OUT_COLOR;

void main() {
    float s = sin(VERT_IN_ROTATE_RADIANS);
    float c = cos(VERT_IN_ROTATE_RADIANS);

    vec2 position =
        (VERT_IN_POSITION * VERT_IN_SCALE) - (VERT_IN_SCALE / 2.0f);
    position = vec2((position.x * c) + (position.y * s),
                    (position.x * -s) + (position.y * c));
    position += (VERT_IN_SCALE / 2.0f) + VERT_IN_TRANSLATE;

    gl_Position = PROJECTION * VIEW * vec4(position, 0.0f, 1.0f);
    VERT_OUT_COLOR = VERT_IN_COLOR;
}
