"""
ONNXモデルの入出力構造を確認するユーティリティ
"""
import onnx
from onnx import TensorProto


def _shape_to_list(value_info):
    return [d.dim_value if d.dim_value > 0 else "?" for d in value_info.type.tensor_type.shape.dim]


def _build_producer_map(model):
    """Map tensor name -> producer summary (initializer or op type/name)."""
    producers = {}
    for init in model.graph.initializer:
        producers[init.name] = "initializer"
    for node in model.graph.node:
        for out in node.output:
            if out in producers:
                # initializer is still the true source; keep existing
                continue
            node_name = node.name or "(no_name)"
            producers[out] = f"{node.op_type} : {node_name}"
    return producers


def analyze_model_structure(model_path: str) -> None:
    """指定したONNXモデルの入力・出力・オペレーター統計を表示する"""
    print("=" * 80)
    print(f"モデル解析: {model_path}")
    print("=" * 80)

    try:
        model = onnx.load(model_path)
    except Exception as e:  # noqa: BLE001 - ユーザー向けツールなので広く捕捉
        print(f"エラー: モデルの読み込みに失敗しました: {e}")
        return

    producers = _build_producer_map(model)

    # 入力情報
    print("\n【入力情報】")
    print("-" * 80)
    for i, inp in enumerate(model.graph.input):
        shape = _shape_to_list(inp)
        dtype = TensorProto.DataType.Name(inp.type.tensor_type.elem_type)
        print(f"入力{i + 1}:")
        print(f"  名前: {inp.name}")
        print(f"  形状: {shape}")
        print(f"  データ型: {dtype}")
        if len(shape) == 4:
            print("  形状の意味: [バッチ, チャンネル, 高さ, 幅]")
            print(f"    - バッチサイズ: {shape[0]}")
            print(f"    - チャンネル数: {shape[1]}")
            print(f"    - 画像サイズ: {shape[2]}x{shape[3]}")

    # 出力情報
    print("\n【出力情報】")
    print("-" * 80)
    print(f"出力数: {len(model.graph.output)}")
    for i, out in enumerate(model.graph.output):
        shape = _shape_to_list(out)
        dtype = TensorProto.DataType.Name(out.type.tensor_type.elem_type)
        print(f"\n出力{i + 1}:")
        print(f"  名前: {out.name}")
        print(f"  形状: {shape}")
        print(f"  データ型: {dtype}")
        print(f"  生成元: {producers.get(out.name, 'unknown')}")

        if len(shape) == 3:
            print("  形状の意味: [バッチ, アンカー数, 特徴数]")
            print(f"    - バッチサイズ: {shape[0]}")
            print(f"    - アンカー数: {shape[1]}")
            print(f"    - 特徴数: {shape[2]}")

    # オペレーター統計
    print("\n【オペレーター統計】")
    print("-" * 80)
    ops = {}
    for node in model.graph.node:
        ops[node.op_type] = ops.get(node.op_type, 0) + 1

    print(f"オペレーター種類数: {len(ops)}")
    print(f"総ノード数: {len(model.graph.node)}")
    print("\nオペレーター一覧:")
    for op in sorted(ops.keys()):
        print(f"  {op}: {ops[op]} 回")

    # ESP-DL互換性チェック（代表的な非対応演算のみ簡易チェック）
    print("\n【ESP-DL互換性チェック（簡易）】")
    print("-" * 80)
    unsupported_ops = ["ArgMax", "TopK", "NonMaxSuppression"]
    found_unsupported = [op for op in unsupported_ops if op in ops]

    if found_unsupported:
        print(f"⚠ 非対応または注意が必要なオペレーター: {', '.join(found_unsupported)}")
    else:
        print("OK: 代表的な非対応オペレーターは見つかりませんでした")

    print("\n" + "=" * 80)


def main():
    models = [
        "w_ESE+IoU-aware+ReLU/translated/uhd_relu_w64_single.onnx",
        "w_ESE+IoU-aware+ReLU/ultratinyod_res_anc8_w64_64x64_quality_relu_nopost.onnx",
    ]

    for model_path in models:
        try:
            analyze_model_structure(model_path)
            print("\n\n")
        except Exception as e:  # noqa: BLE001 - 実行を止めたくないため広く捕捉
            print(f"エラー: {model_path} の解析に失敗しました")
            print(f"詳細: {e}\n\n")


if __name__ == "__main__":
    main()
