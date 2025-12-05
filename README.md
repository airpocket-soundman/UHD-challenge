# UHD-challenge
軽量なUHD系ONNXモデルをESP-DLに変換し、ESP32-S3搭載のM5StackS3で動かすためのリポジトリ。

UHDリポジトリ: https://github.com/PINTO0309/UHD

## UHDモデルについて
UHD（Ultra-lightweight Human Detection）は64x64の超低解像度入力で動作する人物検出モデルです。
- **入力**: 64x64 RGB（YOLOなどの高機能モデルより遥かに軽量）
- **出力**: バウンディングボックス（x,y,w,h）+ objectness + class確率
- **アーキテクチャ**: UltraTinyOD（アンカーベース、8アンカー、出力stride=8）
- **前処理**: [0,255] → [0,1] 正規化（mean=0, std=255）
- **推奨バリアント**: N (1.38M params) または T (3.1M params) がESP32-S3に適している

## 全体の流れ
1. 開発環境準備（Python + esp-dlc + PlatformIO CLI）
2. UHD事前学習モデルのダウンロード
3. ONNXモデルの簡略化とESP-DL形式（`.bin` + `model_config.c`）へ変換
4. M5StackS3用PlatformIOプロジェクトへ組み込み
   - カメラ入力処理（リサイズ: 320x240等 → 64x64）
   - モデル推論
   - 後処理（アンカーデコード + NMS）
   - 結果描画
5. PlatformIOでビルド・書き込み・実機確認

## 手順
### 1. 開発環境
- Python 3.10+ の仮想環境を作成し、esp-dlc と検証用ツールを導入。
```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install --upgrade pip esp-dlc onnxsim onnxruntime
```
- PlatformIO CLI をインストール（VSCode拡張または `pip install platformio`）。
- M5StackS3 用のUSBシリアルドライバを導入。

### 2. UHDモデルのダウンロード
UHDの事前学習モデルをダウンロード（ESP32-S3には N または T バリアントを推奨）:

**Nバリアント（最軽量、1.38M params、推奨）**
```powershell
curl -L -o models\uhd_n_64x64.onnx https://github.com/PINTO0309/UHD/releases/download/onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx
```

**Tバリアント（バランス型、3.1M params）**
```powershell
curl -L -o models\uhd_t_64x64.onnx https://github.com/PINTO0309/UHD/releases/download/onnx/ultratinyod_res_anc8_w96_64x64_quality.onnx
```

**Sバリアント（高性能、5.43M params、ESP32では重い可能性あり）**
```powershell
curl -L -o models\uhd_s_64x64.onnx https://github.com/PINTO0309/UHD/releases/download/onnx/ultratinyod_res_anc8_w128_64x64_quality.onnx
```

**注意**: UHDモデルは64x64入力専用です。他の解像度は推奨されません。

### 3. ESP-DL形式への変換
ESP-PPQ を使って `.espdl` 形式に変換。

**重要**: UHDは64x64入力専用です。
```powershell
# Conda環境をアクティベート
conda activate uhd-challenge

# Nバリアント（推奨）の変換
python model_conversion\convert_to_espdl.py ^
  --model model_conversion\onnx\ultratinyod_res_anc8_w64_64x64_quality.onnx ^
  --output model_conversion\uhd_n_w64 ^
  --input-shape "1,3,64,64"

# Tバリアント
python model_conversion\convert_to_espdl.py ^
  --model model_conversion\onnx\ultratinyod_res_anc8_w96_64x64_quality.onnx ^
  --output model_conversion\uhd_t_w96 ^
  --input-shape "1,3,64,64"
```

**パラメータの説明**:
- ESP-PPQが自動的にINT8量子化を実行
- [0,255] → [0,1] 正規化はモデルに組み込み済み
- 入力shape: **必ず 1,3,64,64** （UHD仕様）
- 出力: `.espdl` ファイル（ESP-DL専用形式）

**注意**: ESP-PPQにはPyTorchが必要です。conda環境で実行してください。

### 4. M5StackS3 (PlatformIO) への組み込み
- `M5StackS3/` 配下に PlatformIO プロジェクトを配置。`platformio.ini` で ESP32-S3 ボードとフレームワーク（`framework = espidf`）を指定。
- 生成した `.espdl` ファイルを `main/` または `src/` ディレクトリに配置し、推論コードから参照する。
- ESP-DLライブラリをプロジェクトに追加（`idf_component.yml`で`espressif/esp-dl`を指定）
- PSRAM利用やカメラ/ディスプレイのピン設定は `sdkconfig` で有効化。

**実装が必要な処理フロー**:
```
1. カメラから画像取得（例: 320x240 RGB）
2. 64x64にリサイズ（バイリニア補間）
3. [0,1]正規化（÷255.0）
4. ESP-DLで推論実行
5. 後処理:
   - アンカーデコード（8アンカー × 8x8グリッド = 512候補）
   - NMS（Non-Maximum Suppression）
   - 信頼度フィルタリング（conf_thresh ≈ 0.15）
6. 検出結果を元の解像度にスケールバック
7. ディスプレイに描画
```

**注意事項**:
- UHDの出力形式: `[batch, num_predictions, 5+num_classes]` または類似形式
- 後処理（デコード+NMS）の実装が必須
- PSRAMの有効化を推奨（モデルサイズ + 推論バッファ用）

### 5. PlatformIOでビルドと書き込み
```bash
cd M5StackS3
pio run -e m5stack-s3            # 環境名は platformio.ini に合わせる
pio run -e m5stack-s3 -t upload  # フラッシュ
pio device monitor -b 115200     # シリアルモニタ
```
- デバイス起動後、シリアルやディスプレイ出力で推論ログを確認。必要に応じてクロックやスレッド数を調整して性能を詰める。

## ディレクトリ構成
```
UHD-challenge/
├── README.md                      # このファイル
├── requirements.txt               # Python依存関係（要追加）
├── .gitignore                     # Git除外設定（要追加）
├── models/                        # ダウンロードしたONNXモデル格納（要作成）
│   ├── uhd_n_64x64.onnx
│   └── uhd_t_64x64.onnx
├── model_conversion/              # ONNX→ESP-DL 変換スクリプトと出力
│   ├── pipeline.py                # 変換パイプライン
│   ├── README.md                  # 変換手順の詳細
│   └── uhd_n_s3/                  # 変換済みモデル出力（例）
│       ├── model.bin
│       └── model_config.c
├── M5StackS3/                     # M5StackS3向けPlatformIOプロジェクト（要実装）
│   ├── platformio.ini
│   ├── src/
│   │   ├── main.cpp               # メイン推論コード
│   │   ├── model.bin              # 変換済みモデル
│   │   └── model_config.c
│   └── README.md
├── Tab5/                          # Tab5向け（将来的な拡張）
│   └── README.md
└── Xiao ESP32 S3 Sense/           # Xiao ESP32S3 Sense向け
    └── README.md
```

## 既知の課題・TODO
- [x] requirements.txt と .gitignore の追加
- [x] ESP-PPQ変換スクリプトの作成
- [ ] ESP-PPQ/ONNXバージョン互換性の解決
- [ ] M5StackS3のPlatformIOプロジェクト実装
- [ ] カメラ入力とリサイズ処理の実装
- [ ] 後処理（アンカーデコード + NMS）の実装
- [ ] ESP-DLサポート演算の検証
- [ ] 実機でのパフォーマンス測定

## 参考リンク
- [UHD GitHub](https://github.com/PINTO0309/UHD) - モデル詳細とトレーニングコード
- [ESP-DL Documentation](https://github.com/espressif/esp-dl) - ESP-DLフレームワーク
- [M5Stack CoreS3](https://docs.m5stack.com/en/core/CoreS3) - ハードウェア仕様
