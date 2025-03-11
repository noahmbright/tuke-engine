#version 410 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_tex_coords;
uniform mat4 u_mvp;

out vec2 tex_coords;

void main(){
    gl_Position = u_mvp * vec4(a_position, 1.0);
    tex_coords = a_tex_coords;
}
