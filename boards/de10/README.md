# Cascade Image Generator for the DE10 Nano

This folder contains scripts which generate a working Cascade image for the [Terasic DE10 Nano FPGA](de10-nano.terasic.com/).

## Requirements

To run the generator, you will need Docker installed. The generator has been tested to work on Linux and macos.

The resulting SD card image is 2GB. You will need a SD card that is at least 2GB to load it on.

## Running the generator

Invoke the generator by running the ```generate_image.sh``` script from this directory:
```bash
$ ./generate_image.sh
```

The image generation step requires root privileges since it uses the loopback device. You can either run the script
with root privilege or the script will prompt you for root privileges when required.

## Load the image on to the SD card

The SD card image will be generated in the ```output_files``` directory. You can load the image on your SD card using the ```dd``` command.


