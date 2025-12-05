# model_conversion_tflite
UHD ONNXモデルをTensorFlow Lite形式（`.tflite`）に変換するプロジェクト

## 🎯 このプロジェクトについて

ESP-DLではArgMax演算子が非対応のため、TensorFlow Lite Microを使用します。
TFLite MicroはArgMaxを含む**全ての演算子**に対応しています。

## ✅ メリット

- ✅ **ArgMax完全対応** - 複数出力モデル不要
- ✅ **ESP32公式サポート** - 実績多数
- ✅ **全演算子対応** - オリジナルモデルをそのまま使用可能
- ✅ **コード量削減** - ArgMaxのC++実装不要

## ⚠️ デメリット

- ⚠️ ESP-DLより5-10%遅い可能性
- ⚠️ ONNX→TFLite変換が必要（初回のみ）

---

## 環境構築

### 前提条件
- Python 3.8-3.10（TensorFlowの要件）
- Conda環境（推奨）

### インストール手順

```powershell
# Conda環境を作成
conda create -n uhd-tflite python=3.10 -y
conda activate uhd-tflite

# TensorFlowのインストール
pip install tensorflow

# ONNX関連のインストール
pip install onnx onnx-tf

# 検証用
pip install numpy
```

**注意**: TensorFlowは大きいパッケージです（約500MB）。時間がかかる場合があります。

---

## クイック実行例

### INT8量子化版（推奨）

```powershell
# Conda環境をアクティベート
conda activate uhd-tflite

# Nバリアント（推奨、1.38M）をTFLiteに変換
python convert_to_tflite.py ^
  --model onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx ^
  --output tflite/uhd_n_w64_int8.tflite ^
  --quantize

# Tバリアント（3.1M）
python convert_to_tflite.py ^
  --model onnx/ultratinyod_res_anc8_w96_64x64_quality.onnx ^
  --output tflite/uhd_t_w96_int8.tflite ^
  --quantize
```

### FP32版（量子化なし）

```powershell
# FP32版（精度は高いがサイズ大きい）
python convert_to_tflite.py ^
  --model onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx ^
  --output tflite/uhd_n_w64_fp32.tflite
```

**推奨**: ESP32では**INT8量子化版**を使用してください（`--quantize`オプション）

---

## 変換の流れ

```
1. ONNX → TensorFlow SavedModel
   ↓
2. TensorFlow → TFLite
   ↓
3. INT8量子化（オプション）
   ↓
4. .tfliteファイル生成
```

**処理時間**: 約1-2分（モデルサイズとPCスペックによる）

---

## 出力ファイル

```
model_conversion_tflite/
├── onnx/                           # 入力ONNXモデル
│   ├── ultratinyod_res_anc8_w64_64x64_quality.onnx (N)
│   ├── ultratinyod_res_anc8_w96_64x64_quality.onnx (T)
│   └── ... (他のバリアント)
│
├── tflite/                         # 出力TFLiteモデル
│   ├── uhd_n_w64_int8.tflite      # 推奨
│   ├── uhd_t_w96_int8.tflite
│   └── uhd_n_w64_fp32.tflite      # FP32版
│
├── convert_to_tflite.py            # 変換スクリプト
└── README.md                       # このファイル
```

---

## ESP32での使用方法

### ステップ1: TFLite Microライブラリの追加

`idf_component.yml`:
```yaml
dependencies:
  espressif/esp-tflite-micro: "^1.0.0"
```

### ステップ2: モデルの組み込み

```cpp
// model.h に変換
xxd -i uhd_n_w64_int8.tflite > model.h
```

または、SPIFFSに配置。

### ステップ3: C++コード（サンプル）

```cpp
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// グローバル変数
constexpr int kTensorArenaSize = 300 * 1024;  // 300KB
uint8_t tensor_arena[kTensorArenaSize];

void setup_model() {
    // モデルロード
    const tflite::Model* model = tflite::GetModel(g_model);
    
    // Resolverの設定（必要な演算子のみ登録で軽量化）
    static tflite::MicroMutableOpResolver<20> resolver;
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddReshape();
    resolver.AddSigmoid();
    resolver.AddMul();
    resolver.AddAdd();
    resolver.AddArgMax();  // ★ ArgMaxもサポート
    // ... 他の演算子 ...
    
    // インタープリター作成
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;
    
    // メモリ割り当て
    interpreter->AllocateTensors();
}

void run_inference(uint8_t* image_data) {
    // 入力テンソル取得
    TfLiteTensor* input = interpreter->input(0);
    
    // 画像データをコピー（64x64 RGB）
    // 前処理: [0,255] → [0,1]
    for (int i = 0; i < 64 * 64 * 3; i++) {
        input->data.f[i] = image_data[i] / 255.0f;
    }
    
    // 推論実行
    TfLiteStatus invoke_status = interpreter->Invoke();
    
    // 結果取得（ArgMaxも含めて全て処理済み）
    TfLiteTensor* output = interpreter->output(0);
    
    // 検出結果の解析
    // output->data.f[...] に検出結果が含まれる
}
```

**重要**: TFLite Microでは**ArgMaxを含む全ての後処理がモデル内で完結**します。C++でのArgMax実装は不要です。

---

## 比較: ESP-DL vs TFLite Micro

| 項目 | ESP-DL | TFLite Micro |
|------|--------|--------------|
| **ArgMax対応** | ❌ | ✅ |
| **推論速度** | ⭐⭐⭐ 最速 | ⭐⭐ やや遅い |
| **メモリ使用量** | ⭐⭐⭐ 最小 | ⭐⭐ 適度 |
| **C++実装** | ArgMax必要 | 不要 |
| **複数出力モデル** | 必要 | 不要 |
| **コード量** | 多い | 少ない |
| **メンテナンス** | 複雑 | 簡単 |

---

## トラブルシューティング

### ONNX→TensorFlow変換エラー

**エラー**: `Unsupported ONNX operation`

**対策**:
```powershell
# onnx-tfを最新版に更新
pip install --upgrade onnx-tf

# または特定のバージョン
pip install onnx-tf==1.10.0
```

### TensorFlowのバージョンエラー

**エラー**: `No module named 'tensorflow'`

**対策**:
```powershell
# TensorFlow 2.xをインストール
pip install tensorflow>=2.13.0
```

**注意**: TensorFlow 2.16以降は一部のAPIが変更されている可能性があります。

### メモリ不足エラー（ESP32）

**エラー**: `Failed to allocate tensors`

**対策**:
1. `kTensorArenaSize`を増やす（例: 300KB → 400KB）
2. PSRAMを有効化（`menuconfig`で設定）
3. より小さいバリアント（N）を使用

### 推論速度が遅い

**対策**:
1. INT8量子化を使用（`--quantize`オプション）
2. 不要な演算子をResolverから除外
3. ESP32のCPU周波数を240MHzに設定

---

## 参考リンク

- [TensorFlow Lite Micro 公式](https://www.tensorflow.org/lite/microcontrollers)
- [ESP-TFLite-Micro GitHub](https://github.com/espressif/esp-tflite-micro)
- [ONNX-TF GitHub](https://github.com/onnx/onnx-tensorflow)
- [UHD GitHub](https://github.com/CheungBH/UHD)

---

## まとめ

**TFLite Microを使用することで**:
- ✅ ArgMaxを含む全演算子が動作
- ✅ C++でのArgMax実装不要
- ✅ 複数出力モデル不要
- ✅ メンテナンスが容易

**ESP-DLと比較して**:
- ⚠️ 5-10%遅い可能性
- ✅ しかし開発効率は大幅に向上

**推奨**: 開発効率を重視する場合はTFLite Microを使用してください。
