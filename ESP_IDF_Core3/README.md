# UHD Object Detection - M5Stack CoreS3 (ESP-IDF)

ESP-DLを使用したM5Stack CoreS3上でのリアルタイム物体検出システム

## 📋 プロジェクト概要

Ultra High-speed and High-quality Object Detection（UHD）モデルをESP32-S3マイコン上で動作させ、カメラからの映像に対してリアルタイムで物体検出を行うシステムです。

### ハードウェア
- **デバイス**: M5Stack CoreS3 (ESP32-S3)
- **カメラ**: GC0308 (QVGA 320×240)
- **ディスプレイ**: ILI9341 LCD (320×240)
- **ストレージ**: microSDカード
- **メモリ**: 
  - SRAM: 512KB
  - PSRAM: 8MB (Octal SPI)
  - Flash: 16MB

### ソフトウェア
- **フレームワーク**: ESP-IDF v5.5.1
- **UI**: LVGL v9
- **AI**: ESP-DL (Deep Learning Library)
- **画像処理**: esp_jpeg

---

## ✅ 達成済みフェーズ

### Phase 1: 基本セットアップ ✓
- [x] ESP-IDF v5.5.1 環境構築
- [x] M5Stack CoreS3 BSP統合
- [x] LVGL v9 統合
- [x] ビルドシステム構築

### Phase 2: SDカード＆JPEG表示 ✓
- [x] SDカードマウント機能
- [x] JPEGファイル読み込み
- [x] `esp_jpeg`によるデコード（RGB565形式）
- [x] LVGL Canvasへの描画・表示

### Phase 3: バウンディングボックス描画 ✓
- [x] ダミー検出結果データ構造
- [x] 矩形オブジェクトによるBB描画
- [x] ラベルとスコア表示
- [x] カラー分け（緑・赤・シアン）

---

## 🎯 実装済み機能

### 1. JPEG画像表示
```
SDカード → JPEGファイル読み込み → デコード → LCD表示
```

**主要コンポーネント**:
- `esp_jpeg_decode()`: JPEG画像をRGB565形式にデコード
- `lv_canvas`: LVGLのCanvasウィジェットで画像表示
- PSRAMを活用したメモリ管理

**実行ログ**:
```
I JPEG_BB_VIEWER: JPEG file size: 27511 bytes
I JPEG_BB_VIEWER: Decode OK: 320x240, 153600 bytes
```

### 2. バウンディングボックス描画
```
検出結果 → BB描画 → ラベル表示 → 画像オーバーレイ
```

**ダミー検出データ**:
```cpp
Detection dummy_detections[] = {
    {50, 50, 100, 100, 0, 0.95f, "person"},   // 緑色
    {180, 80, 80, 120, 2, 0.87f, "car"},      // 赤色
    {100, 150, 60, 60, 1, 0.72f, "bicycle"}   // シアン色
};
```

**実行ログ**:
```
I JPEG_BB_VIEWER: Drawing 3 bounding boxes...
I JPEG_BB_VIEWER: BB #0: person at (50,50,100,100) conf=0.95
I JPEG_BB_VIEWER: BB #1: car at (180,80,80,120) conf=0.87
I JPEG_BB_VIEWER: BB #2: bicycle at (100,150,60,60) conf=0.72
I JPEG_BB_VIEWER: Image with bounding boxes displayed!
```

**表示内容**:
- 3つのカラフルな矩形ボックス
- 各ボックスの上に「label score%」形式のラベル
- 例: `person 95%`, `car 87%`, `bicycle 72%`

---

## 🛠️ セットアップ

### 必要な環境
1. **ESP-IDF v5.5.1**
   ```bash
   git clone -b v5.5.1 --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh
   . ./export.sh
   ```

2. **M5Stack CoreS3 BSP**
   - 自動的に`managed_components`にダウンロードされます
   - `idf_component.yml`で管理

3. **必要なライブラリ**
   - LVGL v9 (自動)
   - esp_jpeg (自動)
   - esp-dl (自動)
   - esp32-camera (自動)

### ビルド手順

```bash
# 1. プロジェクトディレクトリに移動
cd ESP_IDF_Core3

# 2. ESP-IDF環境をアクティベート（必要に応じて）
. ~/esp/esp-idf/export.sh  # Linux/Mac
# または
C:\Espressif\frameworks\esp-idf-v5.5.1\export.ps1  # Windows

# 3. ビルド
idf.py build

# 4. フラッシュ
idf.py -p COM6 flash monitor  # COMポートは環境に応じて変更
```

### SDカード準備
1. microSDカードをFAT32でフォーマット
2. `image.jpg`（320×240推奨）をルートディレクトリに配置
3. M5Stack CoreS3のSDカードスロットに挿入

---

## 📊 メモリ使用状況

### ビルドサイズ
```
uhd_detection_core3.bin: 667,536 bytes (0xa2f90)
空き: 79% (2,478,192 bytes)
```

### ランタイムメモリ
- **SRAM**: ~252KB使用可能
- **PSRAM**: 8MB使用可能
  - JPEG画像バッファ: 153,600 bytes (320×240×2)
  - その他の処理用バッファ

---

## 🚀 実行方法

### 1. 基本実行
```bash
idf.py -p COM6 flash monitor
```

### 2. 期待される動作
1. LCD初期化
2. SDカードマウント
3. `image.jpg`読み込み
4. JPEG画像デコード
5. 画像表示
6. 3つのバウンディングボックス描画
7. ラベル＆スコア表示

### 3. ログ確認
```
I JPEG_BB_VIEWER: === JPEG Viewer with Bounding Boxes ===
I JPEG_BB_VIEWER: Mounting SD card...
I JPEG_BB_VIEWER: Loading: /sdcard/image.jpg
I JPEG_BB_VIEWER: JPEG file size: 27511 bytes
I JPEG_BB_VIEWER: Decode OK: 320x240, 153600 bytes
I JPEG_BB_VIEWER: Drawing 3 bounding boxes...
I JPEG_BB_VIEWER: BB #0: person at (50,50,100,100) conf=0.95
I JPEG_BB_VIEWER: BB #1: car at (180,80,80,120) conf=0.87
I JPEG_BB_VIEWER: BB #2: bicycle at (100,150,60,60) conf=0.72
I JPEG_BB_VIEWER: Image with bounding boxes displayed!
```

### 4. トラブルシューティング
- **SD mount failed**: SDカードが正しく挿入されているか確認
- **File NOT found**: `image.jpg`がルートディレクトリにあるか確認
- **JPEG decode failed**: ファイルが壊れていないか、形式が正しいか確認
- **COMポート busy**: 前のmonitorセッションを終了（Ctrl+]）

---

## 📂 プロジェクト構造

```
ESP_IDF_Core3/
├── main/
│   ├── main.cpp              # メインアプリケーション
│   ├── CMakeLists.txt        # コンポーネントビルド設定
│   └── idf_component.yml     # 依存関係管理
├── managed_components/       # 自動ダウンロードされるコンポーネント
│   ├── espressif__esp-dl/
│   ├── espressif__esp32-camera/
│   ├── espressif__esp_jpeg/
│   ├── espressif__esp_lvgl_port/
│   ├── espressif__m5stack_core_s3/
│   └── lvgl__lvgl/
├── CMakeLists.txt            # プロジェクトビルド設定
├── sdkconfig.defaults        # ESP-IDF設定
├── partitions.csv            # パーティション設定
├── README.md                 # このファイル
├── REVISED_PHASES.md         # 開発計画
├── SPECIFICATIONS.md         # 技術仕様
└── build/                    # ビルド出力（生成される）
```

---

## 🎨 主要コード構造

### Detection構造体
```cpp
struct Detection {
    int x, y, w, h;       // バウンディングボックス座標
    int class_id;         // クラスID
    float confidence;     // 信頼度（0.0-1.0）
    const char* label;    // クラスラベル
};
```

### メイン処理フロー
```cpp
1. LCD初期化
   └─> bsp_display_start()
   
2. SDカードマウント
   └─> bsp_sdcard_mount()
   
3. JPEG読み込み＆デコード
   ├─> fopen("/sdcard/image.jpg")
   ├─> esp_jpeg_decode(&jpeg_cfg, &jpeg_out)
   └─> RGB565形式 (320×240)
   
4. 画像表示
   └─> lv_canvas_set_buffer()
   
5. バウンディングボックス描画
   ├─> lv_obj_create() (矩形)
   └─> lv_label_create() (ラベル)
```

---

## 📈 次のフェーズ

### Phase 4: AI推論実装（進行中）
- [x] ESP-DLモデルロード ✓
- [x] 前処理パイプライン（64×64 float32） ✓
- [ ] **推論実行API実装** ← 現在ここ
- [ ] 後処理（デコード＋NMS）
- [ ] 実際の検出結果でBB描画

### Phase 5: カメラ統合（次）
- [ ] GC0308カメラ初期化
- [ ] リアルタイムフレーム取得
- [ ] カメラ画像のLCD表示
- [ ] FPS測定機能

### Phase 6: リアルタイム検出（最終）
- [ ] カメラ→推論ループ統合
- [ ] パフォーマンス最適化
- [ ] メモリ最適化
- [ ] FPS向上（目標: 10-15 FPS）

---

## 🔧 技術スタック

| カテゴリ | 技術 | バージョン |
|---------|------|-----------|
| フレームワーク | ESP-IDF | v5.5.1 |
| UI | LVGL | v9 |
| 画像処理 | esp_jpeg | latest |
| AI | ESP-DL | latest |
| カメラ | esp32-camera | latest |
| BSP | m5stack_core_s3 | latest |
| ビルドシステム | CMake | 3.x |

---

## 📝 設定ファイル

### sdkconfig.defaults
```ini
# ESP32-S3 Configuration
CONFIG_IDF_TARGET="esp32s3"

# PSRAM Configuration
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y

# Flash Configuration
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHMODE_DIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y

# Partition Configuration
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# FreeRTOS
CONFIG_FREERTOS_HZ=1000

# Component Configuration
CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y
CONFIG_LWIP_LOCAL_HOSTNAME="m5cores3"
```

### partitions.csv
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  24K,
phy_init, data, phy,     0xf000,  4K,
factory,  app,  factory, 0x10000, 3M,
storage,  data, fat,     0x310000,12M,
```

---

## 📸 スクリーンショット説明

### 表示内容
1. **背景**: SDカードから読み込んだJPEG画像（320×240）
2. **バウンディングボックス**:
   - 緑色の矩形: `person 95%`（左上）
   - 赤色の矩形: `car 87%`（右上）
   - シアン色の矩形: `bicycle 72%`（中央下）
3. **ラベル**: 各ボックスの上に黒背景＋カラフルテキスト

---

## 🐛 既知の問題と制限

### 現在の制限
1. **静止画像のみ**: カメラからのリアルタイム映像はまだ未実装
2. **ダミーデータ**: バウンディングボックスは固定座標のダミー
3. **AI推論なし**: ESP-DLによる実際の物体検出はまだ未実装

### 今後の改善点
- カメラ統合
- リアルタイム推論
- パフォーマンス最適化
- より多くのクラスのサポート
- NMS（Non-Maximum Suppression）実装

---

## 📚 参考資料

### 公式ドキュメント
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/)
- [ESP-DL Documentation](https://github.com/espressif/esp-dl)
- [LVGL Documentation](https://docs.lvgl.io/9.0/)
- [M5Stack CoreS3 Docs](https://docs.m5stack.com/en/core/CoreS3)

### 関連リポジトリ
- [ESP-IDF](https://github.com/espressif/esp-idf)
- [ESP-DL](https://github.com/espressif/esp-dl)
- [LVGL](https://github.com/lvgl/lvgl)
- [M5Stack CoreS3 BSP](https://github.com/espressif/esp-bsp/tree/master/components/m5stack_core_s3)

---

## 👥 開発情報

### 開発環境
- **OS**: Windows 11
- **IDE**: Visual Studio Code
- **ツールチェーン**: ESP-IDF v5.5.1
- **デバッグ**: ESP-IDF Monitor

### ビルド情報
```
コンパイル日時: Dec 9 2025 11:57:01
バイナリサイズ: 667,536 bytes
ESP-IDF: v5.5.1
```

---

## 📄 ライセンス

このプロジェクトは開発中です。ライセンスは未定です。

---

## 🎉 まとめ

### Phase 1-4完了（2025/12/09）

現在までに以下を達成しました：
- ✅ M5Stack CoreS3の完全なセットアップ
- ✅ SDカードからのJPEG画像読み込み＆表示
- ✅ ESP-DLモデルロード成功（model.espdl 1.68MB）
- ✅ モデル定数読み込み（anchors/wh_scale）
- ✅ 前処理パイプライン完成（JPEG→RGB565→RGB888→64×64→正規化float32）
- ✅ バウンディングボックス描画システム
- ✅ LVGL v9を使用した洗練されたUI

### 実行ログ（成功）
```
I UHD_DETECTION: === UHD Object Detection with AI ===
I UHD_DETECTION: Mounting SD card...
I UHD_DETECTION: Model constants loaded
I UHD_DETECTION: Model loaded successfully      (3.3秒)
I UHD_DETECTION: JPEG decoded: 320x240
I UHD_DETECTION: Image preprocessed to 64x64
I UHD_DETECTION: Inference execution - PLACEHOLDER
I UHD_DETECTION: Found 2 detections (dummy)
I UHD_DETECTION: Detection complete!
```

### バックアップファイル
現在の動作確認済みコード:
- **`main/main_backup_model_load_working.cpp`** - モデルロード＋前処理パイプライン動作版

### 次のステップ
1. **推論API実装** - ESP-DL Tensor APIの正しい使用方法を調査・実装
2. **後処理実装** - 推論結果のデコード、NMS
3. **カメラ統合** - GC0308からのリアルタイムフレーム取得
4. **リアルタイム検出** - カメラ→推論→BB描画のループ
