## CC2Emu
An emulator for color computer 2
- Emulation for the 6809 processor instruction set
- 64KB RAM
- All video modes
    - Artifact colors can be optionally enabled for the high resolution monochrome mode
    - Border color wasn't implemented yet
- Emulation for the WD 1793 diskette drive
    - 4 drives
    - 256 sector size
    - 18 sectors / track
    - 35 tracks
    - The disk image is just a data dump of the disk data
- Cassette emulation
    - .wav file format
    - Read only
- Joystick emulation:
    - Using keyboard arrow keys
    - Using the mouse
    - Using physical joystics (up to 2)
- Sound

## Build
### Dependencies
- SDL 3
- SDL_image 3.0
- libconfuse

Also the source is shipped with the following:
- [Nuklear](https://immediate-mode-ui.github.io/Nuklear/)

### Compile

#### Linux
```
mkdir -p build
cd build
cmake ..
make
```
If the build was successful a single binary file will be generate build/cc2emu.

### Windows
Tested with Visual Studio Community 2024 with CMake and vcpkg support.

Open the project folder with Visual Studio, wait for the packages install complete and then click on start (with or without debug).

## Run
The emulator can be started by executing the following binary:
```
./cc2emu
```
Currently it doesn't support any command line arguments.
By default it will try to load the ROM files from the following paths:
- Basic ROM: ./basic.rom
- Extended Basic ROM: ./extbasic.rom
- Disk Basic ROM: ./dskbasic.rom

If the Basic ROM can't be loaded or an invalid instruction is executed a red border is drawn.

## Configuration
The setting screen can be opened by clicking on the settings icon on the bottom right of the emulator window. It can be used
to load/unload the roms, cartridge, cassette and disks.

A configuration file is generated when the settings are changed from the settings screen. The configuration file have
the following path: $HOME/.local/share/cc2emu/cc2emu/config.ini

## Keyboard emulation
The US qwerty keys are mapped to CC2 keys in addition to the following keys:
- F1: Clear
- F2: SHIFT+0 (Upper keys toggle)
- F5: Temporary enable/disable keyboard and mouse joystick emulation
- F10: Reset
- CTRL+V: Paste (it converts the text in the keyboard into emulated key presses)

## Joysticks
From settings you can configure how each joystick (left and right) are emulated. Each joystick can be configured independently.
### Keyboard joystick emulation
The following keys are used:
- Arrow keys
- Space and enter for joystick button emulation
When the keyboard joystick emulation is enabled, the arrows, space and enter keys aren't sent to the emulated machine.
To enable the keyboard joystick emulation:
- Select keyboard for right or left joystick in the settings page
- Click on the joystick icon, or press F5 to enable the emulation
To temporary disable the emulation click on the joystick icon or press F5.

### Mouse joystick emulation
When the mouse joystick emulation is enabled, the mouse arrow disappear and is captured for the emulation.
To enable the mouse joystick emulation:
- Select mouse for right or left joystick in the settings page
- Click on the joystick icon, or press F5 to enable the emulation
To temporary disable the emulation click on the joystick icon or press F5 or press the mouse right button.

### Using the physical joysticks
You can select Joystick 1 or 2 in the settings page for each of the right or left ones.
