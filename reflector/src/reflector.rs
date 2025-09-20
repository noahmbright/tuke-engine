pub enum ShaderStage {
    Vertex,
    Fragment,
    Compute,
}

pub struct ShaderToCompile {
    pub relative_path: String, //path relative to project_root/shaders
    pub name: String,
    pub stage: ShaderStage,
    pub source: String,
}
