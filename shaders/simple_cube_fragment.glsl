#version 410 core

in vec3 color;
//in vec2 tex_coords;
out vec4 FragColor;

void main(){
    FragColor = vec4(color, 1.0);
}
