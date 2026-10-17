#!/usr/bin/env python
#
# Builds the firmwares in src/. Without arguments every firmware is built,
# otherwise only the named ones, e.g. `python build_firmwares.py vca`.
# `--libs` builds libDaisy and DaisySP, `--setup-arduino` installs the
# arduino-cli toolchain used by the .ino-based firmwares, `--clean` removes
# the build output of all firmwares and both libraries.
#
import argparse
import hashlib
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent
SRC = ROOT / "src"
LIBS = [ROOT / "lib" / "libDaisy", ROOT / "lib" / "DaisySP"]
# Half the cores keeps the machine usable while the libraries compile.
JOB_COUNT = max(1, (os.cpu_count() or 2) // 2)
JOBS = "-j{}".format(JOB_COUNT)

# Pinned arduino-cli release, verified by sha256 below, downloaded into the
# project instead of relying on a system install.
ARDUINO_CLI_VERSION = "1.5.1"
ARDUINO_CLI_ASSETS = {
    ("Linux", "x86_64"): (
        "arduino-cli_{v}_Linux_64bit.tar.gz",
        "28a8e119c498a25607821c36cb2dc49e8463941b261a0d99091baa7bc692dd2b",
    ),
    ("Darwin", "x86_64"): (
        "arduino-cli_{v}_macOS_64bit.tar.gz",
        "c982e940027996bea9901050e95fae99c59c1dcfee54beedecaf28141e7bf2e7",
    ),
    ("Darwin", "arm64"): (
        "arduino-cli_{v}_macOS_ARM64.tar.gz",
        "cb952e8c1621c95ef5f1d17831c945e3d0ec5973f89c557a7ec8feb9c4f7d4c9",
    ),
}
ARDUINO_CLI_DIR = ROOT / ".pixi" / "arduino-cli" / ARDUINO_CLI_VERSION
ARDUINO_CLI_BIN = ARDUINO_CLI_DIR / "arduino-cli"
ARDUINO_HOME = ROOT / ".pixi" / "arduino"
STM32_CORE_VERSION = "3.0.0"
DAISYDUINO_VERSION = "1.7.0"
STM32_BOARD_MANAGER_URL = (
    "https://github.com/stm32duino/BoardManagerFiles/raw/main/"
    "package_stmicroelectronics_index.json"
)
# FQBN for the Daisy Seed, as configured by the DaisyDuino board support
# package on top of the stm32duino STM32 core.
ARDUINO_FQBN = (
    "STMicroelectronics:stm32:GenH7:pnum=DAISY_SEED,upload_method=dfuMethod,"
    "xserial=generic,usb=CDCgen,xusb=FS,opt=osstd,dbg=none,rtlib=nano"
)


def arduino_env():
    env = os.environ.copy()
    env["ARDUINO_DIRECTORIES_DATA"] = str(ARDUINO_HOME / "data")
    env["ARDUINO_DIRECTORIES_DOWNLOADS"] = str(ARDUINO_HOME / "downloads")
    env["ARDUINO_DIRECTORIES_USER"] = str(ARDUINO_HOME / "user")
    return env


def makefile_firmwares():
    return sorted(path.parent for path in SRC.glob("*/Makefile"))


def arduino_firmwares():
    return sorted(
        folder for folder in SRC.iterdir()
        if folder.is_dir()
        and (folder / "{}.ino".format(folder.name)).exists()
        and not (folder / "Makefile").exists()
    )


def firmwares():
    return makefile_firmwares() + arduino_firmwares()


def make(folder, *targets):
    return subprocess.call(["make", "-s", *targets], cwd=folder)


def build_libs():
    for folder in LIBS:
        if not (folder / "Makefile").exists():
            print("{} is missing, run: git submodule update --init "
                  "--recursive".format(folder.relative_to(ROOT)))
            return 1
        print("Building: {}".format(folder.name))
        if make(folder, JOBS) != 0:
            return 1
    return 0


def download_arduino_cli():
    if ARDUINO_CLI_BIN.exists():
        return 0
    key = (platform.system(), platform.machine())
    if key not in ARDUINO_CLI_ASSETS:
        print("No pinned arduino-cli release for {} {}".format(*key))
        return 1
    name, checksum = ARDUINO_CLI_ASSETS[key]
    name = name.format(v=ARDUINO_CLI_VERSION)
    url = (
        "https://github.com/arduino/arduino-cli/releases/download/"
        "v{v}/{name}".format(v=ARDUINO_CLI_VERSION, name=name)
    )
    print("Downloading: {}".format(url))
    archive = ARDUINO_CLI_DIR / name
    ARDUINO_CLI_DIR.mkdir(parents=True, exist_ok=True)
    urllib.request.urlretrieve(url, archive)

    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != checksum:
        archive.unlink()
        print("Checksum mismatch for {}: expected {}, got {}".format(
            name, checksum, digest))
        return 1

    with tarfile.open(archive) as tar:
        tar.extract("arduino-cli", ARDUINO_CLI_DIR)
    archive.unlink()
    ARDUINO_CLI_BIN.chmod(0o755)
    return 0


def setup_arduino():
    if not arduino_firmwares():
        return 0
    if download_arduino_cli() != 0:
        return 1
    env = arduino_env()
    cli = str(ARDUINO_CLI_BIN)
    steps = [
        [cli, "config", "init", "--overwrite",
         "--additional-urls", STM32_BOARD_MANAGER_URL],
        [cli, "core", "update-index"],
        [cli, "core", "install",
         "STMicroelectronics:stm32@{}".format(STM32_CORE_VERSION)],
        [cli, "lib", "install",
         "DaisyDuino@{}".format(DAISYDUINO_VERSION)],
    ]
    for step in steps:
        if subprocess.call(step, env=env) != 0:
            return 1
    return 0


def build_makefile_firmware(folder):
    make(folder, "clean")
    return make(folder, JOBS)


def build_arduino_firmware(folder):
    build_dir = folder / "build"
    return subprocess.call(
        [str(ARDUINO_CLI_BIN), "compile", "--fqbn", ARDUINO_FQBN,
         "--jobs", str(JOB_COUNT),
         "--output-dir", str(build_dir), str(folder)],
        env=arduino_env(),
    )


def clean():
    for folder in makefile_firmwares() + LIBS:
        make(folder, "clean")
    for folder in arduino_firmwares():
        shutil.rmtree(folder / "build", ignore_errors=True)
    return 0


def build(names):
    available = {folder.name: folder for folder in firmwares()}
    unknown = [name for name in names if name not in available]
    if unknown:
        print("Unknown firmware: {}".format(", ".join(unknown)))
        print("Available: {}".format(", ".join(available)))
        return 1

    for name in names or available:
        print("Building: {}".format(name))
        folder = available[name]
        if (folder / "Makefile").exists():
            exit_code = build_makefile_firmware(folder)
            binary = folder / "build" / "{}.bin".format(name)
        else:
            exit_code = build_arduino_firmware(folder)
            binary = folder / "build" / "{}.ino.bin".format(name)
        if exit_code != 0:
            return 1
        print("  -> {}".format(binary.relative_to(ROOT)))
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", nargs="*", help="firmwares to build")
    parser.add_argument("--libs", action="store_true",
                         help="build libDaisy and DaisySP")
    parser.add_argument("--setup-arduino", action="store_true",
                         help="install the arduino-cli toolchain")
    parser.add_argument("--clean", action="store_true",
                         help="remove all build output")
    args = parser.parse_args()
    os.nice(10)
    sys.stdout.reconfigure(line_buffering=True)
    if args.clean:
        sys.exit(clean())
    if args.libs:
        sys.exit(build_libs())
    if args.setup_arduino:
        sys.exit(setup_arduino())
    sys.exit(build(args.firmware))


if __name__ == '__main__':
    main()
