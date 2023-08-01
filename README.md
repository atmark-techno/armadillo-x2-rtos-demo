# Armadillo RTOS demo

## Usage

### Setup

We use the NXP mcuxpresso sdk for this example, initialize the repo and install required toolchain with these commands:
(~1.2GB download, ~9GB on disk)

```
linux$ virtualenv west-venv
linux$ ./west-venv/bin/pip install west
linux$ ./west-venv/bin/west init -m https://github.com/NXPmicro/mcux-sdk mcuxsdk
linux$ cd mcuxsdk
linux$ ../west-venv/bin/west update
linux$ apt install gcc-arm-none-eabi
```

### Build RTOS application

Build the application.
```
linux$ cd rtos/armgcc
linux$ ./build_release.sh
```
(Scripts have been left as is from the mcuxsdk's `examples/evkmimx8mp/multicore_examples/rpmsg_lite_pingpong_rtos`)

### Build kernel module

TODO

### Install on armadillo

Either copy files manually or generate the swu and install it.

#### swu method

TODO

```
mkswu --update-version rtos.desc
mkswu rtos.desc
```

#### manual method

1. Copy `rtos/armgcc/release/armadillo_rtos_demo.bin` to Armadillo
```
armadillo:~# mount /dev/sda1 /mnt
armadillo:~# cp /mnt/armadillo_rtos_demo.bin /boot/cortex.bin
armadillo:~# persist_file -v /boot/cortex.bin
```

2. Copy module/dtbo to Armadillo

```
TODO
```

3. Tell uboot to boot the cortex M:
```
armadillo:~# vi /boot/boot.txt
load mmc ${mmcdev}:${mmcpart} 0x48000000 /boot/cortex.bin
cp.b 0x48000000 0x7e0000 20000;
bootaux 0x7e0000

# boot into linux
run loadimage && run mmcboot
armadillo:~# mkbootscr /boot/boot.txt
armadillo:~# persist_file -v /boot/boot.*
```
