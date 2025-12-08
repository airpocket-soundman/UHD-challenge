# ESP-IDF UHD Detection for M5Stack CoreS3

M5Stack CoreS3上でリアルタイム物体検出を行うESP-IDFプロジェクト。ESP-DLフレームワークを使用してUltratinyODモデルを実行します。

## 📋 目次

- [プロジェクト概要](#プロジェクト概要)
- [開発環境](#開発環境)
- [ハードウェア仕様](#ハードウェア仕様)
- [セットアップ手順](#セットアップ手順)
- [プロジェクト構成](#プロジェクト構成)
- [現在の実装状況](#現在の実装状況)
- [次のステップ](#次のステップ)

---

## プロジェクト概要

### 目標
M5Stack CoreS3でリアルタイム物体検出を実現する組み込みシステム

### 主要機能
- ✅ ESP32-S3マイコンで動作
- 🔄 カメラ画像取得（320×240）
- 🔄 ESP-DLによる推論実行
- 🔄 LCD画面への結果表示
- 🔄 microSDカードからモデル読み込み

### 使用技術
- **フレームワーク**: ESP-IDF v5.5.1
- **AI推論**: ESP-DL（Espressif Deep Learning）
- **カメラ**: ESP32-Camera（OV2640）
- **ディスプレイ**: M5Stack CoreS3内蔵LCD（ST7789, 320×240）
- **ストレージ**: microSD（SPIFSモード）

---

## 開発環境

### 必須環境
- **OS**: Windows 11（PowerShell）
- **ESP-IDF**: v5.5.1
- **ハードウェア**: M5Stack CoreS3
- **ツール**: 
  - Git
  - Espressif IDF Tools
  - USB-Cケーブル

### ESP-IDFセットアップ

```powershell
# 1. ESP-IDFのインストール（すでにインストール済みの場合はスキップ）
# Espressif IDFインストーラーを使用: https://dl.espressif.com/dl/esp-idf/

# 2. 環境変数を設定
. C:\Espressif\frameworks\esp-idf-v5.5.1\export.ps1

# 3. プロジェクトディレクトリに移動
cd c:\Users\yamas\Documents\GitHub\UHD-challenge\ESP_IDF_Core3

# 4. ターゲットを設定（初回のみ）
idf.py set-target esp32s3

# 5. ビルド
idf.py build

# 6. フラッシュと実行
idf.py -p COM6 flash monitor
```

### 依存コンポーネント（自動管理）

プロジェクトは以下のコンポーネントを使用します（`main/idf_component.yml`で管理）：

```yaml
dependencies:
  espressif/esp-dl: "^3.2.1"          # AI推論フレームワーク
  espressif/esp32-camera: "^2.1.4"   # カメラドライバ
  espressif/esp-dsp: "^1.7.0"        # デジタル信号処理
  espressif/dl_fft: "^0.3.1"         # FFT演算
  espressif/esp_jpeg: "^1.3.1"       # JPEG処理
  espressif/esp_new_jpeg: "^0.6.1"   # 新型JPEG処理
```

初回ビルド時に自動的にダウンロードされ、`managed_components/`に配置されます。

---

## ハードウェア仕様

### M5Stack CoreS3 スペック
- **CPU**: ESP32-S3 Dual-core Xtensa LX7 @ 240MHz
- **RAM**: 512KB SRAM（内蔵）
- **Flash**: 16MB
- **PSRAM**: 8MB（現在は無効化）
- **カメラ**: OV2640（最大2MP）
- **LCD**: 2.0インチ IPS 320×240 TFT（ST7789）
- **microSD**: SPI接続

### ピン配置

#### カメラ（OV2640）- DVPインターフェース
```
XCLK (クロック)     : GPIO 2
SIOD (I2C Data)     : GPIO 12
SIOC (I2C Clock)    : GPIO 11
D7-D0 (データバス)  : GPIO 47, 48, 16, 15, 42, 41, 40, 39
VSYNC (垂直同期)    : GPIO 46
HREF (水平同期)     : GPIO 38
PCLK (ピクセルクロック): GPIO 45
```

#### microSDカード - SPI接続
```
SCK  (クロック)     : GPIO 36
MISO (Master In)    : GPIO 35
MOSI (Master Out)   : GPIO 37
CS   (チップセレクト): GPIO 4
```

#### LCD（ST7789）- 内蔵・専用接続
M5Stack CoreS3の内蔵LCDは専用バスで接続されています。

---

## セットアップ手順

### 1. プロジェクトのクローン

```powershell
cd c:\Users\yamas\Documents\GitHub\UHD-challenge
# プロジェクトは既にクローン済み
```

### 2. ESP-IDF環境の準備

```powershell
# ESP-IDF環境をアクティブ化
. C:\Espressif\frameworks\esp-idf-v5.5.1\export.ps1
```

### 3. プロジェクトのビルド

```powershell
cd ESP_IDF_Core3

# ターゲット設定（初回のみ）
idf.py set-target esp32s3

# ビルド
idf.py build
```

### 4. デバイスへのフラッシュ

```powershell
# COMポートを確認してフラッシュ
idf.py -p COM6 flash monitor
```

### 5. モデルファイルの準備（次フェーズ）

microSDカードのルートディレクトリに以下を配置：
```
/
├── model.espdl     # ESP-DL形式のモデルファイル（約1.68MB）
└── model.bin       # 定数データ（anchors, wh_scale: 128 bytes）
```

モデル変換手順は`../model_conversion/README.md`を参照してください。

---

## プロジェクト構成

```
ESP_IDF_Core3/
├── CMakeLists.txt              # プロジェクトのCMake設定
├── partitions.csv              # パーティション定義（Flash/Storage）
├── sdkconfig.defaults          # デフォルト設定（PSRAM無効化など）
├── README.md                   # このファイル
│
├── main/
│   ├── CMakeLists.txt          # メインコンポーネントのビルド設定
│   ├── idf_component.yml       # 依存ライブラリ定義
│   └── main.cpp                # メインプログラム
│
├── managed_components/          # 自動ダウンロードされる依存ライブラリ
│   ├── espressif__esp-dl/      # ESP-DL AI推論ライブラリ
│   ├── espressif__esp32-camera/# カメラドライバ
│   ├── espressif__esp-dsp/     # DSPライブラリ
│   ├── espressif__dl_fft/      # FFT演算
│   ├── espressif__esp_jpeg/    # JPEG処理
│   └── espressif__esp_new_jpeg/# 新型JPEG処理
│
└── build/                       # ビルド成果物（自動生成）
    ├── uhd_detection_core3.bin # ファームウェアバイナリ
    ├── bootloader/             # ブートローダー
    └── partition_table/        # パーティションテーブル
```

### 主要ファイルの説明

#### `CMakeLists.txt`（ルート）
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(uhd_detection_core3)
```
- プロジェクト名とビルド設定の定義

#### `sdkconfig.defaults`
```
CONFIG_SPIRAM=n                    # PSRAM無効化（重要）
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y  # 16MBフラッシュ設定
CONFIG_PARTITION_TABLE_CUSTOM=y    # カスタムパーティション使用
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

#### `partitions.csv`
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  24K,
phy_init, data, phy,     0xf000,  4K,
factory,  app,  factory, 0x10000, 3M,
storage,  data, fat,     0x310000,12M,
```
- **factory**: アプリケーション（3MB）
- **storage**: microSD/データストレージ（12MB）

#### `main/idf_component.yml`
依存ライブラリの自動管理設定

#### `main/main.cpp`
現在はシンプルなテストコード（Hello World + カウンター）

---

## 現在の実装状況

### ✅ 完了した項目

#### 1. プロジェクト基盤の構築
- [x] ESP-IDFプロジェクト構造の作成
- [x] CMakeビルドシステムの設定
- [x] パーティションテーブルの定義
- [x] 依存ライブラリの統合（ESP-DL, ESP32-Camera, ESP-DSP等）

#### 2. ハードウェア対応
- [x] M5Stack CoreS3のピン設定
- [x] PSRAM無効化による安定動作確認
- [x] シリアルモニター動作確認

#### 3. ビルド・実行環境
- [x] ESP-IDF v5.5.1でのビルド成功
- [x] フラッシュ書き込み成功
- [x] デバイス起動・動作確認
- [x] ログ出力確認

### 現在の動作

```cpp
// main/main.cpp - テストコード
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Hello from M5Stack CoreS3!");
    ESP_LOGI(TAG, "ESP-IDF Simple Test");
    
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "Running... count=%d", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

**実行結果**:
```
I (255) MAIN: Hello from M5Stack CoreS3!
I (259) MAIN: ESP-IDF Simple Test
I (262) MAIN: Running... count=0
I (1265) MAIN: Running... count=1
...
```

✅ **ESP32-S3が正常に起動し、安定して動作しています**

---

## 次のステップ

### Phase 2: カメラ統合（次の実装）

#### タスク
1. **カメラ初期化**
   ```cpp
   - OV2640ドライバの初期化
   - 解像度設定（320×240）
   - フレームバッファ確保
   ```

2. **画像取得**
   ```cpp
   - カメラからフレーム取得
   - RGB565/RGB888フォーマット対応
   - フレームレート調整
   ```

3. **前処理**
   ```cpp
   - 320×240 → 240×240にクロップ
   - 240×240 → 64×64にリサイズ
   - RGB888 → RGB形式正規化（0.0-1.0）
   ```

### Phase 3: AI推論統合

#### タスク
1. **モデルロード**
   ```cpp
   - microSDカードマウント
   - model.espdlの読み込み
   - model.binから定数（anchors/wh_scale）読み込み
   ```

2. **推論実行**
   ```cpp
   - ESP-DL Modelクラスのインスタンス化
   - 入力テンソル準備
   - forward()による推論
   - 出力テンソル取得 [1, 56, 8, 8]
   ```

3. **後処理（デコード）**
   ```cpp
   - predデータのデコード
   - バウンディングボックス計算
   - 信頼度フィルタリング
   - NMS（Non-Maximum Suppression）
   ```

### Phase 4: LCD表示

#### タスク
1. **LCD初期化**
   ```cpp
   - ST7789ドライバ設定
   - 320×240解像度設定
   - フレームバッファ確保
   ```

2. **描画処理**
   ```cpp
   - カメラ画像の表示
   - バウンディングボックスの描画
   - クラスラベルと信頼度の表示
   - FPS表示
   ```

### Phase 5: 最適化

#### タスク
- パフォーマンスチューニング
- メモリ使用量最適化
- フレームレート向上
- PSRAMの有効活用検討

---

## 技術的な詳細

### メモリ構成

#### 現在（PSRAM無効）
```
SRAM (512KB):
  - コード領域
  - スタック
  - ヒープ（動的メモリ）
  - フレームバッファ

Flash (16MB):
  - アプリケーション（3MB）
  - ストレージ（12MB）
    - モデルファイル（1.68MB）
    - その他データ
```

#### 将来（PSRAM有効化時）
```
PSRAM (8MB):
  - 大きなフレームバッファ
  - モデルデータキャッシュ
  - 中間演算結果
```

### パフォーマンス目標

| 項目 | 目標値 | 備考 |
|------|--------|------|
| 推論速度 | 10-20 FPS | ESP32-S3 @ 240MHz |
| レイテンシ | < 100ms | カメラ → 推論 → 表示 |
| メモリ使用量 | < 400KB | SRAM内で動作 |
| モデルサイズ | 1.68MB | microSDから読み込み |

### システムアーキテクチャ

```
┌──────────────────┐
│   M5Stack        │
│   CoreS3         │
│                  │
│  ┌────────────┐  │
│  │  Camera    │  │  320×240
│  │  OV2640    │──┼─────────┐
│  └────────────┘  │          │
│                  │          ▼
│  ┌────────────┐  │     ┌─────────┐
│  │  ESP32-S3  │  │     │ Resize  │
│  │            │  │     │  64×64  │
│  │  - CPU     │◄─┼─────┴─────────┘
│  │  - SRAM    │  │          │
│  │  - Flash   │  │          ▼
│  │            │  │     ┌─────────┐
│  └────────────┘  │     │ ESP-DL  │
│         │        │     │Inference│
│         │        │     └─────────┘
│         ▼        │          │
│  ┌────────────┐  │          ▼
│  │   LCD      │  │     ┌─────────┐
│  │  320×240   │◄─┼─────│  Draw   │
│  └────────────┘  │     │  Box    │
│                  │     └─────────┘
│  ┌────────────┐  │
│  │  microSD   │◄─┼──── model.espdl
│  └────────────┘  │      model.bin
└──────────────────┘
```

---

## トラブルシューティング

### PSRAMエラーが発生する

**症状**: `E (160) octal_psram: PSRAM chip is not connected`

**解決策**:
```powershell
# sdkconfigを削除して再ビルド
cd ESP_IDF_Core3
del sdkconfig
idf.py reconfigure build
```

`sdkconfig.defaults`に`CONFIG_SPIRAM=n`が設定されていることを確認してください。

### ビルドエラー

**症状**: 依存ライブラリが見つからない

**解決策**:
```powershell
# 依存関係を再取得
rm -rf managed_components
idf.py reconfigure
```

### フラッシュできない

**症状**: `Serial port COM6 not found`

**解決策**:
1. デバイスマネージャーで正しいCOMポートを確認
2. USBケーブルを再接続
3. ドライバーをインストール（CH340/CP2102）

---

## 参考資料

### ESP-IDF関連
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)

### ESP-DL関連
- [ESP-DL GitHub](https://github.com/espressif/esp-dl)
- [ESP-DL Documentation](https://github.com/espressif/esp-dl/tree/master/docs)

### M5Stack CoreS3
- [M5Stack CoreS3 Docs](https://docs.m5stack.com/en/core/CoreS3)
- [M5Stack CoreS3 Schematic](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/core/CoreS3/coreS3_sch.pdf)

### モデル変換
- [../model_conversion/README.md](../model_conversion/README.md) - ONNXからESP-DL形式への変換手順

---

## ライセンス

See main project LICENSE

---

## 変更履歴

### 2025-12-09 - Phase 1完了
- ✅ ESP-IDFプロジェクト構造作成
- ✅ 依存ライブラリ統合（ESP-DL, ESP32-Camera等）
- ✅ PSRAM無効化による安定動作確認
- ✅ ビルド・フラッシュ・実行成功
- ✅ M5Stack CoreS3で正常起動確認

**現在の状態**: ESP32-S3が正常に動作し、次のフェーズ（カメラ統合）の準備が整いました。

---

## お問い合わせ

プロジェクトに関する質問や問題は、GitHubのIssueで報告してください。
