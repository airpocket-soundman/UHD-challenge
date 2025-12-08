# ESP32向けNNフレームワークの比較

## 主要なフレームワーク

### 1. ESP-DL (Espressif公式)
**公式サイト**: https://github.com/espressif/esp-dl

**特徴**:
- ESP32専用に最適化
- ハードウェアアクセラレーション対応
- ESP-PPQによるINT8量子化

**ArgMaxサポート**: ❌ **非対応**

**その他の演算子**: Conv, MaxPool, Add, Mul等は対応

---

### 2. TensorFlow Lite Micro (TFLite Micro)
**公式サイト**: https://www.tensorflow.org/lite/microcontrollers

**特徴**:
- TensorFlowの軽量版
- 多くのマイコンで動作
- ESP32対応（公式サポート）

**ArgMaxサポート**: ✅ **対応**
- TFLite演算子リストでArgMaxは標準対応
- https://www.tensorflow.org/lite/guide/ops_compatibility

**変換方法**:
```python
import tensorflow as tf

# ONNXからTFLite変換（onnx-tfを使用）
# 1. ONNX → TensorFlow
# 2. TensorFlow → TFLite

# または
converter = tf.lite.TFLiteConverter.from_saved_model('model')
converter.optimizations = [tf.lite.Optimize.DEFAULT]
tflite_model = converter.convert()
```

**メリット**:
- ✅ ArgMaxを含むほぼ全ての演算子に対応
- ✅ 豊富なドキュメント
- ✅ 活発なコミュニティ

**デメリット**:
- ❌ ESP-DLより推論速度が遅い可能性
- ❌ ONNX→TFLite変換が複雑

---

### 3. ONNX Runtime (マイコン版)
**公式サイト**: https://onnxruntime.ai/

**特徴**:
- ONNX形式を直接実行
- マイコン向けビルドが存在

**ArgMaxサポート**: ✅ **対応**
- ONNXの標準演算子なので対応

**ESP32サポート**: ⚠️ **実験的**
- 公式にはESP32明記なし
- メモリ要件が大きい（2MB以上推奨）

**メリット**:
- ✅ ONNXを直接使用可能
- ✅ 全ONNX演算子に対応

**デメリット**:
- ❌ ESP32での動作実績が少ない
- ❌ メモリ消費が大きい

---

### 4. NNoM (Neural Network on Microcontroller)
**公式サイト**: https://github.com/majianjia/nnom

**特徴**:
- 中国製の軽量NNライブラリ
- Keras/TFLiteからの変換対応

**ArgMaxサポート**: ✅ **対応**
- 基本的な演算子はサポート

**ESP32サポート**: ✅ **対応**
- STM32がメインだがESP32でも動作報告あり

**メリット**:
- ✅ 軽量
- ✅ シンプルなAPI

**デメリット**:
- ❌ ドキュメントが少ない
- ❌ コミュニティが小さい

---

### 5. Edge Impulse
**公式サイト**: https://www.edgeimpulse.com/

**特徴**:
- クラウドベースのML開発プラットフォーム
- ESP32公式サポート

**ArgMaxサポート**: ✅ **対応**（内部的にTFLite Microを使用）

**メリット**:
- ✅ ブラウザベースで簡単
- ✅ ESP32への自動デプロイ
- ✅ ArgMax含む多くの演算子対応

**デメリット**:
- ❌ クラウド依存
- ❌ カスタムモデルの制限

---

## UHDモデルでの推奨フレームワーク

### 結論: **TensorFlow Lite Micro** を推奨

**理由**:
1. ✅ **ArgMaxを含む全演算子に対応**
2. ✅ ESP32公式サポート
3. ✅ 豊富なドキュメントと例
4. ✅ 活発なコミュニティ

### TFLite Microへの変換手順

#### ステップ1: ONNX → TensorFlow変換
```bash
pip install onnx-tf

# ONNX → TensorFlow SavedModel
onnx-tf convert -i model.onnx -o model_tf
```

#### ステップ2: TensorFlow → TFLite変換
```python
import tensorflow as tf

converter = tf.lite.TFLiteConverter.from_saved_model('model_tf')

# INT8量子化
converter.optimizations = [tf.lite.Optimize.DEFAULT]

# 変換
tflite_model = converter.convert()

# 保存
with open('model.tflite', 'wb') as f:
    f.write(tflite_model)
```

#### ステップ3: ESP32での使用
```cpp
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// モデルロード
const tflite::Model* model = tflite::GetModel(model_data);

// インタープリター作成
tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, kTensorArenaSize);
interpreter.AllocateTensors();

// 推論実行
TfLiteTensor* input = interpreter.input(0);
// ... 入力データ設定 ...
interpreter.Invoke();

// 結果取得（ArgMaxも含めて全て処理される）
TfLiteTensor* output = interpreter.output(0);
```

---

## 比較表

| フレームワーク | ArgMax対応 | ESP32サポート | 推論速度 | メモリ消費 | 導入難易度 |
|---------------|-----------|--------------|---------|-----------|----------|
| **ESP-DL** | ❌ | ✅✅✅ | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ |
| **TFLite Micro** | ✅ | ✅✅ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ |
| **ONNX Runtime** | ✅ | ⚠️ | ⭐ | ⭐ | ⭐ |
| **NNoM** | ✅ | ✅ | ⭐⭐ | ⭐⭐⭐ | ⭐⭐ |
| **Edge Impulse** | ✅ | ✅✅ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ |

**評価**: ✅=完全対応, ⚠️=実験的, ⭐=低い/簡単, ⭐⭐⭐=高い/難しい

---

## 推奨アプローチ

### オプション1: TFLite Micro（推奨）
**メリット**:
- ArgMaxを含む全演算子が動作
- ESP32で実績あり
- 複数出力モデル不要

**デメリット**:
- ONNX→TFLite変換が必要
- ESP-DLより少し遅い可能性

### オプション2: ESP-DL + 複数出力モデル（現在の方法）
**メリット**:
- ESP32に最適化
- 最高速度
- 既に変換済み

**デメリット**:
- ArgMaxをC++で実装必要
- 複数出力モデルの管理

### オプション3: Edge Impulse（最も簡単）
**メリット**:
- ブラウザで完結
- 自動最適化
- ArgMax完全サポート

**デメリット**:
- クラウド依存
- カスタマイズ制限

---

## 結論

**ArgMaxを含む全演算子に対応するには、TensorFlow Lite Microが最適です。**

ただし、現在のESP-DL + 複数出力モデルでも十分に動作し、最高のパフォーマンスが得られます。

### 判断基準:
- **速度重視** → ESP-DL + ArgMax実装（現在の方法）
- **開発効率重視** → TensorFlow Lite Micro
- **最も簡単** → Edge Impulse
