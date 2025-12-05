# model_conversion
UHD ONNXモデルをESP-DL形式（`.espdl`）に変換する手順まとめ。

## UHD仕様の重要事項
- **入力解像度**: 64x64 RGB固定（他の解像度は非推奨）
- **正規化**: [0,255] → [0,1] (mean=0, std=255)
- **推奨バリアント**: N (1.38M) または T (3.1M) がESP32-S3向け
- **変換ツール**: ESP-PPQ（esp-dlcというツールは存在しない）

## 変換環境の構築

### 前提条件
- Windows 10/11（PowerShell）
- Conda（Anaconda/Miniconda）
- Git

### 完全な環境構築手順

#### ステップ1: Conda環境の作成
```powershell
# Python 3.10環境を作成
conda create -n uhd-challenge python=3.10 -y

# 環境をアクティベート
conda activate uhd-challenge
```

#### ステップ2: PyTorchのインストール
```powershell
# CPU版PyTorchをインストール（GPU不要）
pip install torch torchvision --index-url https://download.pytorch.org/whl/cpu
```

#### ステップ3: ESP-PPQ最新版のインストール
```powershell
# GitHubからクローン（最新版を使用）
git clone https://github.com/espressif/esp-ppq.git C:\Users\%USERNAME%\Documents\GitHub\esp-ppq-temp

# インストール
cd C:\Users\%USERNAME%\Documents\GitHub\esp-ppq-temp
pip install -e .
```

#### ステップ4: その他の依存パッケージ
```powershell
# ONNX 1.16.0（互換性確認済み）
pip install onnx==1.16.0

# ONNXRuntime（検証用）
pip install onnxruntime
```

#### ステップ5: ESP-PPQ互換性修正
```powershell
# onnx_parser.pyを編集
code C:\Users\%USERNAME%\Documents\GitHub\esp-ppq-temp\esp_ppq\parser\onnx_parser.py
```

以下のように修正（Line 113-119付近）:
```python
if op.type == 'Cast' and key == 'to':
    # The attribute of 'Cast' node is data type (represented in int), need to convert to numpy data type
    # Support both dict-style and function-style access for compatibility
    if callable(helper.tensor_dtype_to_np_dtype):
        value = helper.tensor_dtype_to_np_dtype(value)
    else:
        value = helper.tensor_dtype_to_np_dtype[value]
    op.attributes[key] = value
```

#### ステップ6: 環境確認
```powershell
# 環境情報を表示
conda activate uhd-challenge
python -c "import torch; import esp_ppq; import onnx; print('PyTorch:', torch.__version__); print('ONNX:', onnx.__version__); print('ESP-PPQ installed')"
```

**期待される出力**:
```
PyTorch: 2.5.1+cpu
ONNX: 1.16.0
ESP-PPQ installed
```

### 環境の再アクティベート
次回以降は以下のコマンドのみ:
```powershell
conda activate uhd-challenge
```

## パイプラインの流れ
1. UHD事前学習モデルを用意（`model_conversion/onnx/`に既に存在）
2. ESP-PPQでESP-DL形式へ変換（自動的にINT8量子化）
3. 出力された `.espdl` ファイルをファーム側（例: `M5StackS3/main/`）に配置

**注意**: onnxsimによる簡略化は不要です。ESP-PPQが直接変換します。

## クイック実行例 (PowerShell)
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

**パラメータ説明**:
- `--model`: 入力ONNXファイル（model_conversion/onnx/に格納済み）
- `--output`: 出力ファイル名（拡張子.espdlは自動付与）
- `--input-shape`: **必ず "1,3,64,64"** （UHD仕様）

**ESP-PPQの動作**:
- 自動的にINT8量子化を実行
- ダミーキャリブレーションデータを生成（32サンプル）
- [0,1]正規化はモデルに組み込み済み

**出力物**: 
- `model_conversion\uhd_n_w64.espdl` - ESP-DL形式モデル

## 変換スクリプト（convert_to_espdl.py）について
このスクリプトはESP-PPQ APIを使用してONNX→ESP-DL変換を実行します：

```python
# 主な機能
1. ダミーキャリブレーションデータの自動生成
2. ESP-DL向けINT8量子化
3. .espdl形式でのエクスポート
```

**旧pipeline.pyについて**: `pipeline.py`はonnxsim用で、ESP-DL変換には不要です。

## 出力後の使い道
- `.espdl` ファイルを ESP-IDF プロジェクトの `main/` に配置してビルド
- ESP-DLライブラリの追加（`idf_component.yml`で指定）
- ファーム側で必要な処理:
  1. カメラ画像を64x64にリサイズ
  2. [0,1]正規化（÷255.0）
  3. ESP-DLで.espdlモデルをロード
  4. ESP-DLで推論実行
  5. 後処理（アンカーデコード + NMS）

## 検証（推奨）
変換前のONNXモデルで動作確認:
```python
import onnxruntime as ort
import numpy as np

# オリジナルONNXで推論テスト
session = ort.InferenceSession("model_conversion/onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx")
dummy_input = np.random.rand(1, 3, 64, 64).astype(np.float32)
output = session.run(None, {session.get_inputs()[0].name: dummy_input})
print(f"Output shape: {output[0].shape}")
print(f"Output: {[o.shape for o in output]}")
```

## 発生した問題と対策

### 問題1: ArgMax演算子がESP-DLで非サポート

**問題の詳細**:
- UHDモデルにはArgMax演算子が1つ含まれる
- ArgMaxはクラススコアから最大値のインデックス（クラスID）を取得する演算
- ESP-DLがArgMaxをサポートしていないため、変換が失敗

**分析結果**:
```
最終出力 "detections" = Concat([6要素])
  Element 1: 検出スコア（TopK出力）        ← ArgMax非依存
  Element 2: クラスID                      ← ArgMax依存 ❌
  Element 3-6: バウンディングボックス座標  ← ArgMax非依存
```

**重要な発見**:
- ArgMaxは最終段では**ない**（後にConcat処理がある）
- ArgMaxの入力（クラススコア）だけを出力しても、他の5つのパイプライン（TopK, GatherElements等）が実行されない
- 正しい最終出力を得るには、6つ全てのパイプラインの出力が必要

**対策**: 複数出力モデルの作成
```powershell
# 1. 複数出力モデルを作成（6出力、ArgMax除去）
python model_conversion\create_multi_output_model.py ^
  --input model_conversion\onnx\ultratinyod_res_anc8_w64_64x64_quality.onnx ^
  --output model_conversion\onnx\ultratinyod_res_anc8_w64_64x64_quality_multi_output.onnx

# 2. 複数出力モデルをESP-DL形式に変換
python model_conversion\convert_to_espdl.py ^
  --model model_conversion\onnx\ultratinyod_res_anc8_w64_64x64_quality_multi_output.onnx ^
  --output model_conversion\uhd_n_w64_multi ^
  --input-shape "1,3,64,64"
```

**生成されるモデル**:
- `uhd_n_w64_multi` (1.50 MB) - 6つの出力を持つESP-DLモデル

**モデルの6つの出力**:
```
1. /Unsqueeze_1_output_0  - 検出スコア（TopK出力）
2. /Mul_3_output_0         - クラススコア（ArgMaxの入力、Element 2の代替）
3. /Unsqueeze_3_output_0  - バウンディングボックス座標1
4. /Unsqueeze_4_output_0  - バウンディングボックス座標2
5. /Unsqueeze_5_output_0  - バウンディングボックス座標3
6. /Unsqueeze_6_output_0  - バウンディングボックス座標4
```

**ESP32での実装**:
```cpp
// モデル推論 - 6つの出力を取得
std::vector<Tensor<float>> outputs = model->forward(input);

// ArgMaxを実装：クラスIDを取得
Tensor<float> class_scores = outputs[1];  // 形状: [1, 100, 80]
Tensor<int> class_ids(100, 1);

for (int i = 0; i < 100; i++) {  // 100個の検出
    int max_class_id = 0;
    float max_score = class_scores.data[i * 80];
    
    for (int c = 1; c < 80; c++) {  // 80クラス
        if (class_scores.data[i * 80 + c] > max_score) {
            max_score = class_scores.data[i * 80 + c];
            max_class_id = c;
        }
    }
    class_ids.data[i] = max_class_id;
}

// 6要素を結合して最終検出結果を構築
std::vector<Detection> detections;
for (int i = 0; i < 100; i++) {
    Detection det;
    det.score = outputs[0].data[i];      // 検出スコア
    det.class_id = class_ids.data[i];    // クラスID（ArgMax結果）
    det.bbox.x1 = outputs[2].data[i];    // bbox座標
    det.bbox.y1 = outputs[3].data[i];
    det.bbox.x2 = outputs[4].data[i];
    det.bbox.y2 = outputs[5].data[i];
    
    if (det.score > threshold) {
        detections.push_back(det);
    }
}

// NMS等の後処理
detections = apply_nms(detections);
```

**ArgMax実装の複雑度**: O(n×c) = O(100×80) = 8,000回の比較（非常に軽量）

---

### 問題2: ESP-PPQとONNXの互換性問題

**エラー**: `TypeError: 'function' object is not subscriptable`

**原因**: 
- ONNX 1.16.0以降で`helper.tensor_dtype_to_np_dtype`が関数に変更
- ESP-PPQのコードは辞書としてアクセスしている
- `esp_ppq/parser/onnx_parser.py` line 116が原因

**対策**: ESP-PPQソースコードの修正
```python
# C:\Users\yamas\Documents\GitHub\esp-ppq-temp\esp_ppq\parser\onnx_parser.py
# Line 113-119を以下に変更:

if op.type == 'Cast' and key == 'to':
    # Support both dict-style and function-style access
    if callable(helper.tensor_dtype_to_np_dtype):
        value = helper.tensor_dtype_to_np_dtype(value)
    else:
        value = helper.tensor_dtype_to_np_dtype[value]
```

**推奨バージョン**:
```powershell
pip install onnx==1.16.0
```

---

### 問題3: dataloaderの型エラー

**エラー**: `TypeError: object of type 'generator' has no len()`

**原因**:
- ESP-PPQのキャリブレーション処理でdataloaderの長さが必要
- generatorはlen()をサポートしない

**対策**: `convert_to_espdl.py`でlist形式に変更
```python
def create_dummy_dataloader(input_shape, num_samples=32, batch_size=1):
    """Create dummy calibration data loader"""
    data = []
    for _ in range(num_samples):
        sample = torch.rand(input_shape)
        data.append([sample])  # list形式で返す
    return data  # generatorではなくlist
```

---

### 問題4: ESP-DLのインストールと環境構築

**推奨環境構築手順**:
```powershell
# 1. Conda環境の作成
conda create -n uhd-challenge python=3.10 -y
conda activate uhd-challenge

# 2. PyTorchのインストール（CPU版）
pip install torch torchvision --index-url https://download.pytorch.org/whl/cpu

# 3. ESP-PPQ最新版のインストール（GitHubから）
git clone https://github.com/espressif/esp-ppq.git C:\Users\yamas\Documents\GitHub\esp-ppq-temp
cd C:\Users\yamas\Documents\GitHub\esp-ppq-temp
pip install -e .

# 4. その他の依存関係
pip install onnx==1.16.0 onnxruntime

# 5. ESP-PPQの互換性修正（問題2の対策を適用）
# esp_ppq/parser/onnx_parser.pyを編集
```

---

## 演算子サポート状況

### ESP-DLでサポートされている演算子（UHDモデル内）
```
✅ 完全サポート（量子化対象）:
  Conv (32個), Mul (31個), Sigmoid (32個), Add (5個), Div (2個)
  GatherElements (4個), MaxPool (1個), ReduceMax (1個)
  Slice (1個), Softplus (2個), TopK (1個)

✅ サポート済み（量子化対象外）:
  Reshape (11個), Unsqueeze (6個), Gather (6個)
  Concat (2個), Transpose (1個)

❌ 非サポート（ESP32で実装が必要）:
  ArgMax (1個) - クラスID決定
```

**結論**: UHDモデルの138演算子中、137演算子（99.3%）がESP-DLでサポート。ArgMaxのみがESP32側での実装が必要。

---

## トラブルシューティング

### 変換が遅い
- キャリブレーションステップ数（デフォルト32）を減らす
- より小さいバリアント（N）を選択

### 複数出力モデルが正しく動作しない
1. `create_multi_output_model.py`で6つの出力が正しく設定されているか確認
2. ESP32側で6つ全ての出力を取得しているか確認
3. ArgMax実装が正しいか確認（クラススコアの形状に注意）

### ESP-DLライブラリでモデルをロードできない
- ESP-DLバージョンを確認（3.2.0以降推奨）
- `.espdl`ファイルが破損していないか確認
- ESP32のメモリ容量を確認（PSRAM必須）

### 参考リンク
- [ESP-DL Operator Support State](https://github.com/espressif/esp-dl/blob/master/operator_support_state.md)
- [ESP-PPQ GitHub](https://github.com/espressif/esp-ppq)
- [UHD GitHub](https://github.com/CheungBH/UHD)
