# model_conversion
ONNXモデルをESP-DL形式（`.bin` + `model_config.c`）に変換する手順まとめ。

## 前提環境
- Python 3.10+
- PowerShell (Windows想定)
- esp-dlc, onnxsim, onnxruntime

セットアップ例:
```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install --upgrade pip esp-dlc onnxsim onnxruntime
```

## パイプラインの流れ
1. ONNX入力を用意（例: `models/uhd.onnx`）。
2. onnx-simplifierで簡略化。
3. esp-dlcでESP-DL形式へ変換（必要なら量子化）。
4. 出力された `model.bin` と `model_config.c` をファーム側（例: `M5StackS3/main/`）に配置。

## クイック実行例 (PowerShell)
```powershell
$MODEL = "models/uhd.onnx"        # 入力ONNX
$OUT   = "model_conversion/out"   # 出力先
$SHAPE = "1,3,320,320"            # NCHW 例

mkdir -Force (Split-Path $OUT) | Out-Null
mkdir -Force $OUT | Out-Null

# 1) ONNX簡略化
python -m onnxsim $MODEL "$OUT/uhd_simplified.onnx"

# 2) ESP-DL変換（INT8量子化付き）
esp-dlc ^
  --model "$OUT/uhd_simplified.onnx" ^
  --output $OUT ^
  --input-shape $SHAPE ^
  --mean 0 0 0 --std 255 255 255 ^
  --quantization
```
- 量子化が不要なら `--quantization` を外す。
- 代表データで校正したい場合は esp-dlc の `--calibration-*` オプションを併用。
- 出力物: `$OUT/model.bin`, `$OUT/model_config.c`, ログファイル。

## Pythonでパイプラインを回す例
`python pipeline.py --model models/uhd.onnx --input-shape 1,3,320,320 --out model_conversion/out --quantize`

```python
# pipeline.py
import argparse
import subprocess
from pathlib import Path

def run(cmd):
    print(" ".join(cmd))
    subprocess.run(cmd, check=True)

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--model", required=True, help="入力ONNX")
    p.add_argument("--input-shape", default="1,3,320,320", help="N,C,H,W")
    p.add_argument("--out", default="model_conversion/out")
    p.add_argument("--quantize", action="store_true")
    args = p.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    simplified = out / "model_simplified.onnx"

    run(["python", "-m", "onnxsim", args.model, str(simplified)])

    cmd = [
        "esp-dlc", "--model", str(simplified),
        "--output", str(out),
        "--input-shape", args.input_shape,
        "--mean", "0", "0", "0",
        "--std", "255", "255", "255",
    ]
    if args.quantize:
        cmd.append("--quantization")

    run(cmd)

if __name__ == "__main__":
    main()
```

## 出力後の使い道
- `model.bin` と `model_config.c` を ESP-IDF プロジェクトの `main/` 等に配置してビルド。
- 入力shapeや前処理（mean/std）はファーム側の画像前処理と揃えること。
