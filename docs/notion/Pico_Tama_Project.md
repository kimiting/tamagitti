# Pico Tama 誰でも作れる手順書

最終更新: 2026-05-21

GitHub:
https://github.com/kimiting/tamagitti

## これは何？

Pico Tamaは、植木鉢の状態を小さなキャラクターで見せる電子工作です。

土が乾いたら `DRY`、水が多い時は `WET`、暗くなったら `NIGHT`、タッチすると `TOUCH` になります。画面にはPicoというキャラクターが表示され、状態に合わせてアニメーションします。

## 完成するとできること

- 土の乾き具合が画面でわかる
- 暗くなるとPicoがうとうと眠る
- タッチするとPicoが反応する
- シリアルモニタでセンサー値を確認できる
- GitHubでコードを管理できる

## 必要なもの

| 種類 | 必要なもの |
| --- | --- |
| マイコン | XIAO ESP32C6 |
| 画面 | 1.69inch ST7789V2 LCD |
| センサー | 土壌水分センサー |
| センサー | 照度センサー |
| センサー | タッチセンサー |
| ケーブル | USBケーブル |
| ソフト | Arduino IDE または Arduino CLI |
| ライブラリ | XIAO_ST7789V2_LCD_Display |

## まず全体の流れ

初めての人は、この順番で進めれば大丈夫です。

1. 部品を用意する
2. 配線する
3. Arduino IDEを入れる
4. ESP32ボードを追加する
5. LCDライブラリを入れる
6. GitHubからコードを取る
7. Arduino IDEで開く
8. ボードとポートを選ぶ
9. 書き込む
10. 画面とセンサー値を確認する

## 配線

| 部品 | 接続先 |
| --- | --- |
| 土壌水分センサー | A0 |
| 照度センサー | A2 |
| タッチセンサー | D1 |
| LCD DIN | D10 |
| LCD CLK | D8 |
| LCD CS | D3 |
| LCD DC | D4 |
| LCD RST | D5 |
| LCD BL | D6 |

配線後、USBでXIAO ESP32C6をMacに接続します。

## Arduino IDEで動かす方法

一番わかりやすい方法です。

### 1. Arduino IDEを入れる

公式サイトからArduino IDEをインストールします。

https://www.arduino.cc/en/software

### 2. ESP32ボードを追加する

Arduino IDEを開きます。

`設定` の `追加のボードマネージャのURL` に次を追加します。

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

そのあと、`ツール` → `ボード` → `ボードマネージャ` で `esp32` を検索し、`esp32 by Espressif Systems` をインストールします。

### 3. LCDライブラリを入れる

このプログラムは `st7789v2.h` を使います。

ライブラリはこの場所に入れます。

```text
~/Documents/Arduino/libraries/XIAO_ST7789V2_LCD_Display-main
```

まだ入っていない場合は、ターミナルで次を実行します。

```bash
mkdir -p ~/Documents/Arduino/libraries
git clone https://github.com/limengdu/XIAO_ST7789V2_LCD_Display.git ~/Documents/Arduino/libraries/XIAO_ST7789V2_LCD_Display-main
```

### 4. コードを取る

ターミナルで好きな場所に移動して、次を実行します。

```bash
git clone git@github.com:kimiting/tamagitti.git
cd tamagitti
```

GitHubのSSH設定をしていない人は、HTTPSでも取れます。

```bash
git clone https://github.com/kimiting/tamagitti.git
cd tamagitti
```

Gitがわからない場合は、GitHubのページで `Code` → `Download ZIP` を選び、ZIPを展開して使っても大丈夫です。

### 5. Arduino IDEで開く

Arduino IDEで `cat_tama.ino` を開きます。

Arduino IDEが「スケッチ用の `cat_tama` フォルダを作りますか？」のように聞いてきたら、作成してOKです。その場合は、同じフォルダに `pico_sprites.h` と `images.h` も入れてください。

このフォルダには最低限、次のファイルが必要です。

- `cat_tama.ino`
- `pico_sprites.h`
- `images.h`

### 6. ボードとポートを選ぶ

Arduino IDEの `ツール` から次を選びます。

| 項目 | 選ぶもの |
| --- | --- |
| ボード | `XIAO_ESP32C6` |
| ポート | `/dev/cu.usbmodem...` のようなUSBポート |

### 7. 書き込む

Arduino IDE左上の `→` ボタンを押します。

コンパイルと書き込みが終わって、LCDにPicoが表示されれば成功です。

## コマンドで動かす方法

慣れてきたらこちらが速いです。

`arduino-cli` が使えるか確認します。

```bash
arduino-cli version
```

### ボードを確認する

```bash
arduino-cli board list
```

例:

```text
/dev/cu.usbmodem1101
```

### 速く書き込む

通常の開発ではこれを使います。

```bash
./upload_fast.sh <PORT>
```

このスクリプトは一時フォルダでコンパイルし、アプリ本体だけを書き込みます。

初回書き込み、パーティション変更、ボード設定変更をした時はArduino IDEで全体を書き込む方が安心です。

### センサー値を見る

```bash
arduino-cli monitor -p <PORT> --config baudrate=115200
```

終了するときは `Ctrl + C` を押します。

## 画面の状態

| 表示 | 意味 | Picoの動き |
| --- | --- | --- |
| OK | 普通 | 通常アニメーション |
| DRY | 乾燥 | 乾いた時のアニメーション |
| WET | 湿潤 | 足元に水たまり |
| TOUCH | タッチ反応 | 手を上げて喜ぶ |
| NIGHT | 夜 | 暗い背景、うとうと、左上にzzz |

画面下には `S####` の形で土壌水分センサーの値が出ます。

## センサー値の調整

調整する場所は `cat_tama.ino` の上の方です。

| 値 | 意味 |
| --- | --- |
| `SOIL_DRY_TH` | 乾燥判定のしきい値 |
| `SOIL_WET_TH` | 湿潤判定のしきい値 |
| `SOIL_HYST` | 判定がパカパカ切り替わらないための余裕 |
| `SOIL_DRY_IS_HIGH` | 乾くほど値が大きいセンサーなら `true` |
| `SOIL_FILTER_ALPHA` | 土壌値のなめらかさ |
| `LIGHT_NIGHT_TH` | 夜判定のしきい値 |
| `LIGHT_HYST` | 夜判定の余裕 |
| `ANIM_INTERVAL_MS` | アニメーション速度 |

調整のコツ:

- まずシリアルモニタで乾いた土の値を見る
- 次に水を入れた土の値を見る
- その中間に `SOIL_DRY_TH` と `SOIL_WET_TH` を置く
- 判定が切り替わりすぎる時は `SOIL_HYST` を大きくする

## よくあるトラブル

### Arduino IDEでボードが出ない

ESP32ボードマネージャが入っていない可能性があります。

`https://espressif.github.io/arduino-esp32/package_esp32_index.json` を追加して、`esp32 by Espressif Systems` をインストールしてください。

### `st7789v2.h` が見つからない

LCDライブラリが入っていません。

次の場所に `XIAO_ST7789V2_LCD_Display-main` を入れてください。

```text
~/Documents/Arduino/libraries/
```

### ポートが見つからない

- USBケーブルを抜き差しする
- 充電専用ケーブルではなく、通信できるUSBケーブルを使う
- Arduino IDEを再起動する
- `arduino-cli board list` で確認する

### `port is busy` と出る

シリアルモニタが開いたままかもしれません。

シリアルモニタを閉じてから、もう一度書き込みます。

### 画面が真っ白・真っ黒

- LCDの配線を確認する
- `DIN`, `CLK`, `CS`, `DC`, `RST`, `BL` の接続先を確認する
- XIAO ESP32C6に正しく書き込めているか確認する

### コンパイル容量がギリギリ

このプロジェクトは画像データが大きいです。

直近のビルド例:

```text
Sketch uses 1306782 bytes (99%) of program storage space.
Maximum is 1310720 bytes.
```

画像フレームを増やすと入りきらない可能性があります。

## 軽くするアイデア

一番効くのはスプライト画像を小さくすることです。

今はPicoが `112x120` ピクセルです。

これを `56x60` にして、表示時に2倍拡大すると、見た目はドット絵っぽくなり、画像データは約4分の1になります。

ほかにも次の方法があります。

- アニメーションのフレーム数を減らす
- 似たフレームを使い回す
- 画像データを圧縮して保存する
- 未使用の画像やフォントを消す

## GitHubで管理する方法

変更したら、次の順番で保存します。

```bash
git status
git add .
git commit -m "変更内容を書く"
git push
```

例:

```bash
git add .
git commit -m "Update sleep animation"
git push
```

## Notion自動更新

このMarkdownはNotionページに同期できます。

同期先:
https://www.notion.so/368c7245a8ab812c9cf8db53bfc1dff0

`main` に push すると、GitHub Actions の `Sync Notion` ワークフローがこのページを更新します。

設定方法は `docs/notion/auto_sync.md` を見てください。

おすすめの子ページ:

- 作業ログ
- センサー調整メモ
- アニメーション案
- 部品リスト
- トラブルシューティング

作業ログには、日付・変更内容・書き込み結果・気づいたことを書いておくと、あとで戻りやすいです。

## 次にやるとよさそうなこと

- 全体のドットを粗くして容量を減らす
- センサーの乾燥値・湿潤値を実測してしきい値を調整する
- 水やり後の余韻アニメーションを追加する
- DRYが長く続いた時のしょんぼり表現を追加する
- タッチ回数でリアクションを変える
