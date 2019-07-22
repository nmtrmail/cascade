
# Build the base bitstream (and device tree)
FROM no2chem/quartuslite as quartuslite

VOLUME de10_files
VOLUME output_files

RUN apt-get update && apt-get install -y wget build-essential default-jdk git libc6-i386

RUN git clone https://github.com/wgoossens/sopc2dts.git /sopc2dts && cd sopc2dts && make

COPY generate_image.sh .

ENTRYPOINT [ "bash", "-c",  "./generate_image.sh"  ]