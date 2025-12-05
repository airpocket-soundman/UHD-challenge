# M5Stack CoreS3 Object Detection Demo

リアルタイム物体検出デモ：カメラ画像取得 → ESP-DL推論 → BBox描画 → 画面表示

## 📋 機能

- ✅ カメラからリアルタイム画像取得（320x240）
- ✅ ESP-DL推論エンジン使用
- ✅ **main/ディレクトリからモデル読み込み（Flash内蔵）**
- ✅ 6出力モデル対応（ArgMax実装済み）
- ✅ バウンディングボックス描画
- ✅ クラス名・スコア表示（person専用）
- ✅ NMS（Non-Maximum Suppression）実装
- ✅ FPS表示
- ✅ **単一クラスモデル（person検出専用）**

## 🛠️ ハードウェア要件

- **M5Stack CoreS3**
  - ESP32-S3 (Xtensa 32-bit LX7 dual-core @ 240MHz)
  - 8MB PSRAM
  - 2.0" IPS LCD (320x240)
  - GC0308カメラ
  - **microSDカードスロット**

## 💾 モデルファイル

### モデルの配置
```
M5StackS3/
└── main/
    └── uhd_n_w64_multi  # 1.50 MB (Flash内蔵)
```

**注意**: モデルファイルはビルド時にFlashに書き込まれます。microSDカードは不要です。

## 🔧 ビルド手順（PlatformIO）

### 1. PlatformIOインストール
```bash
# VS Codeの場合
# Extensions → PlatformIO IDE をインストール

# または CLI版
pip install platformio
```

### 2. プロジェクト初期化
```bash
cd M5StackS3

# 依存関係ダウンロード（初回のみ）
pio pkg install
```

### 3. ビルド
```bash
# ビルド
pio run

# または VS Code
# PlatformIO: Build
```

### 4. アップロード
```bash
# M5Stack CoreS3をUSBケーブルで接続

# アップロード
pio run --target upload

# または VS Code
# PlatformIO: Upload
```

### 5. モニター
```bash
# シリアルモニター起動
pio device monitor

# または VS Code
# PlatformIO: Serial Monitor
```

## 📂 プロジェクト構造

```
M5StackS3/
├── platformio.ini          # PlatformIO設定
├── partitions.csv          # パーティションテーブル
├── src/
│   └── main.cpp            # メインアプリケーション
├── main/
│   └── uhd_n_w64_multi     # ★ ESP-DLモデル（1.50 MB）Flash内蔵
├── sdkconfig.defaults      # ESP-IDF設定（オプション）
└── README.md               # このファイル
```

## 💻 コードの主要機能

### 1. モデル読み込み（Flash内蔵）
```cpp
ObjectDetector detector;
// Flash内蔵モデル（main/ディレクトリ）から読み込み
detector.load_model("/spiffs/uhd_n_w64_multi");
```

### 2. カメラ初期化
```cpp
bool init_camera() {
    camera_config_t config;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_QVGA;  // 320x240
    // M5Stack CoreS3のカメラピン設定
    esp_camera_init(&config);
}
```

### 4. 推論処理
```cpp
std::vector<Detection> detect(uint8_t* rgb_data, int width, int height) {
    // 1. リサイズ: 320x240 → 64x64
    // 2. 正規化: [0, 255] → [0.0, 1.0]
    // 3. モデル推論
    model->run(input);
    // 4. 6出力取得
    float* detection_scores = model->get_output(0);  // [100, 1]
    float* class_scores = model->get_output(1);      // [100, 80] ★
    float* bbox_x1 = model->get_output(2);           // [100, 1]
    // ... bbox_y1, x2, y2
}
```

### 5. ArgMax実装
```cpp
// 100検出 × 80クラスに対してArgMax
for (int i = 0; i < 100; i++) {
    int max_class = 0;
    float max_score = class_scores[i * 80];
    
    for (int c = 1; c < 80; c++) {
        if (class_scores[i * 80 + c] > max_score) {
            max_score = class_scores[i * 80 + c];
            max_class = c;
        }
    }
    // max_classを使用
}
```

### 6. BBox描画
```cpp
void draw_detections(const std::vector<Detection>& dets, int width, int height) {
    for (const auto& det : dets) {
        // バウンディングボックス
        M5.Lcd.drawRect(x1, y1, width, height, TFT_GREEN);
        // ラベル + スコア
        M5.Lcd.drawString(label, x1, y1);
    }
}
```

## 🎯 パフォーマンス

### 期待値
- **推論速度**: 5-10 FPS
- **メモリ使用**: ~4MB（PSRAM使用）
- **検出精度**: COCO mAP 30-40%（64x64入力）
- **SD読込時間**: ~1秒（起動時のみ）

### 最適化オプション
```cpp
// src/main.cpp の設定
#define CONF_THRESHOLD 0.5f   // 検出信頼度閾値
#define IOU_THRESHOLD 0.45f   // NMS IoU閾値
```

## 🐛 トラブルシューティング

### Model Load Failed
```
原因: モデルファイルがmain/ディレクトリにない、またはビルド時に含まれなかった
対策:
1. ファイル確認: main/uhd_n_w64_multi が存在するか
2. ファイルサイズ: 約1.5MB
3. 再ビルド: pio run --target upload
```

### Camera Init Failed
```
原因: カメラモジュールの接続問題
対策:
1. M5Stack CoreS3のカメラケーブル確認
2. 再起動
```

### Out of Memory
```
原因: PSRAMが無効、またはモデルサイズ大きすぎ
対策:
1. platformio.ini確認
   board_build.arduino.memory_type = qio_opi
2. ビルドフラグ確認
   -DCONFIG_SPIRAM=1
3. パーティションテーブル確認
```

### 低FPS（< 3 FPS）
```
原因: モデル推論が遅い
対策:
1. CONF_THRESHOLD を上げる（検出数削減）
2. カメラ解像度を下げる
3. ESP32-S3のクロック周波数確認
```

## 📊 モデル詳細

### 入力
- サイズ: 64×64×3 (RGB)
- 範囲: [0.0, 1.0]
- フォーマット: NCHW

### 出力（6出力）
1. **detection_scores** [100, 1] - 検出スコア
2. **class_scores** [100, 1] - クラススコア（単一クラス: person）
3. **bbox_x1** [100, 1] - BBox左上X座標（正規化）
4. **bbox_y1** [100, 1] - BBox左上Y座標（正規化）
5. **bbox_x2** [100, 1] - BBox右下X座標（正規化）
6. **bbox_y2** [100, 1] - BBox右下Y座標（正規化）

### クラス
- **単一クラス**: person のみ検出

## 🔧 オペレータ情報

### オリジナルONNXモデルのオペレータ（18種類）
```
総オペレータ数: 18種類、総使用回数: 143回

- Add: 5回（加算）
- ArgMax: 1回（最大値インデックス取得）★変換時に除外
- Cast: 1回（型変換）
- Concat: 3回（結合）
- Conv: 32回（畳み込み）
- Div: 2回（除算）
- Gather: 6回（要素収集）
- GatherElements: 5回（要素収集）
- MaxPool: 1回（最大プーリング）
- Mul: 31回（乗算）
- ReduceMax: 1回（最大値削減）
- Reshape: 12回（形状変換）
- Sigmoid: 32回（活性化関数）
- Slice: 1回（スライス）
- Softplus: 2回（活性化関数）
- TopK: 1回（上位K個取得）
- Transpose: 1回（転置）
- Unsqueeze: 7回（次元追加）
```

### ESP-DL変換後モデルの対応オペレータ
```
ESP-DLでサポートされる主要オペレータ:
- Conv2D（畳み込み）
- DepthwiseConv2D（深さ方向畳み込み）
- ReLU/ReLU6（活性化関数）
- Sigmoid（活性化関数）
- Add（加算・残差接続）
- Concat（結合）
- Reshape/Flatten（形状変換）
- Mul（乗算）
- その他の基本演算

注: ArgMax、TopK等の一部オペレータは変換時に除外され、
    6出力形式（multi版）に変換されています。
```

### 変換プロセス
```
オリジナルONNX (18オペレータ)
    ↓
ArgMax/TopK除外 + 6出力化
    ↓
ESP-DL最適化
    ↓
ESP-DLバイナリ (uhd_n_w64_multi)
```

## 🔍 使用方法

### 1. ビルド & アップロード
```bash
cd M5StackS3
pio run --target upload
```

モデルファイル（main/uhd_n_w64_multi）は自動的にFlashに書き込まれます。

### 2. 実行
1. M5Stack CoreS3の電源ON
2. 画面表示:
   - "Initializing..." → 初期化中
   - "Camera..." → カメラ初期化
   - "Model..." → モデル読込（Flash内蔵）
   - "Ready!" → 検出開始

### 3. 動作確認
- カメラ画像がリアルタイム表示
- Person検出時に緑色のBBox表示
- "person 0.XX" のラベルとスコア表示
- 左上にFPS表示

## 🔗 参考リンク

- [M5Stack CoreS3 公式](https://docs.m5stack.com/en/core/CoreS3)
- [ESP-DL ドキュメント](https://github.com/espressif/esp-dl)
- [PlatformIO ドキュメント](https://docs.platformio.org/)
- [UHD プロジェクト](https://github.com/Seeed-Studio/ModelAssistant)

## 📝 ライセンス

MIT License

## 👥 作者

UHD-challenge Project Team
