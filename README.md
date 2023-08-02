# Armadillo RTOS demo

## Usage

### Setup

We use the NXP mcuxpresso sdk for this example.

First, install requirements (~200MB download, ~2GB on disk)

```
linux$ apt install gcc-arm-none-eabi python3-virtualenv
linux$ virtualenv west-venv
linux$ ./west-venv/bin/pip install west
```

Then you can either install the whole mcuxpresso sdk, or just required components for the example. Using the full repository is recommended for development (to e.g. add other drivers)

Full repository (~1GB download, ~7GB on disk):

```
linux$ ./west-venv/bin/west init -m https://github.com/nxp-mcuxpresso/mcux-sdk mcuxsdk
linux$ cd mcuxsdk
linux$ ../west-venv/bin/west update
```

Minimal repository (500MB download, 1.3GB on disk):

```
linux$ ./west-venv/bin/west init -l west
linux$ ./west-venv/bin/west update
```


### Build RTOS application

Build the application. This will create `rtos/armgcc/release/armadillo_rtos_demo.{bin,elf}`

```
linux$ cd rtos/armgcc
linux$ ARMGCC_DIR=/usr ./build_release.sh
```

(Scripts have been left as is from the mcuxsdk's `examples/evkmimx8mp/multicore_examples/rpmsg_lite_pingpong_rtos`)

### Build kernel module

1. Download kernel from https://armadillo.atmark-techno.com/resources/software/armadillo-iot-g4/linux-kernel
2. Extract, build & install kernel https://manual.atmark-techno.com/armadillo-iot-g4/armadillo-iotg-g4_product_manual_ja-1.20.0/ch10.html#sct.build-kernel
3. Build module. KERNELDIR is an example. This will create `linux/rpmsg_armadillo.ko`

```
linux$ KERNELDIR=$HOME/linux-5.10-5.10.186-r0
linux$ CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm64 make -C "$KERNELDIR" M="$PWD/linux" modules
```

### Build DTS overlay

This example works as is with the `armadillo_iotg_g4-rpmsg.dtbo` overlay as of linux 5.10.186-r0, but for demonstration purpose we also locked the GPIO we used on the RTOS side to make sure it cannot be used from linux.

The dtbo can be built as follow. This will create `dts/armadillo_iot_g4-rtos-demo.dtbo` (after copy).

```
linux$ cp dts/armadillo_iotg_g4-rtos-demo.dts "$KERNELDIR/arch/arm64/boot/dts/freescale/"
linux$ CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm64 make -C "$KERNELDIR" freescale/armadillo_iotg_g4-rtos-demo.dtbo
linux$ cp "$KERNELDIR/arch/arm64/boot/dts/freescale/armadillo_iotg_g4-rtos-demo.dtbo" dts/
```

### Install on armadillo

Either copy files manually or generate the swu and install it.

#### swu method

`rtos.desc` contains an example mkswu description to install the necessary files to run. It assumes the kernel has already been updated before in the "Build kernel module" step.

```
linux$ mkswu --update-version rtos.desc
linux$ mkswu rtos.desc
```

rtos.swu can then be installed.

#### manual method

1. Copy `rtos/armgcc/release/armadillo_rtos_demo.bin` to Armadillo. We need the .bin file for loading through u-boot.

```
armadillo:~# mount /dev/sda1 /mnt
armadillo:~# cp /mnt/armadillo_rtos_demo.bin /boot/
armadillo:~# persist_file -vp /boot/armadillo_rtos_demo.bin
```

2. Copy module to Armadillo

```
armadillo:~# mkdir /lib/modules/$(uname -r)/extras
armadillo:~# cp /mnt/rpmsg_armadillo.ko  /lib/modules/$(uname -r)/extras/
armadillo:~# depmod
armadillo:~# persist_file -rv /lib/modules
```

3. Enable dtb overlay to reserve memory for cortex M (append to overlays.txt with a space if other dtbos are present)

```
armadillo:~# cp /mnt/armadillo_iotg_g4-rtos-demo.dtbo /boot/
armadillo:~# vi /boot/overlays.txt
fdt_overlays=armadillo_iotg_g4-rtos-demo.dtbo
armadillo:~# persist_file -vp /boot/armadillo_iotg_g4-rtos-demo.dtbo /boot/overlays.txt
```

4. Tell uboot to boot the cortex M:

```
armadillo:~# cp /mnt/boot.txt /boot/
armadillo:~# mkbootscr /boot/boot.txt
armadillo:~# persist_file -vp /boot/boot.*
```

5. Reboot

```
armadillo:~# reboot
```

## Development

### Update without reboot

After loading the cortex code once through u-boot, one can reload the firmware through the remoteproc interface:

1. Stop both cortex M core and the rpmsg module

```
armadillo:~# echo stop > /sys/class/remoteproc/remoteproc0/state
armadillo:~# modprobe -r rpmsg_armadillo
```

2. Update files as appropriate. Note the firmware used here is the .elf file.

```
armadillo:~# cp /mnt/armadillo_rtos_demo.elf /lib/firmware/armadillo_rtos_demo.elf
armadillo:~# cp /mnt/rpmsg_armadillo.ko  /lib/modules/$(uname -r)/extras
```

3. Restart the cortex M core. The kernel module will be loaded automatically when the rpmsg channel is announced.

```
armadillo:~# echo armadillo_rtos_demo.elf > /sys/class/remoteproc/remoteproc0/firmware
armadillo:~# echo start > /sys/class/remoteproc/remoteproc0/state
```
