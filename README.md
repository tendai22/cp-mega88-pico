# CP/Mega88 for Raspberry Pi Pico

A porting toyoshima-san's cp-mega88 to Raspberry Pi Pico.

The original cp-mega88 locates https://github.com/toyoshim/cp-mega88.  It is an i8080 emulator running on ATmega88, and on which CP/M can run.  I would like to have a CP/M machine on my 'z80_pico' project(https://github.com/tendai22/z80_pico), in which an actual Z80 CPU runs with a modern Raspberry Pi Pico.  I choose cp-mega88 as a base implementation of CP/M.  This project is for porting cp-mega88 on Raspberry Pi Pico, and on z80_pico.

## とりあえずのビルド方法(Japanese)

* pico-sdkをインストールしてください。私はWSL派で、ビルドはWSL(Ubuntu20)で行い、ビルド結果のuf2ファイルをWindowsデスクトップにコピーし、Picoへの書き込みはWindows環境のUSBドライブにドラッグアンドドロップでやっています。USBシリアルをwsl側でアクセスできるようにする自信がない、一方で、WLS環境からファイルをWindows環境にコピーは簡単にできるので。

* WSLのインストール: これは[このあたり](https://docs.microsoft.com/ja-jp/windows/wsl/install)でしょうか。
* WSLでUbuntu20インストールする。

> Ubuntu18の場合、cmakeのバージョンが古い(3.16以上が必要なところ、3.8 or 3.10が入る)ので、apt-get で cmakeをインストールしても使えない。最新版の cmake をビルドしてみたが、libssl-devが必要と言われたりして面倒くさい。今回の趣旨にそぐわないので、Ubuntu18は忘れてください。

* 起動して、以下のコマンドをたたく。
```
$ sudo apt update
$ sudo apt upgrade
$ sudo apt install gcc-arm-none-eabi build-essential
$ sudo apt install git
$ sudo apt install gcc cmake libnewlib-arm-none-eabi
  # もう少し入れんといかん気もするが思い出せない。各自の健闘を祈る 
```
> gitは最初から入っているそうです。

* pico-sdkをインストールする([技術評論社のページでどうかな](https://gihyo.jp/admin/serial/01/ubuntu-recipe/0684?page=2))。
```
$ git clone -b master https://github.com/raspberrypi/pico-sdk.git
$ cd pico-sdk
$ git submodule update --init
$ cd ..
$ git clone -b master https://github.com/raspberrypi/pico-examples.git
```
* 環境変数PICO_SDK_PATHをセットしておく。
```
$ export PICO_SDK_PATH="$PWD/pico-sdk"
```
~/.bashrc, ~/.loginに入れて、source コマンドでスクリプトを実行しておくとよい。
```
$ echo 'PICO_SDK_PATH="$PWD/pico-sdk"' >> ~/.bashrc
$ source ~/.bashrc
```
`source`コマンドは実行中のシェルでスクリプト実行させることを意味する。
`~/.bashrc`に環境変数への代入を書き込んでおくと、次回のログインで実行され有効となる。

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
* ビルドする。`cmake`コマンドは引数にディレクトリを渡すことに注意。ここではカレントディレクトリを指定する。
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
* Teraterm側は、[ファイル]->[転送]->[XMODEM]->[送信]で、出てくるダイアログでディスクイメージを選ぶ。`cpm2-1.dsk`がよい。
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
A: DUMP     COM : SDIR     COM : SUBMIT   COM : ED       COM
A: STAT     COM : BYE      COM : RMAC     COM : CREF80   COM
A: LINK     COM : L80      COM : M80      COM : SID      COM
A: RESET    COM : WM       HLP : ZSID     COM : MAC      COM
A: TRACE    UTL : HIST     UTL : LIB80    COM : WM       COM
A: HIST     COM : DDT      COM : Z80ASM   COM : CLS      COM
A: SLRNK    COM : MOVCPM   COM : ASM      COM : LOAD     COM
A: XSUB     COM : LIB      COM : PIP      COM : SYSGEN   COM
A>stat *.*

 Recs  Bytes  Ext Acc
   64     8k    1 R/W A:ASM.COM
    1     1k    1 R/W A:BYE.COM
    1     1k    1 R/W A:CLS.COM
   32     4k    1 R/W A:CREF80.COM
   38     5k    1 R/W A:DDT.COM
    3     1k    1 R/W A:DUMP.COM
   52     7k    1 R/W A:ED.COM
   21     3k    1 R/W A:HIST.COM
   10     2k    1 R/W A:HIST.UTL
   84    11k    1 R/W A:L80.COM
   56     7k    1 R/W A:LIB.COM
   37     5k    1 R/W A:LIB80.COM
  122    16k    1 R/W A:LINK.COM
   14     2k    1 R/W A:LOAD.COM
  157    20k    2 R/W A:M80.COM
   92    12k    1 R/W A:MAC.COM
   76    10k    1 R/W A:MOVCPM.COM
   58     8k    1 R/W A:PIP.COM
    1     1k    1 R/W A:RESET.COM
  106    14k    1 R/W A:RMAC.COM
  119    15k    1 R/W A:SDIR.COM
   61     8k    1 R/W A:SID.COM
   68     9k    1 R/W A:SLRNK.COM
   40     5k    1 R/W A:STAT.COM
   10     2k    1 R/W A:SUBMIT.COM
    8     1k    1 R/W A:SYSGEN.COM
    9     2k    1 R/W A:TRACE.UTL
   82    11k    1 R/W A:WM.COM
   23     3k    1 R/W A:WM.HLP
    6     1k    1 R/W A:XSUB.COM
  193    25k    2 R/W A:Z80ASM.COM
   80    10k    1 R/W A:ZSID.COM
Bytes Remaining On A: 11k

A>
```
てな感じです。如何でしょうか?
