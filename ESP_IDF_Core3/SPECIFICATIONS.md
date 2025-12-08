# UHD Detection for M5Stack CoreS3 - 技術仕様書

## 📋 プロジェクト仕様

### プロジェクト名
**UHD Detection for M5Stack CoreS3**

### バージョン
**Phase 1: v0.1.0** (基盤構築完了)

### 最終更新日
2025年12月9日

---

## 🎯 プロジェクト目標

M5Stack CoreS3上でリアルタイム物体検出（UHD: Ultra High-speed Detection）を実現する組み込みシステムの構築。

### 主要目標
1. **リアルタイム性**: 10-20 FPSでの物体検出
2. **低消費電力**: ESP32-S3の効率的な利用
3. **エッジAI**: クラウドに依存しない完全なエッジ推論
4. **可視化**: LCD画面でのリアルタイム検出結果表示

---

## 🔧 システム仕様

### ハードウェア仕様

#### メインボード: M5Stack CoreS3

| コンポーネント | 仕様 | 備考 |
|--------------|------|------|
| **MCU** | ESP32-S3 | Dual-core Xtensa LX7 @ 240MHz |
| **内蔵SRAM** | 512KB | プログラムとデータ用 |
| **内蔵Flash** | 16MB | アプリケーションとストレージ |
| **外付けPSRAM** | 8MB QSPI | Phase 2で有効化予定 |
| **カメラ** | OV2640 | 最大2MP、UXGA (1600×1200) |
| **ディスプレイ** | 2.0" IPS LCD | 320×240 TFT (ST7789) |
| **ストレージ** | microSD | SPI接続、モデルファイル保存用 |
| **USB** | USB-C | プログラミング・給電・デバッグ |
| **バッテリー** | 内蔵 | Li-Po 500mAh |

#### ピンアサイン

**カメラ（OV2640）- DVPインターフェース**
```
XCLK:  GPIO 2   (カメラクロック信号)
SIOD:  GPIO 12  (I2C Data - カメラ設定用)
SIOC:  GPIO 11  (I2C Clock - カメラ設定用)
D7:    GPIO 47  (画像データビット7 MSB)
D6:    GPIO 48  (画像データビット6)
D5:    GPIO 16  (画像データビット5)
D4:    GPIO 15  (画像データビット4)
D3:    GPIO 42  (画像データビット3)
D2:    GPIO 41  (画像データビット2)
D1:    GPIO 40  (画像データビット1)
D0:    GPIO 39  (画像データビット0 LSB)
VSYNC: GPIO 46  (垂直同期信号)
HREF:  GPIO 38  (水平同期信号)
PCLK:  GPIO 45  (ピクセルクロック)
```

**microSDカード - SPIインターフェース**
```
SCK:   GPIO 36  (SPI Clock)
MISO:  GPIO 35  (Master In Slave Out)
MOSI:  GPIO 37  (Master Out Slave In)
CS:    GPIO 4   (Chip Select)
```

**LCD（ST7789）**
- M5Stack CoreS3内蔵、専用バスで接続
- ハードウェアで管理、ドライバーで制御

### ソフトウェア仕様

#### 開発環境

| 項目 | 仕様 | バージョン |
|------|------|-----------|
| **OS** | Windows 11 | 64-bit |
| **IDE** | Visual Studio Code | 推奨 |
| **ビルドシステム** | CMake | 3.16+ |
| **フレームワーク** | ESP-IDF | v5.5.1 |
| **プログラミング言語** | C++ | C++17 |
| **ツールチェーン** | xtensa-esp32s3-elf-gcc | ESP-IDF同梱 |

#### 依存ライブラリ

| ライブラリ | バージョン | 用途 |
|-----------|-----------|------|
| **espressif/esp-dl** | ^3.2.1 | AI推論エンジン |
| **espressif/esp32-camera** | ^2.1.4 | カメラ制御ドライバ |
| **espressif/esp-dsp** | ^1.7.0 | デジタル信号処理 |
| **espressif/dl_fft** | ^0.3.1 | 高速フーリエ変換 |
| **espressif/esp_jpeg** | ^1.3.1 | JPEG エンコード/デコード |
| **espressif/esp_new_jpeg** | ^0.6.1 | 最適化されたJPEG処理 |

これらは`main/idf_component.yml`で管理され、初回ビルド時に自動ダウンロードされます。

---

## 🧠 AI推論仕様

### モデル仕様

#### 使用モデル
**UltratinyOD (Ultra Tiny Object Detection)**
- 軽量物体検出モデル
- ReLU活性化関数使用
- COCO 80クラス対応

#### モデルバリアント
```
ultratinyod_res_anc8_w64_64x64_quality_relu_nopost.onnx
├─ res: Residual接続あり
├─ anc8: 8種類のアンカーボックス
├─ w64: 幅64（最軽量バリアント = N variant）
├─ 64x64: 入力解像度
├─ quality: 品質重視版
├─ relu: ReLU活性化
└─ nopost: 後処理（ArgMax等）なし
```

#### モデルファイル形式

| ファイル | 形式 | サイズ | 説明 |
|---------|------|--------|------|
| **model.espdl** | ESP-DL Binary | ~1.68 MB | INT8量子化済みモデル |
| **model.bin** | Raw Binary | 128 bytes | 定数データ（anchors, wh_scale） |

### 入力仕様

| パラメータ | 値 | 説明 |
|-----------|-----|------|
| **入力形状** | [1, 3, 64, 64] | Batch×Channel×Height×Width |
| **色空間** | RGB | Red, Green, Blue |
| **データ型** | float32 | 浮動小数点32ビット |
| **値範囲** | [0.0, 1.0] | 正規化済み（0〜255を0〜1にスケーリング） |
| **チャンネル順** | CHW | Channel-first形式 |

### 出力仕様

#### メイン出力: pred
```
形状: [1, 56, 8, 8]
  ├─ 1:  バッチサイズ
  ├─ 56: 予測チャンネル数
  │      ├─ 1: 物体信頼度（objectness）
  │      ├─ 4: バウンディングボックス座標（cx, cy, w, h）
  │      └─ 80: クラス確率（COCO 80クラス）
  │           実際は: 1 + 4 + 51 = 56チャンネル
  │           （51クラスのみ使用、残り29クラスは省略）
  ├─ 8:  グリッド高さ
  └─ 8:  グリッド幅
```

#### 定数データ（model.bin）

**anchors [8, 2]** - アンカーボックスサイズ
```python
[
  [2.2193656e-06, 4.8251577e-06],  # アンカー0: 極小物体用
  [4.4204730e-06, 8.9109344e-06],  # アンカー1: 小物体用
  [5.5083538e-06, 2.0233399e-05],  # アンカー2: 小物体用
  [1.0438751e-05, 1.4592547e-05],  # アンカー3: 中物体用
  [1.1409107e-05, 3.5738733e-05],  # アンカー4: 中物体用
  [1.9950507e-05, 5.5220753e-05],  # アンカー5: 中大物体用
  [3.5157802e-05, 7.3149429e-05],  # アンカー6: 大物体用
  [6.7565757e-05, 8.7956054e-05]   # アンカー7: 最大物体用
]
```

**wh_scale [8, 2]** - スケール係数
```python
[
  [0.98867685, 0.98867685],  # アンカー0-6: 共通スケール
  [0.98867685, 0.98867685],
  [0.98867685, 0.98867685],
  [0.98867685, 0.98867685],
  [0.98867685, 0.98867685],
  [0.98867685, 0.98867685],
  [0.98867685, 0.98867685],
  [6.733903,   7.456346]     # アンカー7: 異なるスケール
]
```

### ESP-DLサポート演算子

モデルで使用される全ての演算子はESP-DLでサポート済み：

| オペレータ | 使用回数 | ESP-DL対応 |
|-----------|---------|-----------|
| Conv | 39 | ✅ 完全対応 |
| Relu | 33 | ✅ 完全対応 |
| Reshape | 5 | ✅ 完全対応 |
| Add | 3 | ✅ 完全対応 |
| Concat | 2 | ✅ 完全対応 |
| MaxPool | 1 | ✅ 完全対応 |
| Mul | 1 | ✅ 完全対応 |
| Sigmoid | 1 | ✅ 完全対応 |
| ReduceMean | 1 | ✅ 完全対応 |

**合計**: 9種類、86ノード - すべてサポート済み ✅

---

## 📊 パフォーマンス仕様

### 目標パフォーマンス

| メトリクス | Phase 1 | Phase 2 | Phase 3 | Phase 4 |
|-----------|---------|---------|---------|---------|
| **起動時間** | - | < 2秒 | < 2秒 | < 2秒 |
| **カメラFPS** | - | 30 FPS | 30 FPS | 30 FPS |
| **推論速度** | - | - | 10-20 FPS | 15-25 FPS |
| **E2Eレイテンシ** | - | - | < 150ms | < 100ms |
| **メモリ使用量** | 50KB | 200KB | 400KB | 450KB |
| **Flash使用量** | 200KB | 1.5MB | 3MB | 3MB |
| **消費電力** | - | < 500mW | < 800mW | < 1W |

### メモリ配分計画

#### Phase 1（現在）- 基盤構築
```
SRAM (512KB):
  ├─ プログラム: ~50KB
  ├─ スタック: ~32KB
  └─ 空き: ~430KB

Flash (16MB):
  ├─ ブートローダー: ~21KB
  ├─ パーティションテーブル: 3KB
  ├─ アプリケーション: ~200KB
  └─ 空き: ~15.7MB
```

#### Phase 2 - カメラ統合後
```
SRAM (512KB):
  ├─ プログラム: ~100KB
  ├─ スタック: ~64KB
  ├─ フレームバッファ (QVGA RGB565): ~150KB
  │   (320×240×2 bytes = 153,600 bytes)
  └─ 空き: ~198KB

Flash (16MB):
  ├─ アプリケーション: ~1.5MB
  └─ 空き: ~14.5MB
```

#### Phase 3 - AI推論統合後
```
SRAM (512KB):
  ├─ プログラム: ~150KB
  ├─ スタック: ~64KB
  ├─ フレームバッファ: ~150KB
  ├─ 推論バッファ: ~100KB
  │   └─ 入力テンソル (64×64×3×4bytes): ~49KB
  │   └─ 中間バッファ: ~51KB
  └─ 空き: ~48KB

Flash (16MB):
  ├─ アプリケーション: ~3MB
  └─ Storage (microSD用): 12MB
      └─ model.espdl: 1.68MB (microSDから動的ロード)
```

#### Phase 4 - PSRAM有効化後（オプション）
```
PSRAM (8MB):
  ├─ フレームバッファ (VGA RGB888): ~900KB
  │   (640×480×3 bytes)
  ├─ モデルキャッシュ: ~2MB
  ├─ 中間演算バッファ: ~1MB
  └─ 空き: ~4.1MB

SRAM (512KB):
  ├─ クリティカルパス専用
  └─ 低レイテンシ処理
```

---

## 🔄 処理フロー仕様

### システム全体フロー

```
[起動]
  │
  ├─ ESP-IDF初期化
  ├─ ハードウェア初期化
  │   ├─ GPIO設定
  │   ├─ I2C初期化
  │   └─ SPI初期化
  │
  ├─ カメラ初期化 (Phase 2)
  │   ├─ OV2640設定
  │   ├─ 解像度設定 (QVGA 320×240)
  │   └─ フレームバッファ確保
  │
  ├─ LCD初期化 (Phase 4)
  │   ├─ ST7789設定
  │   └─ 描画バッファ確保
  │
  ├─ microSDマウント (Phase 3)
  │   ├─ SPIFS初期化
  │   └─ FAT32マウント
  │
  ├─ モデルロード (Phase 3)
  │   ├─ model.espdl読み込み
  │   ├─ model.bin読み込み
  │   └─ ESP-DL初期化
  │
  └─ メインループ
      │
      └─── [1秒ごと] ───┐
           │            │
           ├─ カメラキャプチャ (320×240)
           │    │
           │    ├─ RGB565 → RGB888変換
           │    └─ 中央クロップ (240×240)
           │
           ├─ 前処理
           │    ├─ リサイズ (240×240 → 64×64)
           │    ├─ RGB888 → RGB float32
           │    └─ 正規化 ([0, 255] → [0.0, 1.0])
           │
           ├─ AI推論 (ESP-DL)
           │    ├─ 入力テンソル準備 [1, 3, 64, 64]
           │    ├─ model->forward()
           │    └─ 出力テンソル取得 [1, 56, 8, 8]
           │
           ├─ 後処理（デコード）
           │    ├─ グリッドごとに処理 (8×8=64)
           │    ├─ アンカーごとに処理 (8種類)
           │    ├─ バウンディングボックス計算
           │    ├─ 信頼度フィルタリング (> 0.5)
           │    └─ NMS（重複除去）
           │
           ├─ 描画
           │    ├─ 元画像表示 (320×240)
           │    ├─ バウンディングボックス描画
           │    ├─ ラベル・信頼度表示
           │    └─ FPS表示
           │
           └─ ログ出力
                └─ 検出結果、パフォーマンス統計
```

### 画像処理フロー詳細

```
カメラ画像 (320×240 RGB565)
  │
  ├─ [変換] RGB565 → RGB888
  │   出力: 320×240×3 bytes = 230,400 bytes
  │
  ├─ [クロップ] 中央240×240を抽出
  │   X: (320-240)/2 = 40
  │   Y: 0
  │   出力: 240×240×3 bytes = 172,800 bytes
  │
  ├─ [リサイズ] Bilinear補間
  │   240×240 → 64×64
  │   出力: 64×64×3 bytes = 12,288 bytes
  │
  ├─ [型変換] uint8 → float32
  │   出力: 64×64×3×4 bytes = 49,152 bytes
  │
  └─ [正規化] pixel / 255.0
      出力: [0.0, 1.0] 範囲のfloat32
      形状: [1, 3, 64, 64] (CHW形式)
```

---

## 🗂️ ファイル構成仕様

### プロジェクトディレクトリ構造

```
ESP_IDF_Core3/
│
├── CMakeLists.txt                    # プロジェクトルートCMake
├── partitions.csv                    # フラッシュパーティション定義
├── sdkconfig.defaults                # デフォルト設定
├── README.md                         # プロジェクト概要
├── SPECIFICATIONS.md                 # 本ファイル（技術仕様書）
├── .gitignore                        # Git除外ファイル
│
├── main/                             # メインアプリケーション
│   ├── CMakeLists.txt                # メインコンポーネントCMake
│   ├── idf_component.yml             # コンポーネント依存関係
│   └── main.cpp                      # エントリポイント
│
├── managed_components/               # 自動管理コンポーネント
│   ├── espressif__esp-dl/            # AI推論ライブラリ
│   ├── espressif__esp32-camera/      # カメラドライバ
│   ├── espressif__esp-dsp/           # DSPライブラリ
│   ├── espressif__dl_fft/            # FFTライブラリ
│   ├── espressif__esp_jpeg/          # JPEGライブラリ
│   └── espressif__esp_new_jpeg/      # 最適化JPEGライブラリ
│
└── build/                            # ビルド成果物（自動生成）
    ├── bootloader/
    │   └── bootloader.bin            # ブートローダーバイナリ (21KB)
    ├── partition_table/
    │   └── partition-table.bin       # パーティションテーブル (3KB)
    ├── uhd_detection_core3.elf       # ELFファイル（デバッグ用）
    └── uhd_detection_core3.bin       # ファームウェアバイナリ (~200KB)
```

### microSDカード構成

```
microSD (root)/
│
├── model.espdl                       # ESP-DLモデルファイル (1.68MB)
├── model.bin                         # 定数データ (128 bytes)
│
└── [将来的な拡張]
    ├── config.json                   # 実行時設定
    ├── labels.txt                    # クラスラベル一覧
    └── logs/                         # ログファイル
        └── detection_YYYYMMDD.log    # 検出結果ログ
```

---

## 🔐 設定仕様

### sdkconfig.defaults

```ini
# ESP32-S3ターゲット設定
CONFIG_IDF_TARGET="esp32s3"

# フラッシュ設定
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y
CONFIG_ESPTOOLPY_FLASHMODE_DIO=y

# パーティション設定
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# PSRAM設定（Phase 1では無効）
CONFIG_SPIRAM=n

# 最適化設定
CONFIG_COMPILER_OPTIMIZATION_SIZE=n
CONFIG_COMPILER_OPTIMIZATION_PERF=y

# ログレベル
CONFIG_LOG_DEFAULT_LEVEL_INFO=y

# FreeRTOS設定
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y

# ESP32-S3クロック設定
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y

# メモリ設定
CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP=y
```

### partitions.csv

```csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  24K,
phy_init, data, phy,     0xf000,  4K,
factory,  app,  factory, 0x10000, 3M,
storage,  data, fat,     0x310000,12M,
```

**パーティション説明**:
- **nvs** (24KB): Non-Volatile Storage - WiFi設定等
- **phy_init** (4KB): PHY初期化データ
- **factory** (3MB): アプリケーション領域
- **storage** (12MB): データストレージ（microSD用）

---

## 🧪 テスト仕様

### ユニットテスト項目（将来実装予定）

#### カメラモジュール
- [ ] カメラ初期化成功確認
- [ ] フレーム取得確認
- [ ] 解像度設定確認
- [ ] フレームレート測定

#### 画像処理モジュール
- [ ] RGB565→RGB888変換精度
- [ ] クロップ処理確認
- [ ] リサイズ処理確認
- [ ] 正規化処理確認

#### AI推論モジュール
- [ ] モデルロード成功確認
- [ ] 推論実行時間測定
- [ ] メモリリーク確認
- [ ] 出力テンソル形状確認

#### 後処理モジュール
- [ ] デコード処理確認
- [ ] NMS動作確認
- [ ] バウンディングボックス精度

#### LCD表示モジュール
- [ ] LCD初期化確認
- [ ] 描画処理確認
- [ ] FPS表示確認

### 統合テスト項目

- [ ] E2E処理フロー確認
- [ ] 長時間動作安定性テスト（24時間）
- [ ] メモリ使用量モニタリング
- [ ] 温度上昇テスト
- [ ] バッテリー駆動時間測定

### パフォーマンステスト項目

- [ ] 推論速度ベンチマーク
- [ ] フレームレート測定
- [ ] レイテンシ測定（カメラ→表示）
- [ ] メモリピーク測定
- [ ] CPU使用率測定

---

## 📝 開発フェーズ

### Phase 1: 基盤構築 ✅ **完了**

**期間**: 2025年12月9日

**成果物**:
- [x] ESP-IDFプロジェクト構造
- [x] CMakeビルドシステム
- [x] パーティションテーブル定義
- [x] 依存ライブラリ統合
- [x] PSRAM無効化設定
- [x] ビルド・フラッシュ・実行確認

**検証結果**:
```
✅ ESP32-S3が正常に起動
✅ シリアルモニターで動作確認
✅ メモリ使用量: ~50KB SRAM
✅ Flash使用量: ~200KB
```

### Phase 2: カメラ統合 🔄 **次のフェーズ**

**目標**:
- [ ] OV2640カメラドライバ統合
- [ ] QVGA (320×240)画像取得
- [ ] RGB888形式での取得
- [ ] フレームレート30FPS達成

**主要タスク**:
1. カメラピン設定とI2C通信確立
2. カメラレジスタ設定（解像度、フォーマット）
3. DVPインターフェース設定
4. フレームバッファ管理実装
5. 画像取得APIの実装

**成功基準**:
- 安定して30FPSでフレーム取得
- メモリリークなし
- 画像品質が良好

### Phase 3: AI推論統合

**目標**:
- [ ] microSDカードマウント
- [ ] モデルファイル読み込み
- [ ] ESP-DL推論実行
- [ ] 10-20FPSでの推論達成

**主要タスク**:
1. microSD SPIドライバ統合
2. model.espdl読み込み実装
3. model.bin定数読み込み
4. ESP-DL Modelクラス統合
5. 前処理パイプライン実装
6. 推論実行ループ実装

**成功基準**:
- モデルが正常にロードされる
- 推論が安定して動作
- 10FPS以上を達成
- メモリ使用量が予算内

### Phase 4: LCD表示統合

**目標**:
- [ ] ST7789 LCD制御
- [ ] カメラ画像表示
- [ ] 検出結果の可視化
- [ ] FPS表示

**主要タスク**:
1. LCDドライバ統合
2. フレームバッファ管理
3. 描画API実装
4. バウンディングボックス描画
5. テキスト描画（ラベル、FPS）

**成功基準**:
- リアルタイムで画像が表示される
- 検出結果が正確に描画される
- UIが見やすい

### Phase 5: 最適化

**目標**:
- [ ] パフォーマンスチューニング
- [ ] メモリ使用量削減
- [ ] FPS向上（目標25FPS）
- [ ] PSRAM有効化（オプション）

**主要タスク**:
1. ボトルネック分析
2. コード最適化
3. メモリプール実装
4. PSRAM統合（必要に応じて）
5. 並列処理の導入

**成功基準**:
- 25FPS以上を達成
- メモリ使用効率が向上
- バッテリー駆動時間が延長

---

## 🚀 デプロイ仕様

### ビルド手順

```powershell
# 1. 環境準備
. C:\Espressif\frameworks\esp-idf-v5.5.1\export.ps1

# 2. プロジェクトディレクトリに移動
cd c:\Users\yamas\Documents\GitHub\UHD-challenge\ESP_IDF_Core3

# 3. ターゲット設定（初回のみ）
idf.py set-target esp32s3

# 4. ビルド
idf.py build

# 5. フラッシュ
idf.py -p COM6 flash

# 6. モニター（デバッグ）
idf.py -p COM6 monitor
```

### リリースビルド

```powershell
# 最適化ビルド
idf.py -DCMAKE_BUILD_TYPE=Release build

# サイズ最適化
idf.py -DCONFIG_COMPILER_OPTIMIZATION_SIZE=y build
```

### バイナリ配布

```
release/
├── bootloader.bin              # オフセット 0x0
├── partition-table.bin         # オフセット 0x8000
├── uhd_detection_core3.bin     # オフセット 0x10000
├── model.espdl                 # microSDに配置
├── model.bin                   # microSDに配置
└── flash_instructions.txt      # フラッシュ手順書
```

---

## 📚 API仕様（Phase 2以降で実装）

### カメラAPI

```cpp
namespace Camera {
    esp_err_t init();
    esp_err_t start();
    esp_err_t stop();
    camera_fb_t* capture();
    void return_fb(camera_fb_t* fb);
    esp_err_t set_resolution(framesize_t size);
}
```

### 画像処理API

```cpp
namespace ImageProc {
    void rgb565_to_rgb888(const uint8_t* src, uint8_t* dst, 
                          int width, int height);
    void crop_center(const uint8_t* src, uint8_t* dst, 
                     int src_w, int src_h, int dst_w, int dst_h);
    void resize_bilinear(const uint8_t* src, uint8_t* dst,
                         int src_w, int src_h, int dst_w, int dst_h);
    void normalize(const uint8_t* src, float* dst, int size);
}
```

### AI推論API

```cpp
namespace AI {
    esp_err_t load_model(const char* model_path);
    esp_err_t load_constants(const char* constants_path);
    std::vector<Detection> inference(const float* input_data);
    void unload_model();
}

struct Detection {
    float x, y, w, h;     // バウンディングボックス
    int class_id;         // クラスID
    float confidence;     // 信頼度
    const char* label;    // クラス名
};
```

### LCD表示API

```cpp
namespace Display {
    esp_err_t init();
    void clear();
    void draw_image(const uint8_t* img, int x, int y, int w, int h);
    void draw_box(int x, int y, int w, int h, uint16_t color);
    void draw_text(const char* text, int x, int y, uint16_t color);
    void update();
}
```

---

## 🔍 デバッグ・モニタリング

### ログレベル

```cpp
ESP_LOGE(TAG, "Error");      // エラー（赤）
ESP_LOGW(TAG, "Warning");    // 警告（黄）
ESP_LOGI(TAG, "Info");       // 情報（緑）
ESP_LOGD(TAG, "Debug");      // デバッグ（白）
ESP_LOGV(TAG, "Verbose");    // 詳細（グレー）
```

### パフォーマンスモニタリング

```cpp
// 推論時間測定
uint32_t start = esp_timer_get_time();
inference();
uint32_t elapsed = esp_timer_get_time() - start;
ESP_LOGI(TAG, "Inference time: %lu ms", elapsed / 1000);

// メモリ使用量
ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
ESP_LOGI(TAG, "Min free heap: %lu bytes", esp_get_minimum_free_heap_size());
```

---

## 📖 参考資料

### 公式ドキュメント
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [ESP32-S3 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [M5Stack CoreS3 Docs](https://docs.m5stack.com/en/core/CoreS3)

### ライブラリドキュメント
- [ESP-DL GitHub](https://github.com/espressif/esp-dl)
- [ESP32-Camera GitHub](https://github.com/espressif/esp32-camera)
- [ESP-DSP Documentation](https://docs.espressif.com/projects/esp-dsp/)

### UHDモデル
- [UHD GitHub](https://github.com/CheungBH/UHD)
- [モデル変換手順](../model_conversion/README.md)

---

## 📄 ライセンス

本プロジェクトのライセンスについては、メインプロジェクトのLICENSEファイルを参照してください。

---

## 👥 貢献者

- プロジェクト作成: 2025年12月9日
- Phase 1実装: 完了

---

**Document Version**: 1.0.0  
**Last Updated**: 2025年12月9日  
**Status**: Phase 1 Complete ✅
