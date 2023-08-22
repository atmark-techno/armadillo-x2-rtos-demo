# Armadillo RTOS デモ

## デモの実行

### ビルドに必要な準備

このデモは NXP が提供している MCUXpresso の SDK を使用しています。

以下のコマンドは `ATDE$` か `armadillo#` のプロンプトで記載しています。ATDE の仮想マシンは [Armadillo サイト](https://armadillo.atmark-techno.com/resources/software/atde/atde-v9) に取得できます。
「demo」のディレクトリ名はクローンした armadillo-x2-rtos-demo のディレクトリです。

以下のコマンドで、環境構築に必要なパッケージとツールをインストールします（~200MB ダウンロード、~2GB ディスクスペース）

```
ATDE$ apt install gcc-arm-none-eabi cmake python3-virtualenv
ATDE$ git clone https://github.com/atmark-techno/armadillo-x2-rtos-demo
ATDE$ cd armadillo-x2-rtos-demo
ATDE demo$ virtualenv west-venv
ATDE demo$ ./west-venv/bin/pip install west
```

以降の手順で MCUXpresso SDK をダウンロードできます。

以下の二つの選択肢から選んでください。開発を行う場合は、SDK 全体を取得する手順をお勧めします。

1. SDK 全体を取得します（~400MB ダウンロード、~7GB ディスクスペース）

```
ATDE demo$ ./west-venv/bin/west init -m https://github.com/nxp-mcuxpresso/mcux-sdk mcuxsdk
ATDE demo$ cd mcuxsdk
ATDE mcuxsdk$ ../west-venv/bin/west update --narrow -o=--depth=1
```

2. このデモで使う部分だけを取得します（~50MB ダウンロード、900MB ディスクスペース）

```
ATDE demo$ ./west-venv/bin/west init -l west
ATDE demo$ ./west-venv/bin/west update --narrow -o=--depth=1
```


### RTOS アプリケーションのビルド

このデモのビルドには、GCCまたはIARを利用できます。

以下のターゲットでビルドできます：
 - `release` / `debug` : Tightly coupled memory (TCM) を使うアプリケーションになります。
 この資料では `release` モードを想定しています。
 - `ddr_release` / `ddr_debug` : TCM を使わずに、RAM だけを使うアプリケーションになります。
`boot_script/boot_ddr.txt` に従ってバイナリをロードします。
コードセクション (.text) が 127KB を越える場合に、このターゲットを指定します。
 - `flash_release` / `flash_debug`: Armadillo でサポートしてません。MCUXpresso のサンプルに存在していたものを消さずに残しています。

#### GCC でのビルド手順

以下のコマンドで `rtos/armgcc/release/armadillo_rtos_demo.{bin,elf}` を生成します

```
ATDE demo$ cd rtos/armgcc
ATDE armgcc$ ARMGCC_DIR=/usr ./build_release.sh
... (省略)
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

#### IAR でのビルド手順

`rtos/iar/armadillo_rtos_demo.eww` のワークスペースファイルを IAR で開いて、ターゲットを選択してビルドできます。
`release` を選択してビルドすると `rtos/iar/Release/armadillo_rtos_demo.bin` が生成されます。

### Linux カーネルモジュールのビルド

1. Linux カーネルを Armadillo サイトから取得します
https://armadillo.atmark-techno.com/resources/software/armadillo-iot-g4/linux-kernel

2. Linux を展開し、Module.symvers を生成するためにビルドします。

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

3. Armadillo で実行している Linux カーネルがビルドした物と同じバージョンになっていることを確認します。
ビルドディレクトリが `linux-5.10-5.10.<minor>-r<release>` で、以下の出力で `minor` と `release` が一致していることを確認します。

```
armadillo:~# uname -r
5.10.<minor>-<release>-at
```

Armadillo が別のバージョンで起動していたら、アップデートします（あるいは [マニュアルのインストール手順通り](https://manual.atmark-techno.com/armadillo-iot-g4/armadillo-iotg-g4_product_manual_ja-1.20.0/ch10.html#sct.build-kernel) にカーネルだけをインストールします）

```
armadillo:~# swupdate -d '-u https://download.atmark-techno.com/armadillo-x2/image/baseos-x2-latest.swu'
... (省略)
[INFO ] : SWUPDATE running :  [install_single_image] : Installing baseos-x2-<version>
[INFO ] : SWUPDATE running :  [install_single_image] : Installing post_script
[INFO ] : SWUPDATE running :  [read_lines_notify] : Removing unused containers
[INFO ] : SWUPDATE running :  [read_lines_notify] : swupdate triggering reboot!
```

4. モジュールをビルドします。KERNELDIR にビルドしたカーネルディレクトリを指定します。
以下のコマンドで `linux/rpmsg_armadillo.ko` が生成されます。

```
ATDE$ KERNELDIR=$HOME/linux-<version>
ATDE demo$ CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm64 make -C "$KERNELDIR" M="$PWD/linux" modules
ATDE demo$ ls linux/rpmsg_armadillo.ko
ATDE/rpmsg_armadillo.ko
```

### DTB overlay のビルド

このデモは linux 5.10.186-r0 に含まれている `armadillo_iotg_g4-rpmsg.dtbo` でも動作しますが、
説明のために RTOS で使用する GPIO をロックし、 Linux 側から使えないようにします。

DTBO ファイルは以下のコマンドでビルドできます。 `dts/armadillo_iot_g4-rtos-demo.dtbo` が生成されます。

```
ATDE demo$ cp dts/armadillo_iotg_g4-rtos-demo.dts "$KERNELDIR/arch/arm64/boot/dts/freescale/"
ATDE demo$ CROSS_COMPILE=aarch64-linux-gnu- ARCH=arm64 make -C "$KERNELDIR" freescale/armadillo_iotg_g4-rtos-demo.dtbo
ATDE demo$ cp "$KERNELDIR/arch/arm64/boot/dts/freescale/armadillo_iotg_g4-rtos-demo.dtbo" dts/
ATDE demo$ ls dts/armadillo_iotg_g4-rtos-demo.dtbo
armadillo_iotg_g4-rtos-demo.dtbo
```

このリポジトリには、ビルド済みの dtbo がすでに含まれています。

### Armadillo にインストール

以下の二つの方法のどちらかの手順でインストールしてください。

#### SWU ファイルでのインストール

Armadillo Base OS ではアップデートを SWU ファイルで転送しています。SWU ファイルは [mkswu](https://github.com/atmark-techno/mkswu) コマンドで生成できます。
mkswu の必要な設定は [製品マニュアル](https://manual.atmark-techno.com/armadillo-iot-g4/armadillo-iotg-g4_product_manual_ja-1.20.0/ch10.html#sct.mkswu) に紹介しています。

`rtos.desc` には、デモの実行に必要なファイルをインストールする為のサンプルが含まれています。
モジュールのビルドに使用されたカーネルのバージョンはチェックされない為、モジュールがロードできなかった場合は、事前にビルドしたカーネルをインストールした後に、この手順をやり直してください。

```
ATDE demo$ mkswu --update-version rtos.desc
ATDE demo$ mkswu rtos.desc
```

rtos.swu が作成されるので、インストールします。

#### 手動インストール

1. 必要なファイルを USB メモリにコピーします。以下のファイルは、`release` ビルドの想定です。

```
ATDE demo$ sudo mount /dev/sda1 /mnt
ATDE demo$ cp rtos/*/*/armadillo_rtos_demo.* /mnt
ATDE demo$ cp linux/rpmsg_armadillo.ko /mnt
ATDE demo$ cp dts/armadillo_iotg_g4-rtos-demo.dtbo /mnt
ATDE demo$ cp boot_script/boot.txt /mnt
ATDE demo$ umount /mnt
```

2. `rtos/armgcc/release/armadillo_rtos_demo.bin` を Armadillo にコピーします。
u-boot でロードする際に .bin ファイルを使います。

```
armadillo:~# mount /dev/sda1 /mnt
armadillo:~# cp /mnt/armadillo_rtos_demo.bin /boot/
armadillo:~# persist_file -v /boot/armadillo_rtos_demo.bin
```

3. カーネルモジュールを Armadillo にコピーします。
カーネルのバージョンが異なる場合は、「modprobe」コマンドが失敗します。

```
armadillo:~# mkdir /lib/modules/$(uname -r)/extras
armadillo:~# cp /mnt/rpmsg_armadillo.ko /lib/modules/$(uname -r)/extras/
armadillo:~# depmod
armadillo:~# modprobe rpmsg_armadillo
armadillo:~# persist_file -rv /lib/modules
```

4. DTB overlay を有効にして、Cortex M7 用のメモリを予約します。
（すでに他の dtbo ファイルが指定されている場合は、空白で区切って dtbo ファイル名を追加します）

```
armadillo:~# cp /mnt/armadillo_iotg_g4-rtos-demo.dtbo /boot/
armadillo:~# vi /boot/overlays.txt
fdt_overlays=armadillo_iotg_g4-rtos-demo.dtbo
armadillo:~# persist_file -v /boot/armadillo_iotg_g4-rtos-demo.dtbo /boot/overlays.txt
```

5. u-boot に cortex M7 を起動させます。

```
armadillo:~# cp /mnt/boot.txt /boot/
armadillo:~# mkbootscr /boot/boot.txt
armadillo:~# persist_file -v /boot/boot.*
```

6. カーネルバージョンと追加したファイルを永続化します。
`persist_file` でメモリ上の overlayfs から eMMC に保存していましたが、
アップデートの場合はカーネルが更新されてインストールしたファイルがなくなります。
`/etc/swupdate_preserve_files` に追加することで、Armadillo Base OS の更新でも
なくならないようにします。

```
armadillo:~# vi /etc/swupdate_preserve_files
# 以下の2行を追加します
POST /boot
POST /lib/modules
armadillo:~# persist_file -v /etc/swupdate_preserve_files
```

7. 再起動します。

```
armadillo:~# reboot
```

### 動作確認

インストールが成功した場合に FreeRTOS が cortex M7 上で起動しています。
Linux側にカーネルモジュールがロードされて、RTOS との通信ができます。

1. `dmesg` に rpmsg とモジュールの情報があります。
出力例:

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

2. モジュールが　`/sys/class/remoteproc/remoteproc0/remoteproc0\#vdev0buffer/virtio0/virtio0.rpmsg-armadillo-demo-channel.-1.30/` に書き込み可能なファイルを作ります。

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

このデモでは

 - `set_loglevel` で RTOS から Linux に送信されるメッセージの量を調整できます。
上記の出力例の `[remote]` を含むメッセージは RTOS から rpmsg チャネル経由で出力されました。
0 を書き込むとすべてのメッセージが表示されて、4 を書き込むと何も表示されなくなります。

 - `set_gpio` で GPIO1 pin 15 を制御できます。Armadillo IoT G4 と Armadillo X2 では、CON11 のピン 24 で確認できます。
1 を書き込むと High になり、0 を書き込むと Low になります。

## 開発のための説明

### RTOS 部分の説明

`rtos` ディレクトリには次のような FreeRTOS のサンプルアプリケーションが含まれています：
 - Linux と通信する為の rpmsg チャネルをセットアップします。
 - チャネルのメッセージを処理して GPIO1 pin 15 を制御します。
 - Linuxからデバッグできるように、 rpmsg チャネル経由でログ メッセージを送信します。

ファイルは以下の様に別けています：
 - `main_remote.c` にアプリケーションのコードがあります。
   * `main` で設定の処理とタスクの生成を行っています。
   * `app_task` で rpmsg のチャネルをセットアップして、メッセージを処理します。
   * メッセージのプロトコルは `../common/protocol.h` に定義されています。
 - `pin_mux.c` で入出力のピン設定を行っています。
例えば、CAN を使用したい場合は `IOMUXC_SAI5_RXD1_CAN1_TX` と `IOMUXC_SAI5_RXD2_CAN1_RX` ピンを設定する必要があります。
https://mcuxpresso.nxp.com の Pins Tool を使うと、このファイルを GUI で編集できます（NXP のアカウントが必要です）
 - `armgcc/MIMX8ML8xxxxx_cm7_ram.ld` と `iar/MIMX8ML8xxxxx_cm7_ram.icf` は GCC と IAR が `release` モードで使っているリンカースクリプトです。
このファイルには以下の設定が含まれます：
   * 割り込みハンドラーと .text (コードセクション）は TCM の 0x0 から 0x20000 までにマッピングされています（.text は 127KB です）
   * data は 0x20000000 の `m_data` (TCM, 128KB) と 0x80000000 の `m_data2` (DDR, 16MB) の 2ヶ所があります。 heap と stack は TCM に保存されています。
   * アドレスと物理的なマッピングは `board.c` で行われています。
 - FreeRTOS のコード自体は `mcuxsdk/rtos/freertos/freertos-kernel` にあります。
 - CPU 固有のファイルは `mcuxsdk/core/devices/MIMX8ML8` にあります。このデモは `MIMX8ML8DVNLZ` のパッケージを使っています。
 - NXP が提供しているドライバは `mcuxsdk/core/drivers` と `mcuxsdk/middleware/multicore` にあります。
 - MCUXpresso の SDK 全体をダウンロードした場合には、 `mcuxsdk/examples/evkmimx8mp` が参考になります。Armadillo に近いボードです。


Cortex M7 には FreeRTOS 以外のアプリケーションも使えますが、Linux との通信が必要な場合は rpmsg を推奨しています。
ベアメタルアプリケーションのサンプルとしては、 https://github.com/OpenAMP/open-amp が参考になります。

### Linux モジュールの説明

Linux カーネルのモジュールは、Linux と Cortex M7 が通信する為に必要です。
このモジュールは rpmsg の「rpmsg-armadillo-demo-channel」が作成されるときに自動的にロードされます。
（`../common/protocol.h` に設定されている `RPMSG_LITE_NS_ANNOUNCE_STRING` でマッチングしています）

ロード処理の際にモジュールのバージョン（`VERSION`) を rpmsg チャネルに書き込んで、同じバージョンが戻るのを待ちます。
バージョン確認ができたら：
 - sysfs にファイルを作って、ユーザスペースから制御できるようにしています。
sysfs のファイルは `rpmsg_armadillo_attrs` で定義されていて、各ファイルは `DEVICE_ATTR_WO` で処理が決められています。
他に `DEVICE_ATTR_RO` と `DEVICE_ATTR_RW` で読み取り専用と読み書き可能なファイルも作成できます。その場合は `show` 関数も実装する必要があります。
 - rpmsg チャネルからログメッセージを読んで表示します。

### DTS の説明

この DTB overlay では、以下の内容が設定されています：
 - Cortex M7 が使用するメモリを Linux から使えないようにしています。
   * 0x80000000 からの 16MB 領域（リンカースクリプトの `m_data2`)
   * rpmsg とファームウェアロード用のバッファー（`vdev0vring[0-1]`, `vdevbuffer` and `rsc_table`)
 - `remoteproc` のノード (`imx8mp-cm7`) はコア制御と rpmsg に使用されています。
 - M7 コアから使われているデバイスを Linux から使えないようにしています。
M7 コアからデバイスを利用する場合は、 Linux から利用されないようにする必要があります。
以下の無効化方法があります。
    * `gpio-hog` で gpio をロックできます。 `input`, `output-low` または `output-high` を設定する必要があります。output の場合は Linux の起動時に一度書き込みが行われていますのでご注意ください。Linux から gpio を利用しない場合は、この手順は不要です。
    * ハードウェアデバイス毎に無効化できます。例えば、 `&flexcan1 { status = "disabled"; };` で CAN1 を Linux側で無効化できます。Armadillo の場合は CAN1 をすでに無効化していますので、新たに追加する必要はありません。Armadillo で使用しているデバイスは Linux カーネルソースの `arch/arm64/boot/dts/freescale/armadillo_iot_g4.dts` から確認できます。

### 再起動不要のテスト

Cortex M7 コアを u-boot で一度起動した後であっても、 remoteproc インターフェース経由でリロードできます。
この手順はテストのための手順で、再起動する際に変更がなくなります。

1. Cortex M7 コアと linux のモジュールを停止します

```
armadillo:~# echo stop > /sys/class/remoteproc/remoteproc0/state
armadillo:~# modprobe -r rpmsg_armadillo
```

2. 必要なファイルを更新します。M7 に使われているファームウェアは .elf ファイルを使っています。このファイルは、ツールチェーンが gcc の場合のみに作成されます。

```
armadillo:~# cp /mnt/armadillo_rtos_demo.elf /lib/firmware/armadillo_rtos_demo.elf
armadillo:~# cp /mnt/rpmsg_armadillo.ko /lib/modules/$(uname -r)/extras
```

3. M7コアを起動します。Linux のモジュールは rpmsg の作成の際に自動的にロードされます。

```
armadillo:~# echo armadillo_rtos_demo.elf > /sys/class/remoteproc/remoteproc0/firmware
armadillo:~# echo start > /sys/class/remoteproc/remoteproc0/state
```
