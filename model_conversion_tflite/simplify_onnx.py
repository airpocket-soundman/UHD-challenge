"""
Simplify ONNX model and convert to lower opset version
"""
import argparse
import onnx
from onnx import version_converter

def simplify_onnx(input_path, output_path, target_opset=13):
    """
    Simplify ONNX model and convert to target opset version
    
    Args:
        input_path: Input ONNX file
        output_path: Output ONNX file
        target_opset: Target opset version (default: 13)
    """
    print(f"Loading ONNX model: {input_path}")
    model = onnx.load(input_path)
    
    print(f"Original opset version: {model.opset_import[0].version}")
    
    # Convert to target opset
    print(f"Converting to opset {target_opset}...")
    converted_model = version_converter.convert_version(model, target_opset)
    
    print(f"Saving converted model: {output_path}")
    onnx.save(converted_model, output_path)
    
    print("✓ Conversion completed")
    print(f"  Input opset: {model.opset_import[0].version}")
    print(f"  Output opset: {converted_model.opset_import[0].version}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Simplify ONNX model")
    parser.add_argument("--input", required=True, help="Input ONNX file")
    parser.add_argument("--output", required=True, help="Output ONNX file")
    parser.add_argument("--opset", type=int, default=13, help="Target opset version")
    
    args = parser.parse_args()
    simplify_onnx(args.input, args.output, args.opset)
