#!/bin/bash
#For debian systems (testes on Ubuntu 16.04)

#installation dir
dest=install
src=$(pwd)

apt-get install libnuma-dev elfutils libnewt-dev binutils-dev zlib-bin libelf-dev libnuma1 numactl iotop
MSR_CONF=$(cat /boot/config-$(uname -r) | grep CONFIG_X86_MSR)
#TODO:check that MSR_CONF is equal to ..
sudo modprobe msr
cp .toprc $HOME

mkdir -p $dest
cd install
wget http://www.kernel.org/pub/linux/kernel/v2.6/longterm/v2.6.34/linux-2.6.34.12.tar.bz2
bunzip2 linux-2.6.34.12.tar.bz2
tar -xf linux-2.6.34.12.tar
rm -rf linux-2.6.34.12/tools/perf/
cp -pr $srd/tool/perf linux-2.6.34.12/tools/
cd linux-2.6.34.12/tools/perf/
make HOME="/usr"
make
make install
echo "Done"
