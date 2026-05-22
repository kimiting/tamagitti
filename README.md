# Pico Tama

XIAO ESP32C6、1.69inch ST7789V2 LCD、土壌水分センサー、照度センサー、タッチセンサーを使うPicoたまごっち風スケッチです。

## 初めて実行する手順

### 1. Arduino IDEを入れる

Arduino IDEを公式サイトからインストールします。

https://www.arduino.cc/en/software

### 2. ESP32ボードを追加する

Arduino IDEを開いて、`設定` の `追加のボードマネージャのURL` に次を追加します。

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

そのあと、`ツール` → `ボード` → `ボードマネージャ` で `esp32` を検索し、`esp32 by Espressif Systems` をインストールします。

### 3. ライブラリを入れる

このスケッチは `st7789v2.h` を使います。`XIAO_ST7789V2_LCD_Display-main` ライブラリをArduinoの `libraries` フォルダに入れてください。

インストール先の例:

```text
~/Documents/Arduino/libraries/XIAO_ST7789V2_LCD_Display-main
```

### 4. このフォルダを開く

Arduino IDEで、このフォルダを開きます。

```text
cat_tama
```

中に次のファイルがある状態にしてください。

- `cat_tama.ino`
- `pico_sprites.h`
- `images.h`

### 5. ボードとポートを選ぶ

XIAO ESP32C6をUSBで接続します。

Arduino IDEの `ツール` から次を選びます。

- ボード: `XIAO_ESP32C6`
- ポート: `/dev/cu.usbmodem...` のようなUSBポート

### 6. 書き込む

Arduino IDE左上の `→` ボタンを押すと、コンパイルと書き込みが始まります。

書き込み後、LCDにPicoが表示されれば成功です。

## VS Codeで実行する場合

VS Codeでは、内蔵ターミナルからArduino CLIを使う方法が安定しています。

### 1. VS Codeでフォルダを開く

VS Codeでこのフォルダを開きます。

```text
cat_tama
```

`cat_tama.ino`、`pico_sprites.h`、`README.md` が見えていれば大丈夫です。

### 2. ターミナルを開く

VS Code上部メニューの `Terminal` → `New Terminal` を押します。

ターミナルの場所がこのフォルダになっていることを確認します。

```bash
pwd
```

このリポジトリのフォルダが表示されればOKです。

```text
/path/to/tamagitti
```

違う場所にいる場合は移動します。

```bash
cd /path/to/tamagitti
```

### 3. Arduino CLIが使えるか確認

```bash
arduino-cli version
```

バージョンが表示されればOKです。

### 4. ボードを接続してポートを確認

XIAO ESP32C6をUSBで接続して、次を実行します。

```bash
arduino-cli board list
```

`/dev/cu.usbmodem...` のようなポートを探します。

例:

```text
/dev/cu.usbmodem1101
```

### 5. コンパイル

```bash
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C6 .
```

`Sketch uses ... bytes` のように表示されればコンパイル成功です。

### 6. 書き込み

ポート名は `arduino-cli board list` で出たものに合わせてください。

```bash
arduino-cli upload -p <PORT> --fqbn esp32:esp32:XIAO_ESP32C6 .
```

書き込み後、LCDにPicoが表示されれば成功です。

### 6.5. 速く書き込みたい場合

このフォルダ名と `cat_tama.ino` の名前が違うため、Arduino CLIで直接このフォルダを扱うと失敗する場合があります。
次のスクリプトは一時フォルダでコンパイルして、通常の開発時はアプリ本体だけを書き込みます。

```bash
./upload_fast.sh <PORT>
```

初回書き込みやパーティション設定を変えた時は、通常のArduino IDE/CLIで全体を書き込んでください。

### 7. シリアルモニタを見る

センサー値を確認したい場合は、次を実行します。

```bash
arduino-cli monitor -p <PORT> --config baudrate=115200
```

終了するときは `Ctrl + C` を押します。

書き込み時に `port is busy` と出る場合は、シリアルモニタを閉じてからもう一度アップロードしてください。

## Arduino CLIで実行する場合

VS Codeやターミナルから実行したい場合は、Arduino CLIでも書き込めます。

### 1. ボード一覧を確認

```bash
arduino-cli board list
```

`/dev/cu.usbmodem...` のようなポートを探します。

### 2. コンパイル

```bash
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C6 .
```

### 3. 書き込み

ポート名は自分の環境に合わせて変更してください。

```bash
arduino-cli upload -p <PORT> --fqbn esp32:esp32:XIAO_ESP32C6 .
```

### 4. センサー値を見る

土壌センサーの値を確認したい時は、シリアルモニタを開きます。

```bash
arduino-cli monitor -p <PORT> --config baudrate=115200
```

画面下にも `S####` の形で土壌センサーの安定値が表示されます。

## 状態

状態は5つだけです。

- `NIGHT`: 夜。Picoが眠る。背景が暗くなる
- `WET`: 湿潤
- `OK`: 普通
- `DRY`: 乾燥
- `TOUCH`: タッチ反応

画面下部の丸い状態ドットは表示しません。Picoのアニメーションと短い状態テキストで状態を表します。
状態テキストは状態ごとに色が変わります。

## なめらか化

- 土壌と照度の判定にヒステリシスを入れて、境界付近でパカパカ切り替わらないようにしています。
- 状態が変わる時は、今のアニメーションを最後まで再生してから次の状態へ移ります。
- 状態変更時は基本的にPicoと下の状態テキストだけを更新します。昼/夜の背景色が切り替わる時だけ全画面を塗り直します。
- 夜は最優先で、夜判定になるとタッチや土壌状態よりも睡眠アニメーションが優先されます。
- Picoは `112x120` の画像をそのまま描画します。生成画像は色を揃えて、端で切れた小さいパーツが反対側に回り込まないようにしています。
- 夜状態では立ったままうとうとする `pico_sleep` アニメーションを再生し、画面左上に `zzz` を表示します。
- 湿潤状態では元Picoの連続フレームをベースにした、足元に水たまりが出る滑らかな `pico_wet` アニメーションを再生します。
- タッチ状態では元Picoの連続フレームをベースにした、手を上げて喜ぶ滑らかな `pico_touch` アニメーションを再生します。
- タッチは一度始まったら、タッチ用モーションを3周再生してから次の状態に戻ります。

## 調整する値

- `SOIL_DRY_TH`: 乾燥判定
- `SOIL_WET_TH`: 湿潤判定
- `SOIL_HYST`: 土壌判定の戻り幅
- `SOIL_DRY_IS_HIGH`: 乾くほど値が大きいセンサーなら `true`、逆なら `false`
- `SOIL_FILTER_ALPHA`: 土壌センサー値のなめらかさ。小さいほどゆっくり反応
- `LIGHT_NIGHT_TH`: 夜判定
- `LIGHT_HYST`: 夜判定の戻り幅
- `ANIM_INTERVAL_MS`: アニメーション速度
