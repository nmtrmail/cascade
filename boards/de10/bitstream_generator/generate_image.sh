#!/bin/bash

cp -r /de10_files /de10_build
cp /de10_build/ip/program_logic.template.v /de10_build/ip/program_logic.v
/quartus/quartus/sopc_builder/bin/qsys-generate /de10_build/soc_system.qsys --synthesis=VERILOG
/quartus/quartus/bin/quartus_map /de10_build/DE10_NANO_SoC_GHRD.qpf
/quartus/quartus/bin/quartus_fit /de10_build/DE10_NANO_SoC_GHRD.qpf
/quartus/quartus/bin/quartus_asm /de10_build/DE10_NANO_SoC_GHRD.qpf
/quartus/quartus/bin/quartus_cpf -c  -o bitstream_compression=on /de10_build/output_files/DE10_NANO_SoC_GHRD.sof /de10_build/output_files/DE10_NANO_SoC_GHRD.rbf

sync

java -jar /sopc2dts/sopc2dts.jar --input /de10_build/soc_system.sopcinfo \
    --output /de10_build/soc_system.dtb \
    --type dtb \
    --board /de10_build/soc_system_board_info.xml \
    --board /de10_build/hps_common_board_info.xml \
    --bridge-removal all \
    --clocks

java -jar /sopc2dts/sopc2dts.jar --input /de10_build/soc_system.sopcinfo \
    --output /de10_build/soc_system.dts \
    --type dts \
    --board /de10_build/soc_system_board_info.xml \
    --board /de10_build/hps_common_board_info.xml \
    --bridge-removal all \
    --clocks

cp /de10_build/output_files/DE10_NANO_SoC_GHRD.rbf /output_files/bitfile.rbf
cp /de10_build/soc_system.dtb /output_files/soc_system.dtb
cp /de10_build/soc_system.dts /output_files/soc_system.dts