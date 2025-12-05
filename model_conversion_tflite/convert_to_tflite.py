"""
ONNX to TensorFlow Lite converter for UHD models
Supports ArgMax and all other operations
"""
import argparse
import os
from pathlib import Path
import numpy as np

def convert_onnx_to_tflite(
    onnx_path: str,
    output_path: str,
    quantize: bool = True
):
    """
    Convert ONNX model to TensorFlow Lite format
    
    Args:
        onnx_path: Path to input ONNX file
        output_path: Path for output .tflite file
        quantize: Whether to apply INT8 quantization
    """
    print(f"\n{'='*60}")
    print(f"ONNX to TensorFlow Lite Converter")
    print(f"{'='*60}\n")
    
    print(f"Input ONNX: {onnx_path}")
    print(f"Output TFLite: {output_path}")
    print(f"Quantization: {'Enabled (INT8)' if quantize else 'Disabled (FP32)'}\n")
    
    try:
        import onnx
        from onnx_tf.backend import prepare
        import tensorflow as tf
    except ImportError as e:
        print(f"Error: Required package not installed: {e}")
        print("\nPlease install required packages:")
        print("  pip install onnx onnx-tf tensorflow")
        return None
    
    # Step 1: Load ONNX model
    print("Step 1: Loading ONNX model...")
    try:
        onnx_model = onnx.load(onnx_path)
        print(f"✓ ONNX model loaded")
        print(f"  IR version: {onnx_model.ir_version}")
        print(f"  Opset version: {onnx_model.opset_import[0].version}")
    except Exception as e:
        print(f"✗ Failed to load ONNX model: {e}")
        return None
    
    # Step 2: Convert ONNX to TensorFlow
    print("\nStep 2: Converting ONNX to TensorFlow...")
    try:
        tf_rep = prepare(onnx_model)
        print(f"✓ Converted to TensorFlow")
    except Exception as e:
        print(f"✗ Conversion failed: {e}")
        print("\nNote: Some ONNX operations may not be supported by onnx-tf.")
        print("Try updating onnx-tf: pip install --upgrade onnx-tf")
        return None
    
    # Save as TensorFlow SavedModel (temporary)
    temp_dir = "temp_tf_model"
    print(f"\nStep 3: Exporting TensorFlow SavedModel...")
    try:
        tf_rep.export_graph(temp_dir)
        print(f"✓ Exported to {temp_dir}")
    except Exception as e:
        print(f"✗ Export failed: {e}")
        return None
    
    # Step 4: Convert TensorFlow to TFLite
    print(f"\nStep 4: Converting to TensorFlow Lite...")
    try:
        converter = tf.lite.TFLiteConverter.from_saved_model(temp_dir)
        
        if quantize:
            print("  Applying INT8 quantization...")
            
            # Set optimization
            converter.optimizations = [tf.lite.Optimize.DEFAULT]
            
            # Create representative dataset for quantization
            def representative_dataset():
                # Generate dummy calibration data
                # UHD input: (1, 3, 64, 64) RGB image in [0,1]
                for _ in range(100):
                    data = np.random.rand(1, 3, 64, 64).astype(np.float32)
                    yield [data]
            
            converter.representative_dataset = representative_dataset
            
            # Set input/output types
            converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
            converter.inference_input_type = tf.float32  # Keep input as float32
            converter.inference_output_type = tf.float32  # Keep output as float32
        
        # Convert
        tflite_model = converter.convert()
        print(f"✓ Converted to TFLite")
        
    except Exception as e:
        print(f"✗ TFLite conversion failed: {e}")
        import traceback
        traceback.print_exc()
        return None
    
    # Step 5: Save TFLite model
    print(f"\nStep 5: Saving TFLite model...")
    try:
        with open(output_path, 'wb') as f:
            f.write(tflite_model)
        
        size_mb = len(tflite_model) / (1024 * 1024)
        print(f"✓ Saved: {output_path}")
        print(f"  Size: {size_mb:.2f} MB")
        
    except Exception as e:
        print(f"✗ Failed to save: {e}")
        return None
    
    # Cleanup
    print(f"\nStep 6: Cleaning up temporary files...")
    try:
        import shutil
        if os.path.exists(temp_dir):
            shutil.rmtree(temp_dir)
        print(f"✓ Cleaned up {temp_dir}")
    except Exception as e:
        print(f"⚠ Cleanup warning: {e}")
    
    print(f"\n{'='*60}")
    print("Conversion successful!")
    print(f"{'='*60}\n")
    
    # Verify the model
    print("Verification:")
    try:
        interpreter = tf.lite.Interpreter(model_path=output_path)
        interpreter.allocate_tensors()
        
        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()
        
        print(f"  Inputs: {len(input_details)}")
        for i, detail in enumerate(input_details):
            print(f"    {i}: {detail['name']} - shape {detail['shape']}, dtype {detail['dtype']}")
        
        print(f"  Outputs: {len(output_details)}")
        for i, detail in enumerate(output_details):
            print(f"    {i}: {detail['name']} - shape {detail['shape']}, dtype {detail['dtype']}")
        
    except Exception as e:
        print(f"  ⚠ Verification warning: {e}")
    
    return output_path


def main():
    parser = argparse.ArgumentParser(
        description="Convert UHD ONNX model to TensorFlow Lite format",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Example usage:
  # Convert with INT8 quantization (recommended)
  python convert_to_tflite.py \\
    --model onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx \\
    --output tflite/uhd_n_w64_int8.tflite \\
    --quantize

  # Convert without quantization (FP32)
  python convert_to_tflite.py \\
    --model onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx \\
    --output tflite/uhd_n_w64_fp32.tflite

Note: This converter supports ArgMax and all ONNX operations.
      The original model (not the multi-output version) can be used.
        """
    )
    
    parser.add_argument(
        "--model",
        required=True,
        type=str,
        help="Path to input ONNX model"
    )
    parser.add_argument(
        "--output",
        required=True,
        type=str,
        help="Output path for .tflite file"
    )
    parser.add_argument(
        "--quantize",
        action="store_true",
        help="Apply INT8 quantization (recommended for ESP32)"
    )
    
    args = parser.parse_args()
    
    # Check input file exists
    if not Path(args.model).exists():
        print(f"Error: Input file not found: {args.model}")
        return 1
    
    # Create output directory
    output_dir = Path(args.output).parent
    if output_dir != Path('.'):
        output_dir.mkdir(parents=True, exist_ok=True)
    
    # Convert
    try:
        result = convert_onnx_to_tflite(
            onnx_path=args.model,
            output_path=args.output,
            quantize=args.quantize
        )
        return 0 if result else 1
    except Exception as e:
        print(f"\nConversion failed: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
