# Raspberry Pi Pico Altair 8800

## Serial Terminal

### macos

1. screen /dev/tty.usbmodem101 115200
   
   - screen is built into macOS
   - Exit: Press <kbd>ctrl+c</kbd> then <kbd>K</kbd>

2. picocom /dev/tty.usbmodem101 -b 115200

    - Install

        ```shell
        brew install picocom
        ```

    - Exit: Press <kbd>ctrl+a</kbd>, then <kbd>ctrl+x</kbd>


## Regenerate Disk Image Header

1. Copy the .dsk file to the disks folder
2. Run the following command

    ```shell
    python3 dsk_to_header.py --input cpm63k.dsk --output cpm63k_disk.h --symbol cpm63k_dsk
    ```

3. Copy the .h file to the Altair8800 folder
4. Rebuild and deploy


## Rebuild for Performance

cmake -B build -DCMAKE_BUILD_TYPE=Release regenerated the build directory with CMAKE_BUILD_TYPE explicitly set to Release (confirmed by the “Build type is Release” line). That enables the Pico SDK’s release optimization flags (-O3, no extra debug helpers).
cmake --build build then rebuilt everything with those settings. The log shows only Release-config targets being built and the final altair.elf linked successfully with no errors—just the usual picotool fetch/install noise and a warning about duplicate errors/liberrors.a, which the SDK always emits.


```shell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Building App

There are two build tasks

1. Build Altair (Release): Default build task, does clean then build.
2. Build Altair (Debug)L Create a debug build.
