#!/bin/bash

if [ ! -f sd.img ]; then
qemu-img create sd.img 1G
 
sfdisk ./sd.img << EOF
,64M,c,*
,,L,
EOF

sudo kpartx -av ./sd.img
sudo mkfs.vfat -F 16 -n "boot" /dev/mapper/loop7p1
sudo mkfs.ext4 -L rootfs /dev/mapper/loop7p2
sudo kpartx -d ./sd.img
fi


sudo kpartx -av ./sd.img

sudo mount /dev/mapper/loop7p1 /mnt
sudo cp uRamdisk /mnt
sudo cp ~/linux-5.0.2/arch/arm/boot/dts/vexpress-v2p-pds03.dtb /mnt
sudo cp ~/linux-5.0.2/arch/arm/boot/zImage /mnt
sync

ls -lha /mnt

sudo umount /mnt
sudo kpartx -d ./sd.img






