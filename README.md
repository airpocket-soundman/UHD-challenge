# UHD-challenge
軽量なUHD系ONNXモデルをESP-DLに変換し、ESP32-S3搭載のM5StackS3で動かすためのリポジトリ。

UHDリポジトリ: https://github.com/PINTO0309/UHD

## 全体の流れ
1. 開発環境準備（Python + esp-dlc + PlatformIO CLI）
2. ONNXモデルの軽量化（モジュール削減・量子化・onnx-simplifier等）
3. ESP-DL形式（`.bin` + `model_config.c`）へ変換
4. M5StackS3用PlatformIOプロジェクトへ組み込み
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

### 2. ONNXモデルの整形・軽量化
- UHD から取得した小型モデル（例: YOLOv8n など）を使用。必要に応じて入力解像度を下げる。
- `onnxsim`で簡略化し、ESP-DLがサポートする演算のみを含むことを確認。
- 量子化が可能な場合は代表データセットでINT8量子化し、推論精度を事前にPC上で確認。

### 3. ESP-DL形式への変換
- esp-dlc を使って `.bin` と `model_config.c` を生成。入力名・入力shape・前処理はモデルに合わせて調整。
```bash
esp-dlc ^
  --model path/to/model.onnx ^
  --input-shape "1,3,320,320" ^
  --mean 0 0 0 --std 255 255 255 ^
  --quantization ^
  --output ./model_conversion/uhd_s3
```
- 生成物は `model_conversion/` にまとめ、バージョン管理する。

### 4. M5StackS3 (PlatformIO) への組み込み
- `M5StackS3/` 配下に PlatformIO プロジェクトを配置。`platformio.ini` で ESP32-S3 ボードとフレームワーク（例: `framework = espidf` または `arduino`）を指定。
- 生成した `model.bin` と `model_config.c` を `src/` などビルド対象ディレクトリに配置し、推論コードから参照する。
- PSRAM利用やカメラ/ディスプレイのピン設定は `sdkconfig` / `sdkconfig.defaults` 相当のオプション、あるいはライブラリ設定で有効化。

### 5. PlatformIOでビルドと書き込み
```bash
cd M5StackS3
pio run -e m5stack-s3            # 環境名は platformio.ini に合わせる
pio run -e m5stack-s3 -t upload  # フラッシュ
pio device monitor -b 115200     # シリアルモニタ
```
- デバイス起動後、シリアルやディスプレイ出力で推論ログを確認。必要に応じてクロックやスレッド数を調整して性能を詰める。

## ディレクトリメモ
- `model_conversion/`: ONNX→ESP-DL 変換物を配置
- `M5StackS3/`: M5StackS3 向けPlatformIOプロジェクト（本体実装）
- `Tab5/`: Tab5 など他ターゲット向けの作業場所
- `Xiao ESP32 S3 Sense/`: Seeed Xiao ESP32S3 Sense向け実験用
