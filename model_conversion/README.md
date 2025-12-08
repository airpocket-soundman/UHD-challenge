# model_conversion

UHD ONNXモデル（ReLU版）をESP-DL形式（`.espdl`）に変換する手順。

## 使用するモデル

**推奨モデル**: `w_ESE+IoU-aware+ReLU/ultratinyod_res_anc8_w64_64x64_quality_relu_nopost.onnx`

- ✅ **すべてのオペレータがESP-DLでサポート済み**
- ✅ ReLU活性化関数使用（効率的）
- ✅ ArgMax演算子なし（変換が簡単）
- ✅ 最軽量バリアント（w64 = N variant）

## モデルの出力形式

**3つの出力**:

1. **pred** `[1, 56, 8, 8]` - 予測値（動的）
   - 8×8グリッド上の各セルについて56チャンネルの予測データ
   - 物体の存在確率、バウンディングボックス座標、クラス確率を含む

2. **anchors** `[8, 2]` - アンカーボックス（定数）
   - 8種類のアンカーボックスサイズ [width, height]
   - 小さい物体用〜大きい物体用

3. **wh_scale** `[8, 2]` - スケール係数（定数）
   - 各アンカーの幅・高さをスケーリングする係数
   - バウンディングボックスの最終サイズを調整

**重要**: anchorsとwh_scaleは定数なので、モデルから分離してC++コードにハードコーディングします。

---

## 環境構築

### 前提条件
- Windows 10/11（PowerShell）
- Conda（Anaconda/Miniconda）
- Git

### セットアップ手順

```powershell
# 1. Conda環境を作成
conda create -n uhd-challenge python=3.10 -y
conda activate uhd-challenge

# 2. PyTorchをインストール（CPU版）
pip install torch torchvision --index-url https://download.pytorch.org/whl/cpu

# 3. ESP-PPQをインストール
git clone https://github.com/espressif/esp-ppq.git C:\Users\%USERNAME%\Documents\GitHub\esp-ppq-temp
cd C:\Users\%USERNAME%\Documents\GitHub\esp-ppq-temp
pip install -e .

# 4. その他の依存関係
pip install onnx==1.16.0 onnxruntime

# 5. ESP-PPQ互換性修正
# esp_ppq/parser/onnx_parser.py の Line 113-119 を修正:
```

**onnx_parser.py修正内容**:
```python
if op.type == 'Cast' and key == 'to':
    # Support both dict-style and function-style access
    if callable(helper.tensor_dtype_to_np_dtype):
        value = helper.tensor_dtype_to_np_dtype(value)
    else:
        value = helper.tensor_dtype_to_np_dtype[value]
    op.attributes[key] = value
```

### 環境確認

```powershell
python -c "import torch; import esp_ppq; import onnx; print('PyTorch:', torch.__version__); print('ONNX:', onnx.__version__); print('ESP-PPQ: OK')"
```

---

## 変換手順

### ステップ1: モデルを解析（オプション）

変換前にモデルの構造とESP-DL互換性を確認:

```powershell
python model_conversion\analyze_relu_model.py
```

出力例:
```
✓✓✓ ALL OPERATORS ARE SUPPORTED BY ESP-DL! ✓✓✓
Number of outputs: 3
✓ This is a 3-output model (pred, anchors, wh_scale)
```

### ステップ2: シングル出力モデルを作成

3つの出力のうち、anchorsとwh_scaleは定数なので分離します:

```powershell
python model_conversion\create_single_output_model.py ^
  --input model_conversion\w_ESE+IoU-aware+ReLU\ultratinyod_res_anc8_w64_64x64_quality_relu_nopost.onnx ^
  --output model_conversion\w_ESE+IoU-aware+ReLU\translated\uhd_relu_w64_single.onnx ^
  --constants model_conversion\w_ESE+IoU-aware+ReLU\translated\uhd_relu_w64_constants.npz
```

**生成されるファイル** (`w_ESE+IoU-aware+ReLU/translated/`):
- `uhd_relu_w64_single.onnx` - pred出力のみのモデル
- `uhd_relu_w64_constants.npz` - anchorsとwh_scaleの定数

### ステップ3: ESP-DL形式に変換

シングル出力モデルをESP-DL形式に変換（INT8量子化込み）:

```powershell
conda run -n uhd-challenge python model_conversion\convert_to_espdl.py ^
  --model model_conversion\w_ESE+IoU-aware+ReLU\translated\uhd_relu_w64_single.onnx ^
  --output model_conversion\w_ESE+IoU-aware+ReLU\translated\uhd_relu_w64 ^
  --input-shape "1,3,64,64"
```

**生成されるファイル** (`w_ESE+IoU-aware+ReLU/translated/`):
- `uhd_relu_w64.espdl` - ESP-DL形式モデル（INT8量子化済み）
- `uhd_relu_w64.json` - モデルメタデータ
- `uhd_relu_w64.info` - 変換詳細情報

### ステップ4: .binファイルを作成（オプション）

ESP32で読み込みやすい.bin形式に変換:

```powershell
python model_conversion\convert_constants_to_bin.py ^
  --input model_conversion\w_ESE+IoU-aware+ReLU\translated\uhd_relu_w64_constants.npz ^
  --output model_conversion\w_ESE+IoU-aware+ReLU\translated\uhd_relu_w64_constants.bin
```

**生成されるファイル**:
- `uhd_relu_w64_constants.bin` (128 bytes) - バイナリ定数ファイル

### ステップ5: M5StackS3にデプロイ

**microSDへの配置（推奨）**:

```powershell
# 1. microSDのルートに、汎用名にリネームして保存
copy model_conversion\w_ESE+IoU-aware+ReLU\translated\uhd_relu_w64.espdl <microSD_path>\model.espdl
copy model_conversion\w_ESE+IoU-aware+ReLU\translated\uhd_relu_w64_constants.bin <microSD_path>\model.bin
```

**microSDのファイル構成**:
```
microSD (root)/
  ├── model.espdl  (1.68 MB) - ESP-DLモデル
  └── model.bin    (128 bytes) - anchors/wh_scale定数
```

**ESP32での読み込み**:
```cpp
// SDカードをマウント
ESP_ERROR_CHECK(bsp_sdcard_mount());

// モデルをロード
Model *model = new Model("/sdcard/model.espdl", fbs::MODEL_LOCATION_IN_SDCARD);

// 定数をロード
ModelConstants consts;
FILE* f = fopen("/sdcard/model.bin", "rb");
if (f) {
    fread(consts.anchors, sizeof(float), 8 * 2, f);
    fread(consts.wh_scale, sizeof(float), 8 * 2, f);
    fclose(f);
}
```

**メリット**:
- ✅ モデル変更時もファームウェア再書き込み不要
- ✅ 複数のバリアントを切り替え可能
- ✅ 開発・デバッグが容易

---

## ESP32での実装

### 1. モデルのロード

```cpp
#include "dl_model_base.hpp"

// モデルファイルをロード
extern const uint8_t model_data[] asm("_binary_uhd_n_w64_relu_start");
Model *model = new Model((const char*)model_data, fbs::MODEL_LOCATION_IN_FLASH_RODATA);
```

### 2. 定数の読み込み

anchorsとwh_scaleは**モデルごとに異なる**ため、microSDから読み込むことを推奨します。

#### オプションA: .npzファイルから読み込む（推奨）

**手順**:
1. `create_single_output_model.py`で生成された`uhd_relu_constants.npz`をmicroSDに配置
2. ESP32で読み込み

```cpp
// 簡易的なバイナリ読み込み例
struct ModelConstants {
    float anchors[8][2];
    float wh_scale[8][2];
};

ModelConstants load_constants_from_file(const char* path) {
    ModelConstants constants;
    FILE* f = fopen(path, "rb");
    // .npz は複雑なので、代わりに.binを使う（後述）
    fclose(f);
    return constants;
}
```

#### オプションB: .bin形式で保存（シンプル）

Pythonで.npzを.binに変換:

```python
import numpy as np

# .npzから読み込み
data = np.load("uhd_relu_constants.npz")
anchors = data['anchors'].astype(np.float32)
wh_scale = data['wh_scale'].astype(np.float32)

# .binとして保存（読み込みが簡単）
with open("uhd_relu_constants.bin", "wb") as f:
    anchors.tofile(f)
    wh_scale.tofile(f)
```

ESP32で読み込み:

```cpp
struct ModelConstants {
    float anchors[8][2];
    float wh_scale[8][2];
};

ModelConstants load_constants_from_bin(const char* path) {
    ModelConstants constants;
    FILE* f = fopen(path, "rb");
    if (f) {
        fread(constants.anchors, sizeof(float), 8 * 2, f);
        fread(constants.wh_scale, sizeof(float), 8 * 2, f);
        fclose(f);
        ESP_LOGI(TAG, "Loaded constants from %s", path);
    } else {
        ESP_LOGE(TAG, "Failed to open %s", path);
    }
    return constants;
}

// 使用例
ModelConstants consts = load_constants_from_bin("/sdcard/uhd_relu_constants.bin");
```

#### オプションC: ハードコード（単一モデル専用）

バリアント切り替えが不要な場合のみ:

```cpp
// ⚠️ 警告: この値は ultratinyod_res_anc8_w64_64x64_quality_relu_nopost.onnx 専用
const float ANCHORS[8][2] = {
    {2.2193656e-06f, 4.8251577e-06f},
    {4.4204730e-06f, 8.9109344e-06f},
    {5.5083538e-06f, 2.0233399e-05f},
    {1.0438751e-05f, 1.4592547e-05f},
    {1.1409107e-05f, 3.5738733e-05f},
    {1.9950507e-05f, 5.5220753e-05f},
    {3.5157802e-05f, 7.3149429e-05f},
    {6.7565757e-05f, 8.7956054e-05f}
};

const float WH_SCALE[8][2] = {
    {0.98867685f, 0.98867685f},
    {0.98867685f, 0.98867685f},
    {0.98867685f, 0.98867685f},
    {0.98867685f, 0.98867685f},
    {0.98867685f, 0.98867685f},
    {0.98867685f, 0.98867685f},
    {0.98867685f, 0.98867685f},
    {6.733903f,   7.456346f}
};
```

**定数値**:
```cpp
const int NUM_CLASSES = 80;  // COCO classes
const int GRID_SIZE = 8;     // 8x8 grid
const int NUM_ANCHORS = 8;   // 8 anchors
```

### 3. 推論の実行

```cpp
// 入力画像の準備 (64x64 RGB, normalized to [0,1])
float input_data[1 * 3 * 64 * 64];
// カメラから画像を取得してリサイズ・正規化
// ... 

// 推論実行
Tensor<float> input(input_data, {1, 3, 64, 64});
std::vector<Tensor<float>> outputs = model->forward(input);

// 出力: pred [1, 56, 8, 8]
Tensor<float>& pred = outputs[0];
```

### 4. 後処理（デコード）

predの生の出力をデコードして検出結果を取得します。詳細なデコード処理は`demo.py`の`decode_ultratinyod_raw`関数を参照してください。

**基本的な流れ**:
1. predから各グリッドセル・各アンカーの予測値を抽出
2. sigmoid/softplus関数で値を変換
3. anchorsとwh_scaleを使ってバウンディングボックスを計算
4. 信頼度でフィルタリング
5. NMS（Non-Maximum Suppression）で重複を除去

---

## 利用可能なツール

### 変換ツール

1. **`create_single_output_model.py`** - 3出力→1出力に変換
   - anchorsとwh_scaleを.npzファイルに抽出
   
2. **`convert_to_espdl.py`** - ONNX→ESP-DL変換
   - INT8量子化を自動実行

3. **`convert_constants_to_bin.py`** - .npz→.bin変換
   - ESP32で簡単に読み込める.bin形式に変換
   - 使用例: `python model_conversion\convert_constants_to_bin.py --input uhd_relu_constants.npz`

### 解析ツール

1. **`analyze_relu_model.py`** - モデル構造とESP-DL互換性を確認
2. **`check_nopost_model.py`** - モデル詳細情報を表示
3. **`demo.py`** - Python推論デモ（カメラ/画像入力）

---

## サポートされているオペレータ

ReLU版_nopostモデルで使用されているすべてのオペレータ:

| オペレータ | 使用回数 | ESP-DLサポート |
|-----------|---------|---------------|
| Conv | 39 | ✅ サポート |
| Relu | 33 | ✅ サポート |
| Reshape | 5 | ✅ サポート |
| Add | 3 | ✅ サポート |
| Concat | 2 | ✅ サポート |
| MaxPool | 1 | ✅ サポート |
| Mul | 1 | ✅ サポート |
| Sigmoid | 1 | ✅ サポート |
| ReduceMean | 1 | ✅ サポート |

**総計**: 9種類、86ノード - **すべてサポート済み** ✅

---

## トラブルシューティング

### ESP-PPQ変換エラー

**エラー**: `TypeError: 'function' object is not subscriptable`

**解決**: esp_ppq/parser/onnx_parser.py を修正（環境構築のステップ5参照）

### モデルが大きすぎる

**解決**: 
- より小さいバリアントを使用（w64が最小）
- ESP32-S3のPSRAMを有効化

### 推論が遅い

**最適化**:
- INT8量子化を確認（自動で実行されます）
- ESP32のクロック周波数を上げる
- 不要な後処理を削減

---

## 参考リンク

- [ESP-DL Documentation](https://github.com/espressif/esp-dl)
- [ESP-PPQ GitHub](https://github.com/espressif/esp-ppq)
- [UHD GitHub](https://github.com/CheungBH/UHD)
- [ESP32-S3 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)

---

## 旧モデル（ArgMax含む）について

古いモデルにはArgMax演算子が含まれており、ESP-DLでサポートされていませんでした。これらのモデルと関連ファイルは`old_argmax_related/`フォルダに移動されています。

**現在のReLU版_nopostモデルでは、この問題は発生しません。**
