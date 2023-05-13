import os
import subprocess
import pathlib
import shutil
from traceback import print_tb

def GetFilesTimes(file):
    return


if __name__ == '__main__':
    source_dir = os.getcwd() + "\\resources\\shaders"
    target_dir =  os.getcwd() + "\\bin\\resources\\shaders"
    src_files = os.listdir(source_dir)
    trg_files = set(os.listdir(target_dir))
    for file in trg_files:
        if file not in src_files:
            os.remove(target_dir + "\\" + file)


