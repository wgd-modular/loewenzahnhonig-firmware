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

# FQBN for the Daisy Seed, as configured by the DaisyDuino board support
# package on top of the stm32duino STM32 core.
ARDUINO_FQBN = (
    "STMicroelectronics:stm32:GenH7:pnum=DAISY_SEED,upload_method=dfuMethod,"
    "xserial=generic,usb=CDCgen,xusb=FS,opt=osstd,dbg=none,rtlib=nano"
)


def build_makefiles():
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


def build_arduino_sketches():
    for folder in sorted(pathlib.Path("src").iterdir()):
        sketch = folder / "{}.ino".format(folder.name)
        if (folder / "Makefile").exists() or not sketch.exists():
            continue
        os.system("echo Building: {}".format(folder))
        exit_code = subprocess.call(
            "arduino-cli compile --fqbn {} --output-dir {} {}".format(
                ARDUINO_FQBN, folder / "build", folder
            ),
            shell=True,
        )
        if exit_code != 0:
            sys.exit(1)


def main():
    build_makefiles()
    build_arduino_sketches()


if __name__ == '__main__':
    main()

