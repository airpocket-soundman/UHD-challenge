import onnx
import os

# カレントディレクトリを取得
script_dir = os.path.dirname(os.path.abspath(__file__))
model_path = os.path.join(script_dir, 'onnx', 'ultratinyod_res_anc8_w64_64x64_quality.onnx')

# オリジナルモデルのオペレータ解析
print("=" * 60)
print("オリジナルONNXモデルのオペレータリスト")
print("=" * 60)
model = onnx.load(model_path)
ops = set()
for node in model.graph.node:
    ops.add(node.op_type)

print(f"総オペレータ数: {len(ops)}")
print("\nオペレータ一覧:")
for op in sorted(ops):
    # 各オペレータの使用回数を数える
    count = sum(1 for node in model.graph.node if node.op_type == op)
    print(f"  - {op}: {count}回")

print("\n" + "=" * 60)
print("変換後モデル (multi版) のオペレータリスト")
print("=" * 60)

# ESP-DLモデルはバイナリなので解析できないため、
# 変換プロセスから推測
print("注: ESP-DLモデルはバイナリ形式のため、直接解析できません")
print("変換により以下のオペレータがサポートされます:")
print("\nESP-DL対応オペレータ:")
print("  - Conv2D (畳み込み)")
print("  - DepthwiseConv2D (深さ方向畳み込み)")
print("  - ReLU/ReLU6 (活性化関数)")
print("  - Add (残差接続)")
print("  - Concat (結合)")
print("  - Reshape/Flatten (形状変換)")
print("  - Softmax (正規化)")
print("  - その他の基本演算")

print("\n" + "=" * 60)
print("モデル詳細情報")
print("=" * 60)
print(f"入力名: {model.graph.input[0].name}")
print(f"入力shape: {[d.dim_value for d in model.graph.input[0].type.tensor_type.shape.dim]}")
print(f"\n出力数: {len(model.graph.output)}")
for i, output in enumerate(model.graph.output):
    print(f"出力{i+1}: {output.name}")
    print(f"  shape: {[d.dim_value for d in output.type.tensor_type.shape.dim]}")
