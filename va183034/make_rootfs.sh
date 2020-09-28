
rm initramfs.cpio.gz
rm uRamdisk

cd rootfs
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio 
cd ..
gzip initramfs.cpio
mkimage -A arm -O linux -T ramdisk -d initramfs.cpio.gz uRamdisk
