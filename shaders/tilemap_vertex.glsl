#version 410 core

layout (location = 0) in vec2 a_tex_coords;
layout (location = 1) in vec3 a_pos;
layout (location = 2) in vec4 a_color;
layout (location = 3) in float a_tex_id;

out vec4 color;
out float tex_id;

layout (std140) uniform u_Matrices{
    mat4 view;
    mat4 projection;
};

void main(){
    gl_Position = projection * view * vec4(a_pos, 1.0);
    color = a_color;
    tex_id = a_tex_id;
}
