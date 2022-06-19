# CAT B100 tools

Some little Linux tools implementing various features of the Windows XP PhoneSuite CAT B100 Ver 5.4.1 software.

## Getting the source code

Retrieve the source code and the submodule dependency with this command :
```
git clone --recurse-submodules https://github.com/RICCIARDI-Adrien/CAT_B100_Tools
```

## Building

Use the `make` command to build the software :
```
cd CAT_B100_Tools
make
```

This will create the `b100-tools` executable.

## Usage

1. Connect you CAT B100 phone through USB to your Linux PC.
2. Make sure that your Linux user belongs to the `dialout` or `uucp` group (see your Linux distribution documentation for more information), so your user can access to the phone serial port without being `root`. You can also add an `udev` rule if you prefer.
3. On the phone, select the `Serial port` choice from the menu displayed on screen. A serial port called `/dev/ttyACMx` or `/dev/ttyUSBx` should appear on your Linux machine.
4. Run `b100-tools` with the command you want (run `b100-tools` without any parameter to display the program usage help).
5. The data retrieved from the phone will be stored to a directory called `Output` that is automatically created by `b100-tools`.
