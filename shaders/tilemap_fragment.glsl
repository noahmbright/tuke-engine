#version 410 core

in vec4 color;
in float tex_id;

out vec4 frag_color;

void main(){
    frag_color = color;
    if (tex_id == 0.0){
        frag_color = vec4(0.3, 0.0, 0.0, 1.0);
    }
    if (tex_id == 1.0){
        frag_color = vec4(0.0, 0.3, 0.0, 1.0);
    }
    //frag_color = vec4(vec3(gl_FragCoord.z), 1.0);
}
