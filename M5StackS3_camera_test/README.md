# M5Stack CoreS3 Camera Test

M5Stack CoreS3のカメラから画像を取得し、ディスプレイにリアルタイム表示するテストプログラムです。

## ✅ 動作確認済み

このプロジェクトは**M5Stack CoreS3で正常に動作確認済み**です。

## 📋 機能

- ✅ カメラの初期化（正しいGPIOピン定義）
- ✅ リアルタイム画像取得（QVGA 320×240）
- ✅ ディスプレイへの高速表示（writePixels最適化）
- ✅ I2Cバス競合の回避
- ✅ PSRAMを活用したダブルバッファリング

## 🔧 ハードウェア要件

- **M5Stack CoreS3**
  - 内蔵カメラ（GC0308）
  - 320×240 IPS LCDディスプレイ
  - 8MB PSRAM
  - ESP32-S3 (Dual-core 240MHz)

## 📦 ソフトウェア要件

- **PlatformIO**
- **Arduino Framework**
- **ライブラリ**:
  - M5Unified@^0.1.16
  - M5GFX@^0.1.16

## 🚀 セットアップ

### 1. PlatformIOのインストール

VS Codeの拡張機能から「PlatformIO IDE」をインストール。

### 2. プロジェクトを開く

```bash
cd M5StackS3_camera_test
code .
```

### 3. ビルド

VS Code下部のステータスバーから「✓ PlatformIO: Build」をクリック。

### 4. M5Stack CoreS3への書き込み

#### 重要: 書き込みモードに入れる

M5Stack CoreS3は手動で書き込みモードに入れる必要があります：

**正しい手順:**
1. **VS Codeで「→ PlatformIO: Upload」をクリック**
2. **Resetボタン（側面の小さいボタン）を長押し**
3. **緑色のLEDが光ったらResetボタンを離す**
4. 書き込みが自動的に開始される

### 5. シリアルモニタの起動

VS Code下部のステータスバーから「🔌 PlatformIO: Serial Monitor」をクリック。

## ⚙️ platformio.ini の重要な設定

```ini
[env:m5stack-cores3]
platform = espressif32@6.4.0
board = m5stack-cores3
framework = arduino

build_flags = 
    -DCORE_DEBUG_LEVEL=4
    -DBOARD_HAS_PSRAM
    -DARDUINO_M5STACK_CORES3
    -DARDUINO_RUNNING_CORE=1
    -DARDUINO_EVENT_RUNNING_CORE=1

lib_deps = 
    m5stack/M5Unified@^0.1.16
    m5stack/M5GFX@^0.1.16

# 🔑 CoreS3で動作させるために必須の設定
board_build.arduino.memory_type = qio_qspi
board_build.partitions = default_16MB.csv
board_build.f_flash = 80000000L
board_build.flash_mode = qio
board_upload.flash_size = 16MB
```

### 重要なポイント

- `memory_type = qio_qspi` ← **これがないとCoreS3では動作しない**
- `partitions = default_16MB.csv` ← 16MBフラッシュをフル活用
- `f_flash = 80000000L` ← フラッシュ周波数80MHz

## 📸 カメラ設定

### GPIO ピン定義（M5Stack CoreS3専用）

```cpp
// 正しいピン定義（重要！）
.pin_xclk     = 2
.pin_sscb_sda = 12
.pin_sscb_scl = 11

.pin_d7 = 47  // Y9
.pin_d6 = 48  // Y8
.pin_d5 = 16  // Y7
.pin_d4 = 15  // Y6
.pin_d3 = 42  // Y5
.pin_d2 = 41  // Y4
.pin_d1 = 40  // Y3
.pin_d0 = 39  // Y2

.pin_vsync = 46
.pin_href  = 38
.pin_pclk  = 45
```

### カメラパラメータ

- **解像度**: QVGA (320×240)
- **ピクセルフォーマット**: RGB565（ディスプレイ直接表示用）
- **フレームバッファ**: 2枚（ダブルバッファリング）
- **バッファ配置**: PSRAM
- **取得モード**: `CAMERA_GRAB_WHEN_EMPTY`

## 💻 コードの特徴

### I2Cバス解放（重要！）

カメラとI2Cバスが競合するため、カメラ初期化前に解放が必須：

```cpp
esp_err_t camera_init(){
    M5.In_I2C.release();  // ← 必須！
    esp_err_t err = esp_camera_init(&camera_config);
    // ...
}
```

### 最適化された画像表示

```cpp
esp_err_t camera_capture(){
    camera_fb_t *fb = esp_camera_fb_get();
    
    // 高速描画
    M5.Display.startWrite();
    M5.Display.setAddrWindow(0, 0, 320, 240);
    M5.Display.writePixels((uint16_t*)fb->buf, fb->len / 2);
    M5.Display.endWrite();
    
    esp_camera_fb_return(fb);
    return ESP_OK;
}
```

## 📊 パフォーマンス

- **フレームレート**: 約15-20 FPS
- **解像度**: 320×240ピクセル
- **色深度**: RGB565 (16bit)
- **レイテンシ**: 約50-70ms

## 🐛 トラブルシューティング

### Camera Init Failed

**原因**: PSRAM設定またはI2Cバス競合

**解決方法**:
1. `platformio.ini`で`memory_type = qio_qspi`を確認
2. `M5.In_I2C.release()`が呼ばれているか確認
3. `DBOARD_HAS_PSRAM`フラグが設定されているか確認

### Camera Capture Failed

**原因**: PSRAMが正しく初期化されていない

**解決方法**:
1. シリアルモニタで「PSRAM found」が表示されるか確認
2. `board_build.partitions = default_16MB.csv`を確認
3. ビルドキャッシュをクリア: `.pio`フォルダを削除

### 書き込みエラー: Wrong boot mode detected

**原因**: ESP32-S3がダウンロードモードに入っていない

**解決方法**:
1. M5Stack CoreS3の電源OFF
2. Resetボタンを押したまま電源ON
3. Uploadをクリック
4. 「Connecting...」表示後、Resetボタンを離す

### Windowsパス長制限エラー

**原因**: ファイルパスが260文字を超えている

**解決方法**:
```powershell
# 管理者権限でPowerShellを開いて実行
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" `
                 -Name "LongPathsEnabled" `
                 -Value 1 `
                 -PropertyType DWORD `
                 -Force
# PC再起動
```

## 🎯 使い方

1. M5Stack CoreS3をUSBで接続
2. 書き込みモードに入れる（Resetボタン）
3. PlatformIO: Uploadを実行
4. 起動後、リアルタイムでカメラ画像が表示される

### 期待される動作

1. 起動時「HelloWorld」表示
2. カメラ初期化
3. リアルタイムカメラ画像が表示される
4. 画像がスムーズに更新される（15-20 FPS）

## 🔄 カスタマイズ

### 解像度の変更

`camera_config.frame_size`を変更：
```cpp
.frame_size = FRAMESIZE_QVGA,    // 320×240（推奨）
.frame_size = FRAMESIZE_QQVGA,   // 160×120（高速）
.frame_size = FRAMESIZE_VGA,     // 640×480（低速）
```

### ピクセルフォーマットの変更

```cpp
.pixel_format = PIXFORMAT_RGB565,     // ディスプレイ表示用（推奨）
.pixel_format = PIXFORMAT_GRAYSCALE,  // グレースケール
.pixel_format = PIXFORMAT_JPEG,       // JPEG圧縮
```

## 📚 参考資料

- [M5Stack CoreS3 公式ドキュメント](https://docs.m5stack.com/en/core/CoreS3)
- [M5Unified ライブラリ](https://github.com/m5stack/M5Unified)
- [ESP32 Camera ドライバ](https://github.com/espressif/esp32-camera)
- [ESP32-S3 データシート](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

## 🏆 動作確認環境

- **デバイス**: M5Stack CoreS3
- **PlatformIO**: Core 6.1.x
- **Platform**: espressif32@6.4.0
- **Framework**: Arduino
- **M5Unified**: 0.1.16
- **OS**: Windows 11

## 📝 次のステップ

このカメラテストが成功したら、次は物体検出モデルの統合に進めます：
1. ESP-DLモデルの読み込み
2. カメラ画像の前処理（リサイズ）
3. 推論実行
4. 検出結果の描画

## 🎓 学んだこと

### Arduino IDE vs PlatformIO

| 設定 | Arduino IDE | PlatformIO |
|------|-------------|------------|
| ボード定義 | 自動 | 明示的に指定が必要 |
| Memory Type | 自動 | `qio_qspi`を指定 |
| PSRAM | 自動有効 | ビルドフラグで明示 |
| I2C解放 | 不要な場合あり | 明示的に必要 |

### 重要な教訓

1. **M5Stack CoreS3では`memory_type = qio_qspi`が必須**
2. **I2Cバス解放を忘れるとカメラが初期化できない**
3. **書き込みには手動でダウンロードモード**
4. **正しいGPIOピン定義が重要**（古い情報に注意）

## 📄 ライセンス

MIT License
