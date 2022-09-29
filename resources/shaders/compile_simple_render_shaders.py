import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = ["simple.vert", "mrt.frag", "resolve.frag",
     "resolve.vert", "omnishadow.frag", "omnishadow.vert", 
     "taa.vert", "taa.frag", "result.vert", "result.frag", 
     "median.frag", "blur.frag", "kuwahara.frag"]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader)])

