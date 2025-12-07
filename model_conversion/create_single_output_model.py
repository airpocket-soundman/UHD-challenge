"""
Create a single-output model from _nopost.onnx that only outputs 'pred'.
The 'anchors' and 'wh_scale' are constants that can be extracted separately.
"""
import onnx
import numpy as np
import argparse


def extract_constants(model):
    """Extract anchors and wh_scale constants from the model"""
    constants = {}
    
    # Find anchors and wh_scale in initializers
    for init in model.graph.initializer:
        if init.name == 'anchors':
            # Convert to numpy array
            anchors = onnx.numpy_helper.to_array(init)
            constants['anchors'] = anchors
            print(f"Found anchors: shape={anchors.shape}")
            print(anchors)
        elif init.name == 'wh_scale':
            wh_scale = onnx.numpy_helper.to_array(init)
            constants['wh_scale'] = wh_scale
            print(f"Found wh_scale: shape={wh_scale.shape}")
            print(wh_scale)
    
    return constants


def create_single_output_model(input_model, output_model, output_constants=None):
    """
    Create a single-output model that only outputs 'pred'.
    Save anchors and wh_scale constants to a separate file.
    """
    print("=" * 70)
    print(f"Loading model: {input_model}")
    model = onnx.load(input_model)
    
    print(f"\nOriginal outputs: {len(model.graph.output)}")
    for i, out in enumerate(model.graph.output):
        print(f"  {i+1}. {out.name}")
    
    # Extract constants before modifying
    print("\n" + "=" * 70)
    print("Extracting constants...")
    print("=" * 70)
    constants = extract_constants(model)
    
    # Save constants to numpy file if requested
    if output_constants:
        np.savez(output_constants, **constants)
        print(f"\n✓ Constants saved to: {output_constants}")
    
    # Keep only the 'pred' output
    new_outputs = []
    for out in model.graph.output:
        if out.name == 'pred':
            new_outputs.append(out)
            print(f"\n✓ Keeping output: {out.name}")
    
    if len(new_outputs) == 0:
        raise ValueError("'pred' output not found in model!")
    
    # Clear and set new outputs
    del model.graph.output[:]
    model.graph.output.extend(new_outputs)
    
    # Validate model
    try:
        onnx.checker.check_model(model)
        print("✓ Model validation passed")
    except Exception as e:
        print(f"⚠ Model validation warning: {e}")
        print("  Proceeding anyway...")
    
    # Save model
    onnx.save(model, output_model)
    print(f"\n✓ Single-output model saved: {output_model}")
    
    print("\n" + "=" * 70)
    print("RESULT")
    print("=" * 70)
    print(f"""
Single-output model created successfully!

Model output:
  - pred: [1, 56, 8, 8] - Raw predictions

Constants extracted:
  - anchors: {constants['anchors'].shape if 'anchors' in constants else 'Not found'}
  - wh_scale: {constants['wh_scale'].shape if 'wh_scale' in constants else 'Not found'}

Next steps:
1. Convert the single-output model to ESP-DL format:
   python model_conversion/convert_to_espdl.py \\
     --model {output_model} \\
     --output model_conversion/esp_dl/uhd_n_w64_nopost \\
     --input-shape "1,3,64,64"

2. Use the extracted constants in your ESP32 code:
   - Load anchors and wh_scale from {output_constants}
   - Or hardcode them in your C++ code
""")
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Create single-output model for ESP-DL conversion"
    )
    parser.add_argument(
        "--input",
        required=True,
        help="Input ONNX model path (_nopost.onnx)"
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Output ONNX model path (single output)"
    )
    parser.add_argument(
        "--constants",
        default=None,
        help="Output path for constants (.npz file)"
    )
    
    args = parser.parse_args()
    
    success = create_single_output_model(
        args.input, 
        args.output,
        args.constants
    )
    return 0 if success else 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
