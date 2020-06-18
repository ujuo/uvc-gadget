#!/bin/sh
export PATH=/home/ryu/workspace/gcc/gcc-7.4/usr/bin:$PATH
export INSTALL_MOD_PATH=/opt/workspace/nfs-roots/imx8mq-dev/root
export CROSS_COMPILE=aarch64-buildroot-linux-gnu-
export ARCH=arm64
export NAND_IPL_SIZE=16384
#export UIMAGE_TYPE=multi
#export UIMAGE_IN=/home/ryu/linux/linux-4.9.115_svn/out1/arch/arm/boot/zImage:/home/ryu/linux/linux-4.9.115_svn/out1/arch/arm/boot/dts/nxp4330-v45