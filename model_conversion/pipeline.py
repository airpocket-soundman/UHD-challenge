"""ONNX -> ESP-DL 変換パイプライン。
PowerShell 例:
python pipeline.py --model models/uhd.onnx --input-shape 1,3,320,320 --out model_conversion/out --quantize
"""
import argparse
import subprocess
from pathlib import Path


def run(cmd):
    print(" ".join(map(str, cmd)))
    subprocess.run(cmd, check=True)


def main():
    parser = argparse.ArgumentParser(description="ONNXをESP-DL形式に変換するパイプライン")
    parser.add_argument("--model", required=True, help="入力ONNXファイルパス")
    parser.add_argument("--input-shape", default="1,3,320,320", help="N,C,H,W 形式の入力shape")
    parser.add_argument("--out", default="model_conversion/out", help="出力ディレクトリ")
    parser.add_argument("--mean", nargs=3, default=[0, 0, 0], help="RGB mean")
    parser.add_argument("--std", nargs=3, default=[255, 255, 255], help="RGB std")
    parser.add_argument("--quantize", action="store_true", help="INT8量子化を有効化")
    parser.add_argument("--calib-data", help="代表データセットディレクトリ (esp-dlc calibration用)")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    simplified = out_dir / "model_simplified.onnx"

    # 1) ONNXを簡略化
    run(["python", "-m", "onnxsim", args.model, str(simplified)])

    # 2) ESP-DL 形式に変換
    cmd = [
        "esp-dlc", "--model", str(simplified),
        "--output", str(out_dir),
        "--input-shape", args.input_shape,
        "--mean", *map(str, args.mean),
        "--std", *map(str, args.std),
    ]

    if args.quantize:
        cmd.append("--quantization")
    if args.calib_data:
        cmd += ["--calibration-data", args.calib_data]

    run(cmd)

    print("\nDone. Outputs:")
    print(f"- {out_dir / 'model.bin'}")
    print(f"- {out_dir / 'model_config.c'}")


if __name__ == "__main__":
    main()
