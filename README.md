# Armadillo RTOS demo

## Usage

### Setup

We use the NXP mcuxpresso sdk for this example.

Commands below are listed with either a `ATDE$` or `armadillo#` prompt. The ATDE virtual machine can be downloaded on the [Armadillo site (ja)](https://armadillo.atmark-techno.com/resources/software/atde/atde-v9).
The armadillo-x2-rtos-demo clone directory has been shortened to "demo" in prompts for brievty.

First, install requirements (~200MB download, ~2GB on disk)

```
ATDE$ apt install gcc-arm-none-eabi cmake python3-virtualenv
ATDE$ git clone https://github.com/atmark-techno/armadillo-x2-rtos-demo
ATDE$ cd armadillo-x2-rtos-demo
ATDE demo$ virtualenv west-venv
ATDE demo$ ./west-venv/bin/pip install west
```

Then you can either install the whole mcuxpresso sdk, or just required components for the example. Using the full repository is recommended for development (to e.g. add other drivers)

Full repository (~400MB download, ~7GB on disk):

```
ATDE demo$ ./west-venv/bin/west init -m https://github.com/nxp-mcuxpresso/mcux-sdk mcuxsdk
ATDE demo$ cd mcuxsdk
ATDE mcuxsdk$ ../west-venv/bin/west update --narrow -o=--depth=1
```

Minimal repository (50MB download, 900MB on disk):

```
ATDE demo$ ./west-venv/bin/west init -l west
ATDE demo$ ./west-venv/bin/west update --narrow -o=--depth=1
```


### Build RTOS application

Either GCC or IAR can be used to build this project.

Each project have these targets:
 - `release`/`debug`: Build for the tightly coupled memory (TCM). The rest of this document assumes 'release' build is used.
 - `ddr_release`/`ddr_debug`: Binary will be mapped in RAM, without using TCM.
`boot_script/boot_ddr.txt` can be used with these binaries.
This can be necessary if the code (.text) section grows bigger than 127KB.
 - `flash_release`/`flash_debug`: Not supported for Armadillo, leftover from mcuxpresso examples.

#### Building with GCC

The following command will create `rtos/armgcc/release/armadillo_rtos_demo.{bin,elf}`

```
ATDE demo$ cd rtos/armgcc
ATDE armgcc$ ARMGCC_DIR=/usr ./build_release.sh
[...]
[100%] Linking C executable release/armadillo_rtos_demo.elf
Memory region         Used Size  Region Size  %age Used
    m_interrupts:         680 B         1 KB     66.41%
          m_text:       21296 B       127 KB     16.38%
          m_data:       43944 B       128 KB     33.53%
         m_data2:          0 GB        16 MB      0.00%
[100%] Built target armadillo_rtos_demo.elf
ATDE armgcc$ ls release
armadillo_rtos_demo.bin*  armadillo_rtos_demo.elf*
```

#### Building with IAR

Open the `rtos/iar/armadillo_rtos_demo.eww` workspace file with IAR and build for your target.
The relase build with create `rtos/iar/Release/armadillo_rtos_demo.bin`


### Build linux kernel module

1. Download linux kernel from https://armadillo.atmark-techno.com/resources/software/armadillo-iot-g4/linux-kernel

2. Extract and build linux to perform compatibility check on module loading:

```
ATDE$ sudo apt install crossbuild-essential-arm64 bison flex \
    python3-pycryptodome python3-pyelftools zlib1g-dev libssl-dev \
    bc firmware-misc-nonfree
ATDE$ tar xf linux-at-x2-<version>.tar
ATDE$ tar xf linux-at-x2-<version>/linux-<version>.tar.gz
ATDE$ cd linux-<version>
ATDE linux-<version>$ CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm64 make x2_defconfig
ATDE linux-<version>$ CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm64 make -j9
```

3. Check that the linux kernel running on armadillo is the same as the version we built.
The build directory is in the form `linux-5.10-5.10.<minor>-r<release>`, check that `minor` and `release` match below:

```
armadillo:~# uname -r
5.10.<minor>-<release>-at
```

If the versions do not match, update armadillo (or install the kernel as described in the [reference manual (ja)](https://manual.atmark-techno.com/armadillo-iot-g4/armadillo-iotg-g4_product_manual_ja-1.20.0/ch10.html#sct.build-kernel)

```
armadillo:~# swupdate -d '-u https://download.atmark-techno.com/armadillo-x2/image/baseos-x2-latest.swu'
[...]
[INFO ] : SWUPDATE running :  [install_single_image] : Installing baseos-x2-<version>
[INFO ] : SWUPDATE running :  [install_single_image] : Installing post_script
[INFO ] : SWUPDATE running :  [read_lines_notify] : Removing unused containers
[INFO ] : SWUPDATE running :  [read_lines_notify] : swupdate triggering reboot!
```

4. Build module. KERNELDIR is the directory of the linux kernel we built.
This will create `linux/rpmsg_armadillo.ko`

```
ATDE$ KERNELDIR=$HOME/linux-<version>
ATDE demo$ CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm64 make -C "$KERNELDIR" M="$PWD/linux" modules
ATDE demo$ ls linux/rpmsg_armadillo.ko
ATDE/rpmsg_armadillo.ko
```

### Build DTB overlay

This example works as is with the `armadillo_iotg_g4-rpmsg.dtbo` overlay as of linux 5.10.186-r0, but for demonstration purpose we also locked the GPIO we used on the RTOS side to make sure it cannot be used from linux.

The dtbo can be built as follow. This will create `dts/armadillo_iot_g4-rtos-demo.dtbo` (after copy).

```
ATDE demo$ cp dts/armadillo_iotg_g4-rtos-demo.dts "$KERNELDIR/arch/arm64/boot/dts/freescale/"
ATDE demo$ CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm64 make -C "$KERNELDIR" freescale/armadillo_iotg_g4-rtos-demo.dtbo
ATDE demo$ cp "$KERNELDIR/arch/arm64/boot/dts/freescale/armadillo_iotg_g4-rtos-demo.dtbo" dts/
ATDE demo$ ls dts/armadillo_iotg_g4-rtos-demo.dtbo
armadillo_iotg_g4-rtos-demo.dtbo
```

The default overlay build is included for convenience.

### Install on armadillo

Either copy files manually or generate the swu and install it.

#### SWU method

Updates on Armadillo Base OS are distributed through SWU files, generated with [mkswu](https://github.com/atmark-techno/mkswu).
Please refer to the [reference manual (ja)](https://manual.atmark-techno.com/armadillo-iot-g4/armadillo-iotg-g4_product_manual_ja-1.20.0/ch10.html#sct.mkswu) for its initial configuration.

`rtos.desc` contains an example mkswu description to install the necessary files to run.
It does not check the kernel version that was used to build the module, so if the module does not load please install the kernel built previously and redo this step.

```
ATDE demo$ mkswu --update-version rtos.desc
ATDE demo$ mkswu rtos.desc
```

rtos.swu can then be installed.

#### manual method

1. Copy files we will need on a USB drive:

```
ATDE demo$ sudo mount /dev/sda1 /mnt
ATDE demo$ cp rtos/*/*/armadillo_rtos_demo.* /mnt
ATDE demo$ cp linux/rpmsg_armadillo.ko /mnt
ATDE demo$ cp dts/armadillo_iotg_g4-rtos-demo.dtbo /mnt
ATDE demo$ cp boot_script/boot.txt /mnt
ATDE demo$ umount /mnt
```

2. Copy `rtos/armgcc/release/armadillo_rtos_demo.bin` to Armadillo. We need the .bin file for loading through u-boot.

```
armadillo:~# mount /dev/sda1 /mnt
armadillo:~# cp /mnt/armadillo_rtos_demo.bin /boot/
armadillo:~# persist_file -v /boot/armadillo_rtos_demo.bin
```

3. Copy module to Armadillo. If the kernel is not compatible with the module, the modprobe command will fail.

```
armadillo:~# mkdir /lib/modules/$(uname -r)/extras
armadillo:~# cp /mnt/rpmsg_armadillo.ko /lib/modules/$(uname -r)/extras/
armadillo:~# depmod
armadillo:~# modprobe rpmsg_armadillo
armadillo:~# persist_file -rv /lib/modules
```

4. Enable dtb overlay to reserve memory for cortex M7 (append to overlays.txt with a space if other dtbos are present)

```
armadillo:~# cp /mnt/armadillo_iotg_g4-rtos-demo.dtbo /boot/
armadillo:~# vi /boot/overlays.txt
fdt_overlays=armadillo_iotg_g4-rtos-demo.dtbo
armadillo:~# persist_file -v /boot/armadillo_iotg_g4-rtos-demo.dtbo /boot/overlays.txt
```

5. Tell uboot to boot the cortex M7:

```
armadillo:~# cp /mnt/boot.txt /boot/
armadillo:~# mkbootscr /boot/boot.txt
armadillo:~# persist_file -v /boot/boot.*
```

6. Freeze the kernel version and newly installed files.
We've saved the files from the in-memory overlayfs to the eMMC with
`persist_file`, but files will be lost after an OS upgrade.
Adding them to `/etc/swupdate_preserve_files` will freeze them
across updates.

```
armadillo:~# vi /etc/swupdate_preserve_files
# add the following two lines
POST /boot
POST /lib/modules
armadillo:~# persist_file -v /etc/swupdate_preserve_files
```

7. Reboot

```
armadillo:~# reboot
```

### Verify it works

If install succeeded, you should have FreeRTOS running on the cortex M7, and a module driver on linux allowing to communicate with the RTOS.

1. `dmesg` should contain informations about rpmsg and the module, such as:

```
[    0.048219] imx rpmsg driver is registered.
[    1.158745] virtio_rpmsg_bus virtio0: rpmsg host is online
[    2.158859] virtio_rpmsg_bus virtio0: creating channel rpmsg-armadillo-demo-channel addr 0x1e
[    3.372122] rpmsg_armadillo: no symbol version for module_layout
[    3.372135] rpmsg_armadillo: loading out-of-tree module taints kernel.
[    3.372182] rpmsg_armadillo: module verification failed: signature and/or required key missing - tainting kernel
[    3.374029] rpmsg_armadillo virtio0.rpmsg-armadillo-demo-channel.-1.30: probing rpmsg_armadillo on channel 0x400 -> 0x1e
[    3.374088] rpmsg_armadillo virtio0.rpmsg-armadillo-demo-channel.-1.30: rpmsg_armadillo ready
```

2. the module will create a couple of writable files in `/sys/class/remoteproc/remoteproc0/remoteproc0\#vdev0buffer/virtio0/virtio0.rpmsg-armadillo-demo-channel.-1.30/`:

```
armadillo:~# cd /sys/class/remoteproc/remoteproc0/remoteproc0\#vdev0buffer/virtio0/virtio0.rpmsg-armadillo-demo-channel.-1.30/
armadillo:virtio0.rpmsg-armadillo-demo-channel.-1.30# ls set*
set_gpio  set_loglevel
armadillo:virtio0.rpmsg-armadillo-demo-channel.-1.30# echo 0 > set_loglevel
armadillo:virtio0.rpmsg-armadillo-demo-channel.-1.30# dmesg | tail -n 2
[  159.965872] rpmsg_armadillo virtio0.rpmsg-armadillo-demo-channel.-1.30: setting loglevel to 0
[  159.966227] rpmsg_armadillo virtio0.rpmsg-armadillo-demo-channel.-1.30: [remote] set log level
armadillo:virtio0.rpmsg-armadillo-demo-channel.-1.30# echo 1 > set_gpio
armadillo:virtio0.rpmsg-armadillo-demo-channel.-1.30# dmesg | tail -n 2
[  181.364096] rpmsg_armadillo virtio0.rpmsg-armadillo-demo-channel.-1.30: setting gpio to 1
[  181.364145] rpmsg_armadillo virtio0.rpmsg-armadillo-demo-channel.-1.30: [remote] toggling gpio
```

In this demo,

 - `set_loglevel` allows controlling how much messages are sent from the RTOS application to linux, for debugging.
Messages prefixed with `[remote]` above come from the RTOS application through the rpmsg channel.
0 prints all messages and 4 prints none.

 - `set_gpio` allow controlling GPIO1 pin 15, which can be checked on CON11 pin 24 on Armadillo IoT G4 and Armadillo X2.
Writing 1 sets it to high and 0 to low.

## Development

### RTOS description

The `rtos` directory contains a sample FreeRTOS application that:
 - sets up an rpmsg channel to communicate with linux.
 - listen to the channel to toggle a GPIO1 pin 15 when requested.
 - sends any log messages over through the rpmsg channel for debugging.

Here is the breakdown of the files:
 - `main_remote.c` is the main program:
   * `main` does the initilization and creates a task.
   * `app_task` creates the rpmsg channel and listens to it forever.
   * the "protocol" used to transmit messages is defined in `../common/protocol.h`
 - `pin_mux.c` contains the used pin definitions to interact with other hardware.
For example, using CAN will require setting these two pins: `IOMUXC_SAI5_RXD1_CAN1_TX` / `IOMUXC_SAI5_RXD2_CAN1_RX`.
The Pins Tool at https://mcuxpresso.nxp.com can be used to generate this section (requires an NXP account).
 - `armgcc/MIMX8ML8xxxxx_cm7_ram.ld` and `iar/MIMX8ML8xxxxx_cm7_ram.icf` contain the linker script used by gcc and IAR respectively, for the release build we support.
They define:
   * interrupt handlers and text (binary code) is included in the TCM from 0x0 to 0x20000 (text is 127KB).
   * data segments are defined in 0x20000000 (`m_data` in TCM, 128KB) and 0x80000000 (`m_data2` in DDR, 16MB). Heap and main stack are in the TCM segment.
   * These addresses are mapped from physical regions in `board.c`.
 - FreeRTOS code itself is in `mcuxsdk/rtos/freertos/freertos-kernel`.
 - CPU specific files are in `mcuxsdk/core/devices/MIMX8ML8`, the exact package model used is `MIMX8ML8DVNLZ`.
 - Drivers from NXP are in `mcuxsdk/core/drivers` and `mcuxsdk/middleware/multicore`.
 - If you did a full clone of the sdk, examples in `mcuxsdk/examples/evkmimx8mp` are mostly compatible.


Applications other than FreeRTOS can be used on the Cortex M7, but if communication with linux is desirable we recommend rpmsg for transmission.
The code at https://github.com/OpenAMP/open-amp can be used as a base for bare metal applications.

### Linux module description

The linux module is necessary to interact with the Cortex M7 from linux.
This module is automatically loaded when the rpmsg named rpmsg-armadillo-demo-channel is created from the Cortex M7 side (`RPMSG_LITE_NS_ANNOUNCE_STRING` defined in `../common/protocol.h`).

On init, this module sends its own version (`VERSION`) to the rpmsg channel and waits for the same version to be echoed back.
After this initial handshake, it:
 - creates files in sysfs to allow sending well-defined messages from userspace
The files are created through the `rpmsg_armadillo_attrs` list, where each file has its own callbacks generated with the `DEVICE_ATTR_WO` macro. `DEVICE_ATTR_RO` and `DEVICE_ATTR_RW` can also be used for readable and read-write files, in which case they also need to implement a `show` callback.
 - keeps listening to the rpmsg channel for log messages

### DTS description

The DTB overlay defines:
 - Reserved memory for the Cortex M7, so linux does not use it. In particular:
   * 16MB at offset 0x80000000 as seen in linker script's `m_data2`
   * shared buffers for rpmsg and firmware reload (`vdev0vring[0-1]`, `vdevbuffer` and `rsc_table`)
 - Remote proc node (`imx8mp-cm7`) for control and rpmsg
 - Disabled components as they are used on the M7 core.
If you use a hardware component in the RTOS you should make sure that it is not used by linux:
   * `gpio-hog` can be used to lock a single gpio. One of `input`, `output-low` or `output-high` must be used in this case, and the output variants will write the state once on boot so it might conflict with your application. This can be left out if the GPIO is never used on linux.
   * components can be disabled e.g. `&flexcan1 { status = "disabled"; };` would disable CAN1 on linux. Note that CAN1 is already disabled by default for the Armadillo IoT G4, so this particular example is not required; you can find enabled components in `arch/arm64/boot/dts/freescale/armadillo_iot_g4.dts` in the linux kernel sources.

### Update without reboot

After loading the cortex M7 code once through u-boot, one can reload the firmware through the remoteproc interface.
Note this is only used for testing and changes here will not persist after reboot.

1. Stop both cortex M7 core and the rpmsg module

```
armadillo:~# echo stop > /sys/class/remoteproc/remoteproc0/state
armadillo:~# modprobe -r rpmsg_armadillo
```

2. Update files as appropriate. Note the firmware used here is the .elf file, which is only created with the gcc toolchain.

```
armadillo:~# cp /mnt/armadillo_rtos_demo.elf /lib/firmware/armadillo_rtos_demo.elf
armadillo:~# cp /mnt/rpmsg_armadillo.ko /lib/modules/$(uname -r)/extras
```

3. Restart the cortex M7 core. The kernel module will be loaded automatically when the rpmsg channel is announced.

```
armadillo:~# echo armadillo_rtos_demo.elf > /sys/class/remoteproc/remoteproc0/firmware
armadillo:~# echo start > /sys/class/remoteproc/remoteproc0/state
```

Note: the remoteproc interface is not safe to stop the Cortex-M system and is only intended for testing. See [AN5317](https://www.nxp.com/docs/en/application-note/AN5317.pdf) for details.
