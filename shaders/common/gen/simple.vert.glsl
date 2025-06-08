#version 410 core

 in vec3 a_pos;
 in vec3 a_normal;

 out vec3 norm;

void main(){
    norm = a_normal;
    gl_Position = vec4(a_pos, 1.0);
}
