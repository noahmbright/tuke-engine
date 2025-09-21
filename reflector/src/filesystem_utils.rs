use std::fs;
use std::path::{Path, PathBuf};

use crate::reflector::{ShaderStage, ShaderToCompile};

const TERMINAL_COLOR_RED: &str = "\x1b[31m";
const TERMINAL_COLOR_RESET: &str = "\x1b[0m";

fn log_error(context: &str, e: &std::io::Error) {
    eprintln!(
        "{}{}: {}, {}{}",
        TERMINAL_COLOR_RED,
        context,
        e,
        e.kind(),
        TERMINAL_COLOR_RESET
    );
}

// recursively get subdirectories of path_string, ignoring gen directories
pub fn recursively_walk_directories(path_string: &str) -> Vec<PathBuf> {
    fn recurse(directory_path: &Path, path_bufs: &mut Vec<PathBuf>) {
        // get all the files and subdirectories out of directory_path
        path_bufs.push(directory_path.to_path_buf());
        let entries_in_subdirectory = match fs::read_dir(directory_path) {
            Ok(entries) => entries,
            Err(e) => {
                log_error(
                    format!("Failed to read directory {}", directory_path.display()).as_str(),
                    &e,
                );
                return;
            }
        };

        // for each file/subdir of directory_path, check if we can access, and then recurse on
        // subdirs
        for entry_result in entries_in_subdirectory {
            let entry = match entry_result {
                Ok(path) => path,
                Err(e) => {
                    log_error(
                        format!("Failed to read a file in {}", directory_path.display()).as_str(),
                        &e,
                    );
                    continue;
                }
            };

            let subdirectory_path = entry.path();
            if subdirectory_path.is_dir() {
                recurse(&subdirectory_path, path_bufs);
            }
        }
    }

    let path = Path::new(path_string);
    let mut path_bufs = Vec::new();
    recurse(path, &mut path_bufs);
    path_bufs
}

pub fn collect_shaders_to_compile(
    subdirectories_of_shaders: &Vec<PathBuf>,
) -> Vec<ShaderToCompile> {
    let mut shaders_to_compile = Vec::new();

    for subdirectory_path in subdirectories_of_shaders {
        // skip e.g. shaders/common/gen
        if subdirectory_path
            .file_name()
            .map(|n| n == "gen")
            .unwrap_or(false)
        {
            println!(
                "Skipping gen directory {}",
                subdirectory_path.to_string_lossy()
            );
            continue;
        }

        // reread the subdirectory
        let entries_in_subdirectory = match fs::read_dir(subdirectory_path) {
            Ok(entries) => entries,
            Err(e) => {
                log_error(
                    format!("Failed to read directory {}", subdirectory_path.display()).as_str(),
                    &e,
                );
                continue;
            }
        };

        for entry in entries_in_subdirectory {
            let entry = match entry {
                Ok(path) => path,
                Err(e) => {
                    log_error(
                        format!("Failed to read a file in {}", subdirectory_path.display())
                            .as_str(),
                        &e,
                    );
                    continue;
                }
            };

            let entry_path = entry.path();
            if entry_path.is_dir() {
                continue;
            }

            println!(
                "found entry {} in nongen subdir {}",
                entry.path().display(),
                subdirectory_path.display()
            );

            // TODO adapt the / for windows to \ or whatever it is
            // turn e,g shaders/common/shader.stage.in into common/shader.stage.in
            let relative_path = match entry_path.strip_prefix("shaders/") {
                Ok(stripped_name) => stripped_name,
                Err(e) => {
                    panic!(
                        "collecting shaders to compile, but subdirectory path {} is not prefixed with shaders/, error: {}",
                        subdirectory_path.display(),
                        e,
                    )
                }
            };

            println!(
                "After stripping shaders/ from {}, the relative path is {}",
                subdirectory_path.to_string_lossy(),
                relative_path.to_string_lossy(),
            );

            // turn common/shader.stage.in into shader.stage.in
            let shader_filename = if let Some(name) = relative_path.file_name() {
                name.to_string_lossy()
            } else {
                continue;
            };

            println!(
                "The filename of relative path {} is {}",
                relative_path.to_string_lossy(),
                shader_filename
            );

            let filename_parts: Vec<&str> = shader_filename.split('.').collect();
            if filename_parts.len() != 3 {
                eprintln!(
                    "Skipping shader {}, expected filename of the form {{shader name}}.{{vert/frag/comp}}.in",
                    shader_filename
                );
                continue;
            }

            let shader_name = filename_parts[0];
            let shader_stage_string = filename_parts[1];
            let shader_filename_extension = filename_parts[2];

            if shader_filename_extension != "in" {
                eprintln!(
                    "Skipping shader {}, extension is not \"in\"",
                    shader_filename
                );
                continue;
            }

            let shader_stage: ShaderStage = match shader_stage_string {
                "vert" => ShaderStage::Vertex,
                "frag" => ShaderStage::Fragment,
                "comp" => ShaderStage::Compute,
                _ => {
                    eprintln!(
                        "Skipping shader {}, Stage is not one of vert, frag, or comp",
                        shader_filename
                    );
                    continue;
                }
            };

            let shader_source = match fs::read_to_string(entry.path()) {
                Ok(source) => source,
                Err(e) => {
                    eprintln!("Failed to read source from file {}: {}", shader_filename, e);
                    continue;
                }
            };

            let shader_to_compile = ShaderToCompile {
                relative_path: relative_path.to_string_lossy().into(),
                name: shader_name.to_string(),
                stage: shader_stage,
                source: shader_source,
            };
            shaders_to_compile.push(shader_to_compile);
        } // iteration over entries_in_subdirectory
    }

    return shaders_to_compile;
}
