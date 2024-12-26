#version 410 core

in vec4 color;
in float tex_id;

out vec4 FragColor;

void main(){
    FragColor = color;
    if (tex_id == 0.0){
        FragColor = vec4(0.3, 0.0, 0.0, 1.0);
    }
    if (tex_id == 1.0){
        FragColor = vec4(0.0, 0.3, 0.0, 1.0);
    }
}
