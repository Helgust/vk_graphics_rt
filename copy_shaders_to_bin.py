import os
import subprocess
import pathlib
import shutil
from traceback import print_tb

if __name__ == '__main__':
    source_dir = os.getcwd() + "\\resources\\shaders"
    target_dir =  os.getcwd() + "\\bin\\resources\\shaders"

    dict_target_files = dict()
    for file in os.listdir(target_dir):
        dict_target_files[file] = os.path.getmtime(target_dir+"\\"+file)
    for file in os.listdir(source_dir):
        if(file not in dict_target_files.keys() or
            os.path.getmtime(source_dir+"\\"+file) != dict_target_files[file]):
            src_file = source_dir + "\\"+file
            trg_file = target_dir + "\\"+file
            shutil.copyfile(src_file, trg_file)



