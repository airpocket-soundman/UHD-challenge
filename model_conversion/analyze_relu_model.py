"""
Analyze ReLU version of _nopost.onnx model
Check output format and ESP-DL compatibility
"""
import onnx
import numpy as np
from onnx import numpy_helper


def analyze_model(model_path):
    """Analyze ONNX model structure and outputs"""
    
    print("=" * 70)
    print(f"ANALYZING: {model_path}")
    print("=" * 70)
    
    model = onnx.load(model_path)
    
    # Model inputs
    print("\n[INPUTS]")
    for inp in model.graph.input:
        shape = [d.dim_value if d.dim_value > 0 else 'dynamic' for d in inp.type.tensor_type.shape.dim]
        dtype = onnx.TensorProto.DataType.Name(inp.type.tensor_type.elem_type)
        print(f"  {inp.name}: {shape} ({dtype})")
    
    # Model outputs
    print("\n[OUTPUTS]")
    for idx, out in enumerate(model.graph.output):
        shape = [d.dim_value if d.dim_value > 0 else 'dynamic' for d in out.type.tensor_type.shape.dim]
        dtype = onnx.TensorProto.DataType.Name(out.type.tensor_type.elem_type)
        print(f"  {idx+1}. {out.name}: {shape} ({dtype})")
    
    # Initializers (constants like anchors, wh_scale)
    print("\n[INITIALIZERS / CONSTANTS]")
    constants = {}
    for init in model.graph.initializer:
        arr = numpy_helper.to_array(init)
        constants[init.name] = arr
        print(f"  {init.name}: shape={arr.shape}, dtype={arr.dtype}")
        if arr.size <= 20:  # Show small arrays
            print(f"    Values: {arr.flatten()}")
        if init.name in ['anchors', 'wh_scale']:
            print(f"    {init.name}:")
            print(arr)
    
    # Operators
    print("\n[OPERATORS]")
    ops = {}
    for node in model.graph.node:
        ops[node.op_type] = ops.get(node.op_type, 0) + 1
    
    print(f"  Total operator types: {len(ops)}")
    print(f"  Total nodes: {len(model.graph.node)}")
    print("\n  Operator counts:")
    for op in sorted(ops.keys()):
        print(f"    {op:20s}: {ops[op]:3d}")
    
    return model, constants, ops


def check_espdl_support(ops):
    """Check if all operators are supported by ESP-DL"""
    
    # ESP-DL supported operators
    esp_dl_supported = {
        'Conv', 'Add', 'Mul', 'Sigmoid', 'MaxPool', 'Relu',
        'Reshape', 'Concat', 'Transpose', 'Div', 'Sub', 'Softmax',
        'ReduceMean'  # Added - commonly supported reduction operation
    }
    
    # Known unsupported
    esp_dl_unsupported = {
        'ArgMax', 'TopK', 'GatherElements'
    }
    
    print("\n" + "=" * 70)
    print("ESP-DL COMPATIBILITY CHECK")
    print("=" * 70)
    
    all_supported = True
    for op in sorted(ops.keys()):
        count = ops[op]
        if op in esp_dl_supported:
            status = "✓ Supported"
        elif op in esp_dl_unsupported:
            status = "✗ NOT Supported"
            all_supported = False
        else:
            status = "? Unknown (needs verification)"
            all_supported = False
        
        print(f"  {op:20s}: {count:3d} times - {status}")
    
    print("\n" + "=" * 70)
    if all_supported:
        print("✓✓✓ ALL OPERATORS ARE SUPPORTED BY ESP-DL! ✓✓✓")
        print("\nThis model can be converted to ESP-DL format.")
    else:
        print("✗ SOME OPERATORS MAY NOT BE SUPPORTED")
        print("\nPlease verify unsupported operators before conversion.")
    
    return all_supported


def main():
    model_path = "model_conversion/w_ESE+IoU-aware+ReLU/ultratinyod_res_anc8_w64_64x64_quality_relu_nopost.onnx"
    
    model, constants, ops = analyze_model(model_path)
    is_supported = check_espdl_support(ops)
    
    print("\n" + "=" * 70)
    print("SUMMARY")
    print("=" * 70)
    
    # Check if this has the same structure as the previous _nopost model
    num_outputs = len(model.graph.output)
    print(f"\nNumber of outputs: {num_outputs}")
    
    if num_outputs == 3:
        print("✓ This is a 3-output model (pred, anchors, wh_scale)")
        print("\nNext steps:")
        print("1. Use create_single_output_model.py to extract pred only")
        print("2. Use convert_to_espdl.py to convert to ESP-DL format")
    elif num_outputs == 1:
        print("✓ This is a single-output model")
        print("\nNext steps:")
        print("1. Use convert_to_espdl.py directly to convert to ESP-DL format")
    else:
        print(f"⚠ Unexpected number of outputs: {num_outputs}")
    
    if 'anchors' in constants and 'wh_scale' in constants:
        print("\n✓ Model contains anchors and wh_scale constants")
    else:
        print("\n⚠ Anchors or wh_scale not found in model")


if __name__ == "__main__":
    main()
