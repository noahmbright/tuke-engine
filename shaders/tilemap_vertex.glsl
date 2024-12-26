#version 410 core

layout (location = 0) in vec2 a_tex_coords;
layout (location = 1) in vec3 a_pos;
layout (location = 2) in vec4 a_color;
layout (location = 3) in float a_tex_id;

out vec4 color;
out float tex_id;

uniform mat4 mvp;

void main(){
    gl_Position = mvp*vec4(a_pos, 1.0);
    color = a_color;
    tex_id = a_tex_id;
}
