"""
Check operators in the _nopost.onnx models
"""
import onnx
import sys


def check_model_operators(model_path):
    """Check what operators are used in the model"""
    print("=" * 70)
    print(f"Analyzing: {model_path}")
    print("=" * 70)
    
    try:
        model = onnx.load(model_path)
    except Exception as e:
        print(f"Error loading model: {e}")
        return
    
    # Count operators
    ops = {}
    for node in model.graph.node:
        ops[node.op_type] = ops.get(node.op_type, 0) + 1
    
    print(f"\nTotal operator types: {len(ops)}")
    print(f"Total nodes: {len(model.graph.node)}")
    
    print("\nOperator list:")
    for op in sorted(ops.keys()):
        print(f"  {op}: {ops[op]} times")
    
    # Check for ArgMax
    has_argmax = 'ArgMax' in ops
    print(f"\n{'[CRITICAL]' if has_argmax else '[OK]'} ArgMax operator: {'FOUND' if has_argmax else 'NOT FOUND'}")
    
    # Model I/O info
    print(f"\nModel Input:")
    for inp in model.graph.input:
        shape = [d.dim_value if d.dim_value > 0 else '?' for d in inp.type.tensor_type.shape.dim]
        print(f"  Name: {inp.name}")
        print(f"  Shape: {shape}")
    
    print(f"\nModel Outputs: {len(model.graph.output)}")
    for i, out in enumerate(model.graph.output):
        shape = [d.dim_value if d.dim_value > 0 else '?' for d in out.type.tensor_type.shape.dim]
        print(f"  Output {i+1}: {out.name}")
        print(f"    Shape: {shape}")
    
    print("\n" + "=" * 70)
    return has_argmax


def main():
    models = [
        "model_conversion/models/ultratinyod_res_anc8_w64_64x64_quality_nopost.onnx",
        "model_conversion/models/ultratinyod_res_anc8_w96_64x64_quality_nopost.onnx",
    ]
    
    print("\n" + "=" * 70)
    print("CHECKING _NOPOST.ONNX MODELS FOR ESP-DL COMPATIBILITY")
    print("=" * 70)
    
    results = {}
    for model_path in models:
        has_argmax = check_model_operators(model_path)
        results[model_path] = has_argmax
        print("\n")
    
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    for model_path, has_argmax in results.items():
        model_name = model_path.split('/')[-1]
        status = "NEEDS WORKAROUND (has ArgMax)" if has_argmax else "READY FOR CONVERSION"
        print(f"{model_name}: {status}")
    
    print("\n" + "=" * 70)
    print("CONCLUSION")
    print("=" * 70)
    
    if all(not has_argmax for has_argmax in results.values()):
        print("All _nopost.onnx models are ready for direct conversion to ESP-DL!")
        print("No ArgMax workaround needed.")
    else:
        print("Some models still contain ArgMax operator.")
        print("Multi-output model approach is still required.")


if __name__ == "__main__":
    main()
