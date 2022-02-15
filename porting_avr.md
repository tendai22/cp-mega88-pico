# AVRへの移植

ATmega328+秋月DRAMへの移植。

## ピン割り当て

|ピン<br>番号|端子<br>名|接続<br>先|説明|
|--|--|--|--|
|23|PC0|TX|シリアルのTXに接続|
|24|PC1|RX|シリアルのRXに接続|
|28|PC5|CS|SDCardのCSに接続|
|26|PC3|DI|SDCardのDIに接続|
|25|PC2|SCK|SDcardのSCKに接続|
|27|PC4|DO|SDcardのDOに接続|
|2|PD0|AD0|DRAMのA0-IO0に接続|
|3|PD1|AD1|DRAMのA1-IO1に接続|
|4|PD2|AD2|DRAMのA2-IO2に接続|
|5|PD3|AD3|DRAMのA3-IO3に接続|
|6|PD4|AD4|DRAMのA4-IO4に接続|
|11|PD5|AD5|DRAMのA5-IO5に接続|
|12|PD6|AD6|DRAMのA6-IO6に接続|
|13|PD7|AD7|DRAMのA7-IO7に接続|
|14|PB0|/OE|DRAMの/OEに接続|
|15|PB1|/RAS|DRAMの/RASに接続|
|16|PB2|/CAS|DRAMの/CASに接続|
|17|PB3/MOSI|/WE<br>/MOSI|ProgrammerのMOSIに接続<br>DRAMの/WEに接続|
|18|PB4/MISO|MISO|ProgrammerのMISOに接続|
|19|PB5/SCK|SCK|ProgrammerのSCKに接続|
|1|RESET|RESET|ProgrammerのRESETに接続|

## ビルド
* arduino-cliを使う。
  + src/の下にicoファイルを置く。`src/src.ico`
  + コンパイルオプションを全て`hardware_config.h`に置く。
* 必要なファイルだけ残し他を消す。スクリプト`delete_...`を作ったが消えた。
* printf書式文字列をフラッシュ領域に格納し、SRAMを空ける。`debug.[ch]`の作成、`debug/debug0`関数。


## デバッグ

* `con_getchar`が文字を返さない。最初のリターンの前に`asm volatile("nop");`を入れると動くようになる。コンパイラ最適化の効きすぎ?
* debug関数。static グローバルなchar配列(101バイト)上に`sprintf_P`関数を用いて文字列を展開し、それを`con_puts`で出力するようにした。
* LFをCRLFに展開する出力関数: `con_puts2`を作成。

