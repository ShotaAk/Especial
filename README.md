# Especial

![Especial](https://github.com/ShotaAk/especial/blob/images/images/especial.jpg)

ESP32を搭載したマイクロマウスのプログラム

# Requirements

- Especial (Optional)
  - 私が作成したオリジナルマウスがあると、本プログラムの全機能を動かせます
- ESP32-WROOM-32D
- ESP-IDF stable (v4.0.1)
  
# Installation

## ESP-IDFのインストール

公式マニュアルを参照してください。


[ESP-IDF Get Started | stable](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html)

## especialのビルド

```sh
# ESP-IDF の環境設定
$ alias get_idf='. $HOME/esp/esp-idf/export.sh'
$ get_idf

# Especialのクローン
cd ~/esp
git clone https://github.com/ShotaAk/especial

# 走行プログラムのビルド
cd especial
$ idf.py build
## 書き込み
$ idf.py -p /dev/ttyUSB0 flash monitor

# サンプルプログラムのビルド
$ cd examples/0_hello_world 
$ idf.py build
## 書き込み
$ idf.py -p /dev/ttyUSB0 flash monitor
```

# 現在の開発工程

[ここ見て](https://github.com/ShotaAk/especial/milestones)

# 問題点一覧

[ここ見て](https://github.com/ShotaAk/especial/issues)

# その他
**Especialと全く同じ**マイクロマウスが[こちらのブログ](https://rt-net.jp/mobility/archives/category/original/shota-originalmouse)で紹介されているようです。
