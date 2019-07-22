FROM ubuntu:18.04 as build

ARG UBUNTU_VER="18.04.2"

VOLUME output_files
VOLUME cascade_dir

RUN apt-get update && apt-get install -y wget rsync git python parted udev dosfstools\
    && wget http://cdimage.ubuntu.com/ubuntu-base/releases/18.04.2/release/ubuntu-base-18.04.2-base-armhf.tar.gz -O ubuntu.tar.gz \
    && mkdir -p /builder/rootfs \
    && tar -xf ubuntu.tar.gz -C /builder/rootfs && \
    wget https://github.com/multiarch/qemu-user-static/releases/download/v4.0.0-5/qemu-arm-static && \
    chmod a+x qemu-arm-static && \
    cp qemu-arm-static /builder/rootfs/usr/bin

COPY fstab /builder/etc/fstab
COPY ttyPS0.conf /builder/etc/init/ttyPS0.conf
COPY generate_image.sh .

ENTRYPOINT [ "bash", "-c",  "./generate_image.sh"  ]