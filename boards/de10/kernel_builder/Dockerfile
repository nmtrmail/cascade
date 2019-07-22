FROM ubuntu:18.04 as build

ARG UBOOT_TAG="v2019.07"
ARG KERNEL_TAG="v5.2"
ARG GCC_VER="8.2-2018.11"
ARG UBUNTU_VER="18.04.2"

# Build parameters
ARG MAKE_PARALLELISM=4

VOLUME output_files

# First, get tools we need from package manager
RUN apt-get update && apt-get install -y wget \
    build-essential \
    bison \
    flex \
    git \
    qemu-user-static \
    binfmt-support \
    bc \
    xz-utils \
    python

# Now, get all the components we need
RUN mkdir /builder \
    && git clone git://git.denx.de/u-boot.git  --depth=1 --branch ${UBOOT_TAG} /builder/uboot \
    && git clone git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git --depth=1 --branch ${KERNEL_TAG} /builder/kernel \
    && wget https://developer.arm.com/-/media/Files/downloads/gnu-a/${GCC_VER}/gcc-arm-${GCC_VER}-x86_64-arm-eabi.tar.xz -O toolchain.tar.xz \
    && mkdir /builder/toolchain \
    && tar -xf toolchain.tar.xz -C /builder/toolchain \
    && rm toolchain.tar.xz \
    && wget http://cdimage.ubuntu.com/ubuntu-base/releases/18.04.2/release/ubuntu-base-18.04.2-base-armhf.tar.gz -O ubuntu.tar.gz \
    && mkdir /builder/rootfs \
    && tar -xf ubuntu.tar.gz -C /builder/rootfs

# Set the path for the cross compiler
ENV CROSS_COMPILE=/builder/toolchain/gcc-arm-${GCC_VER}-x86_64-arm-eabi/bin/arm-eabi-

COPY socfpga_cyclone5_de10_nano.dts /builder/uboot/arch/arm/dts/socfpga_cyclone5_de10_nano.dts
COPY u-boot.script .
COPY build_kernel.sh .

ENTRYPOINT [ "bash", "-c",  "./build_kernel.sh"  ]