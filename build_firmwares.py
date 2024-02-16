#!/usr/bin/env python
#
# recompiles all Make-based projects within the repository
# excluding any that are located within the specified excluded
# directory list
#
import sys
import os
import subprocess
import pathlib


def main():
    cwd = os.getcwd()
    for file in pathlib.Path("src").rglob("Makefile"):
        folder = file.parents[0]
        os.chdir(folder)
        os.system("echo Building: {}".format(folder))
        subprocess.call("make -s clean", shell=True)
        exit_code = subprocess.call("make -s", shell=True)
        if exit_code != 0:
            os.chdir(cwd)
            sys.exit(1)
        os.chdir(cwd)


if __name__ == '__main__':
    main()

