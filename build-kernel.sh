rm arch/arm/boot/uImage
export CROSS_COMPILE=arm-xilinx-linux-gnueabi-
make ARCH=arm UIMAGE_LOADADDR=0x8000 uImage && cp arch/arm/boot/uImage /mnt/hgfs/share/rootfs-700/


