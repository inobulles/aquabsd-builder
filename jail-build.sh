#!/usr/bin/env sh
set -e

# install dependencies required for building
# this includes, e.g., required library headers for AQUA devices

echo -n "Installing dependencies required for building ..."
read _

pkg install git-lite pkgconf pango librsvg2 icu

cd /
git clone https://github.com/inobulles/aqua-unix

( cd /aqua-unix/ && sh build.sh --devbranch aquabsd.alps )

echo "#!/bin/sh" > /aqua-unix/src/devices/aquabsd.alps.vga/build.sh
echo "cc -shared -fPIC main.c -o device -DWITHOUT_X11 \"\$@\"" >> /aqua-unix/src/devices/aquabsd.alps.vga/build.sh

( cd /aqua-unix/ && sh build.sh --devbranch aquabsd.alps --devices --kos --install )

# build kernel & userspace

echo -n "Compiling kernel and userland ..."
read _

cd /usr/src

# building kernel takes about 8 minutes, and building userspace, 20
# TODO experiment with disabling Kerberos ('WITHOUT_KERBEROS=yes')

thread_count=$(sysctl -n hw.ncpu)
flags="WITHOUT_LIB32=yes WITHOUT_TOOLCHAIN=yes WITHOUT_TCSH=yes WITHOUT_FREEBSD_UPDATE=yes"

make buildkernel -j$thread_count
make buildworld -j$thread_count $flags

# TODO maybe this is a better way of doing things

cd release/
make bootonly -DNO_ROOT -DNODOCS -DNOPORTS -DNOSRC $flags

root_dir=/rootfs/
mv /usr/obj/usr/src/amd64.amd64/release/bootonly/ $root_dir

# install necessary packages
# TODO see if the fetching stage can be skipped with packages cached previously

pkg -r $root_dir install pango librsvg2 icu

# install other necessary stuff
# e.g. all AQUA components

cp /usr/local/include/iar.h $root_dir/usr/local/include/
cp /usr/local/lib/libiar.a /usr/local/lib/libiar.so $root_dir/usr/local/lib/
cp /usr/local/bin/iar $root_dir/usr/local/bin/

mv /root/.aqua-root/ $root_dir/root/
mv /aqua-unix/ $root_dir

echo "#!/bin/sh" > $root_dir/install-aqua.sh
echo "cd aqua-unix/ && sh build.sh --install" >> $root_dir/install-aqua.sh

chmod +x $root_dir/install-aqua.sh
chroot $root_dir /install-aqua.sh

rm -rf $root_dir/aqua-unix/
rm $root_dir/install-aqua.sh

# a bit of extra setup

echo "kern.vty=sc" >> $root_dir/boot/loader.conf

# make final UFS filesystem image

label="aquabsd-install"
image="/aquabsd.img"

echo "/dev/ufs/$label / ufs ro,noatime 1 1" > $root_dir/etc/fstab
echo "root_rw_mount='NO'" > $root_dir/etc/rc.conf.local

makefs -B little -o label=$label -o version=2 $image.part $root_dir

rm $root_dir/etc/fstab
rm $root_dir/etc/rc.conf.local

# make EFI System Partition (ESP)

. /usr/src/tools/boot/install-boot.sh # include a bunch of helpful functions

esp_image="/esp.img"
make_esp_file $esp_image $fat32min $root_dir/boot/loader.efi

# make final system image

mkimg -s mbr -b $root_dir/boot/mbr -p efi:=$esp_image -p freebsd:-"mkimg -s bsd -b $root_dir/boot/boot -p freebsd-ufs:=$image.part" -a 2 -o $image

# clean up
# TODO is this necessary if everything is going to be deleted anyway?

rm $esp_image
rm $image.part

exit