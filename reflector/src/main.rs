mod filesystem_utils;
mod lexer;
mod reflector;

fn main() -> std::io::Result<()> {
    let shaders_dir_relative_path = "shaders";
    let path_bufs = filesystem_utils::recursively_walk_directories(&shaders_dir_relative_path);
    for path_buf in &path_bufs {
        println!("{:#?}", path_buf);
    }

    let _shaders_to_compile = filesystem_utils::collect_shaders_to_compile(&path_bufs);

    Ok(())
}
