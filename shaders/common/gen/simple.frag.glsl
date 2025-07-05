#version 410 core

in vec3 norm;
out vec4 FragColor;

uniform UniformBufferObject{
    float x;
} ubo;

void main(){
    FragColor = vec4(0.0, 0.0, 0.3, ubo.x);
}
