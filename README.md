# loewenzahnhonig-firmware
A repository to collect different firmwares for the daisy-based löwenzahnhonig module.

<img src="media/DSC07012.JPG" height="500">

### Setup development environment

To compile the firmwares contained in this repository and possibly make changes to them you need to install the [arduino ide](https://www.arduino.cc/en/software). You'll also need to add daisy support which is very well explained in a [video by electro-smith](https://www.youtube.com/watch?v=UyQWK8JFTps) themselves.

#### Controls

One side of the modules front panel has a generic labelling. This table contains a mapping of which control is associated with which pin of the daisy seed.

| **Control**  | **Daisy Pin**   |
|------------- | --------------- |
| P1           | A0              |
| P2           | A1              |
| P3           | A2              |
| P4           | A3              |
| CV1          | A4              |
| CV2          | A5              |


### Contributing

New firmwares for the löwenzahnhonig module are always very welcome. Feel free to open a PR anytime. Each new firmware should be located in a new sub-folder with the same name as the `.ino` file together with a `README.md` file telling users about the controls and other possibly interesting stuff.
