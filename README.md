# loewenzahnhonig-firmware
A repository to collect different firmwares for the daisy-based Löwenzahnhonig module.

<img src="media/DSC07012.JPG" height="500">


## Flash your module

Flash your module straight from the browser with the [Löwenzahnhonig web flasher](https://wgd-modular.github.io/loewenzahnhonig-firmware/). Open it in Google Chrome or Edge, connect the Daisy Seed over USB, hold BOOT and tap RESET to enter the bootloader, then pick a firmware and press flash. It lists every firmware of the latest release together with its controls, and can also flash a local `.bin` you built yourself.

Prefer to grab the raw binaries? Every firmware is pre-compiled and attached to the [latest release](https://github.com/wgd-modular/loewenzahnhonig-firmware/releases/latest).


## Controls

One side of the modules front panel has a generic labelling. This table contains a mapping of which control is associated with which pin of the daisy seed.

| **Control**  | **Daisy Pin**   |
|------------- | --------------- |
| P1           | A0              |
| P2           | A1              |
| P3           | A2              |
| P4           | A3              |
| CV1          | A4              |
| CV2          | A5              |

The CV inputs take -5 V to +5 V. The firmwares read them as values from -1 (-5 V) through 0 (0 V) to 1 (+5 V); `lib/loewy.h` has the details.


# Compilation

This step is only required if you want to modify a firmware or create your own. The firmwares depend on the `libDaisy` and `DaisySP` development libraries by Electro-Smith, and some depend on `lib/eurorack` or `lib/cloudseed-daisy`. These are added as submodules to this repository and are most easily fetched when initially cloning the repository as follows:

```shell
git clone --recursive https://github.com/wgd-modular/loewenzahnhonig-firmware
```

If you cloned without `--recursive`, fetch them afterwards with `git submodule update --init --recursive`.

Everything else, the ARM GCC toolchain, `make`, Python and the arduino-cli toolchain the `.ino`-based firmwares need, is managed by [pixi](https://pixi.sh), so nothing has to be installed system-wide apart from pixi itself. It works the same on Linux and macOS (Intel and Apple Silicon); on Windows use WSL. To compile all firmwares run:

```shell
pixi run build
```

The first run sets up the toolchains and compiles both libraries, later runs reuse them. To compile one specific firmware, pass its name:

```shell
pixi run build vca
```

This places the firmware in `src/vca/build/vca.bin`, ready for the web flasher. `pixi run build-libs` only rebuilds the libraries, and `pixi run clean` removes all build output again.

When working on a single Make-based firmware it can be handy to drop into the environment with `pixi shell` and call `make` inside the firmware's folder directly.


## Contributing

New firmwares for the Löwenzahnhonig module are always very welcome. Feel free to open a PR anytime. Each new firmware should be located in a new sub-folder with the same name as the `.ino` file together with a `README.md` file telling users about the controls and other possibly interesting stuff.

Make-based firmwares can use the small hardware abstraction in `lib/loewy.h`: it initializes the Daisy Seed and the ADC channels of the four pots and two CV inputs, and smooths their readings. `src/vca/vca.cpp` is a minimal example of how to use it. It also applies the Seed's system settings from the `lib/cloudseed-daisy` submodule (the 480 MHz clock where the silicon allows it, the SDRAM's refresh count and timings, the cache policies), which is why every firmware needs the submodules initialized.

The Cloud Seed firmware (`src/cloudseed`) is the reference application of the [cloudseed-daisy](https://github.com/benjaminvdb/cloudseed-daisy) library, a Cloud Seed reverb for any Daisy Seed; the library is a submodule in `lib/cloudseed-daisy`.
