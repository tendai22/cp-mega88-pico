# CP/Mega88 for Raspberry Pi Pico

A porting toyoshima-san's cp-mega88 to Raspberry Pi Pico.

The original cp-mega88 locates https://github.com/toyoshim/cp-mega88.  It is an i8080 emulator running on ATmega88, and on which CP/M can run.  I would like to have a CP/M machine on my 'z80_pico' project(https://github.com/tendai22/z80_pico), in which an actual Z80 CPU runs with a modern Raspberry Pi Pico.  I choose cp-mega88 as a base implementation of CP/M.  This project is for porting cp-mega88 on Raspberry Pi Pico, and on z80_pico.

## とりあえずのビルド方法(Japanese)

* pico-sdkをインストールしてください。私はWSL派で、ビルドはWSL(Ubuntu20)で行い、ビルド結果のuf2ファイルをWindowsデスクトップにコピーし、Picoへの書き込みはWindows環境のUSBドライブにドラッグアンドドロップでやっています。USBシリアルをwsl側でアクセスできるようにする自信がない、一方で、WLS環境からファイルをWindows環境にコピーは簡単にできるので。

* WSLのインストール: これは[このあたり](https://docs.microsoft.com/ja-jp/windows/wsl/install)でしょうか。
* WSLでUbuntu20(Ubuntu18でも大丈夫だと思うけど)インストールしたら、起動して、以下のコマンドをたたく。
```
$ sudo apt update
$ sudo apt install gcc-arm-none-eabi build-essential
$ sudo apt install git
  # もう少し入れんといかん気もするが思い出せない。各自の健闘を祈る 
```
* pico-sdkをインストールする([技術評論社のページでどうかな](https://gihyo.jp/admin/serial/01/ubuntu-recipe/0684?page=2))。
```
$ sudo apt install gcc cmake gcc-arm-none-eabi libnewlib-arm-none-eabi
$ git clone -b master https://github.com/raspberrypi/pico-sdk.git
$ cd pico-sdk
$ git submodule update --init
$ cd ..
$ git clone -b master https://github.com/raspberrypi/pico-examples.git
```
* 環境変数PICK_SDK_PATHをセットしておく。
```
$ export PICO_SDK_PATH="$PWD/pico_sdk"
# ~/.bashrc, ~/.loginに入れておいてもよい。
```
* cp-mega88-picoをcloneする
```
$ cd somewhere
$ git clone https://github.com/tendai22/cp-mega88-pico.git
$ cd cp-mega88-pico
```
* いまのところ、developブランチでしか動かないので、developブランチをチェックアウトする。
```
$ git checkout develop
```
* ビルドする。`CMakeFile.txt`にPICO_SDK_PATHの定義が書かれているので、これを消しておくか、自分の環境のpico-sdkのありかを指すように変更しておく。
```
$ cmake .
$ make
```
* これで`cpmega88.uf2`ファイルができるので、デスクトップにコピーする。  
私の環境では、デスクトップは`D:\tendai22plus\Desktop`においてあるので、
WSL環境からWindows環境へのコピーを以下の要領で行う。
```
$ cp cpmega88.uf2 /mnt/d/tendai22plus/Desktop
```
* デスクトップに`cpmega88.uf2`ファイルが湧いてくるので、Raspberry Pi Picoの基板のスイッチを押しながらUSBケーブルをPCにつなぐ(通電する)と、USBドライブが湧いてくる(ドライブ名`RPI-RP2`、うちの環境では`E:`ドライブ)ので、そこにドラッグアンドドロップする。これで、ファームウェアがPicoに焼きこまれ再起動する。
* Windows環境でTeratermを起動して、 湧いてきたCOMポートにつなぎ、PicoのUSBケーブルを抜き差しする。
```
booting CP/Mega88 done.
memory test: 65535/65535
SDC: ok
EEPROM: load
CP/Mega88>
```
というメッセージが出て`CP/Mega88`が起動する。
* `r`コマンドを叩いてみよう。ヘルプメッセージが出てくる。
```
CP/Mega88>r
monitor commands
 r                : reset
 b                : boot CP/M 2.2
 wp <on/off>      : file system write protection
 a <on/off>       : auto boot
 fd <addr>        : dump flash disk
 mr <addr>        : memory read from <addr>
 mw <addr>,<data> : memory write <data> to <addr>
 md <addr>        : memory dump from <addr>, 256bytes
 xr               : xmodem receive
 xs               : xmodem send
 vt <on/off>      : vt100 compatible mode
CP/Mega88>
```
* `xr`, `xs`コマンドが新設されたもの。  
  + `xr`(XMODEM receive)は、ディスクイメージをPicoに書き込む。
  + `xs`は、Pico中のディスクイメージをダウンロードしてWindows環境にファイルを作る。
* ディスクイメージを入手する。[`z80pack`](https://github.com/udo-munk/z80pack.git)のgithubプロジェクトをcloneして、
```
$ cd someotherwhere
$ git clone https://github.com/udo-munk/z80pack.git
$ cd z80pack/cpmsim/disks/library
$ ls
cpm13.dsk  cpm1975.dsk  cpm2-2.dsk      cpm3-1.dsk
cpm3-8080-1.dsk  hd-tools.dsk    i8080tests.dsk  mpm-2.dsk
cpm14.dsk  cpm2-1.dsk   cpm2-62khd.dsk  cpm3-2.dsk  
cpm3-8080-2.dsk  hd-toolsrc.dsk  mpm-1.dsk       z80tests.dsk
```
てな感じで。このファイル群をWindows環境にコピーしておく。
```
$ cp *.dsk /mnt/d/tendai22plus/Desktop/disks
```
* CPMega88で`xr`コマンドをたたく。すかさずTeratermのXMODEM(送信)機能でディスクイメージをアップロードする。
```
(CPMega88側)
CPMega88> xr
```
* Teraterm側は、[ファイル]->[転送]->[XMODEM]->[送信]で、出てくるダイアログでディスクイメージを選ぶ。`cmm2-1.dsk`がよい。
* 無事アップロードと書き込みが完了すると
```
CC
write 256256 (3E900) bytes, crc = A510
CP/Mega88>
```
と、転送バイト数とCRC16値が出る。さぁCP/Mをブートだ
```
CP/Mega88>b
64K CP/M Vers. 2.2 (Z80 CBIOS V1.2 for Z80SIM, Copyright 1988-2007 by Udo Munk)

A>dir
A: DUMP     COM : MBASIC   COM : SUBMIT   COM : ED       COM
A: STAT     COM : BYE      COM : ASCIIART BAS : TRACE    UTL
A: HIST     UTL : RESET    COM : M        SUB : CLS      COM
A: LOAD     COM : XSUB     COM : PIP      COM : SYSGEN   COM
A>stat *.*

 Recs  Bytes  Ext Acc
    3     1k    1 R/W A:ASCIIART.BAS
    2     1k    1 R/W A:BYE.COM
    1     1k    1 R/W A:CLS.COM
    3     1k    1 R/W A:DUMP.COM
   52     7k    1 R/W A:ED.COM
   10     2k    1 R/W A:HIST.UTL
   14     2k    1 R/W A:LOAD.COM
    1     1k    1 R/W A:M.SUB
  190    24k    2 R/W A:MBASIC.COM
   58     8k    1 R/W A:PIP.COM
    2     1k    1 R/W A:RESET.COM
   40     5k    1 R/W A:STAT.COM
   10     2k    1 R/W A:SUBMIT.COM
    8     1k    1 R/W A:SYSGEN.COM
    9     2k    1 R/W A:TRACE.UTL
    6     1k    1 R/W A:XSUB.COM
Bytes Remaining On A: 181k

A>
```
てな感じです。如何でしょうか?
