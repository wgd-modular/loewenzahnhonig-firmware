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


# Compilation

This step is only required if you want to modify a firmware or create your own. The firmwares depends on the `libDaisy` and `DaisySP` development libraries by Electro-Smith. These are added as submodules to this repository and are most easily fetched when initially cloning the repository as follows:

```shell
git clone --recursive https://github.com/wgd-modular/loewenzahnhonig-firmware
```

Next you run the provided script to compile the libraries:

```shell
./build_libs.sh
```

Finally, if you want to compile all libraries you can run the provided Python script:

```shell
python ./build_firmwares.py
```

If instead you want to compile one specific firmware, you should change your working directory to that of the firmware and execute the Makefile. For example:

```shell
cd src/vca
make
```

This will compile the VCA firmware and place its output in the `build` subdirectory.


## Contributing

New firmwares for the Löwenzahnhonig module are always very welcome. Feel free to open a PR anytime. Each new firmware should be located in a new sub-folder with the same name as the `.ino` file together with a `README.md` file telling users about the controls and other possibly interesting stuff.

Make-based firmwares can use the small hardware abstraction in `lib/loewy.h`: it initializes the Daisy Seed and the ADC channels of the four pots and two CV inputs, and smooths their readings. `src/vca/vca.cpp` is a minimal example of how to use it.
