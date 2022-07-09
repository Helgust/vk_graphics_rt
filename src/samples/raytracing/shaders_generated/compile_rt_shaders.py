import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"
    secret_param = "-DGLSL -I.. -I/home/frol/PROG/kernel_slicer/apps/LiteMath"
    shader_list = ["CastSingleRayMega.comp"]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader), "-DGLSL", "-I.."])

