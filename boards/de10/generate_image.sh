#!/bin/bash

if [ -d "output_files" ]; then
  echo -e "Output files already exist!

To rebuild, delete the output files
${RED}rm -rf output_files${NC}
Then run this script again.
"
exit 1
fi

mkdir -p output_files
chmod a+rwx output_files
DE10_FILES=$(cd ../../src/target/core/de10/fpga; pwd)
CASCADE_DIR=$(cd ../..;pwd)
OUTPUT_FILES="$(cd output_files; pwd)"


echo "Building Docker Support Images..."``
docker build --rm -f "bitstream_generator/Dockerfile" -t de10_bitstream_generator:latest bitstream_generator
docker build --rm -f "kernel_builder/Dockerfile" -t kernel_builder:latest kernel_builder
docker build --rm -f "image_generator/Dockerfile" -t image_generator:latest image_generator

echo "Build Base Bitstream..."
docker run -v $DE10_FILES:/de10_files -v $OUTPUT_FILES:/output_files -it de10_bitstream_generator:latest bash

echo "Build Linux Kernel & uBoot Image..."
docker run -v $OUTPUT_FILES:/output_files -it kernel_builder:latest

echo "Running the image generator (requires sudo)..."
sudo docker run --privileged -v $OUTPUT_FILES:/output_files -v $CASCADE_DIR:/cascade_dir -it image_generator:latest
