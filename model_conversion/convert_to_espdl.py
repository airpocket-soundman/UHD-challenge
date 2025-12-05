"""
ONNX to ESP-DL format converter using ESP-PPQ
"""
import argparse
from pathlib import Path
import torch
import esp_ppq
from esp_ppq import QuantizationSettingFactory, TargetPlatform
from esp_ppq.api import quantize_onnx_model, export_ppq_graph


def create_dummy_dataloader(input_shape, num_samples=32, batch_size=1):
    """Create dummy calibration data loader"""
    print(f"  Creating dummy calibration data: {num_samples} samples...")
    
    # Generate random calibration data as a list
    # ESP-PPQ needs a list, not a generator
    data = []
    for _ in range(num_samples):
        # Random data in range [0, 1]
        sample = torch.rand(input_shape)
        data.append([sample])  # Wrap in list for PPQ format
    
    return data


def convert_onnx_to_espdl(
    onnx_path: str,
    output_path: str,
    input_shape: tuple = (1, 3, 64, 64),
    calibration_data=None
):
    """
    Convert ONNX model to ESP-DL format (.espdl)
    
    Args:
        onnx_path: Path to input ONNX file
        output_path: Path for output .espdl file (without extension)
        input_shape: Model input shape (batch, channels, height, width)
        calibration_data: Optional calibration data for quantization
    """
    print(f"\n{'='*60}")
    print(f"ONNX to ESP-DL Converter")
    print(f"{'='*60}\n")
    
    print(f"Input ONNX: {onnx_path}")
    print(f"Output path: {output_path}")
    print(f"Input shape: {input_shape}\n")
    
    # Create quantization setting for ESP platform
    print("Step 1: Creating quantization settings...")
    setting = QuantizationSettingFactory.espdl_setting()
    
    # Create calibration data if not provided
    if calibration_data is None:
        print("Step 2: Creating calibration data...")
        calibration_data = create_dummy_dataloader(input_shape, num_samples=32)
        calib_steps = 32
    else:
        calib_steps = 32
    
    # Quantize the model
    print("Step 3: Quantizing model...")
    try:
        quantized = quantize_onnx_model(
            onnx_import_file=onnx_path,
            calib_dataloader=calibration_data,
            calib_steps=calib_steps,
            input_shape=input_shape,
            setting=setting,
            platform=TargetPlatform.ESPDL_INT8,
            device='cpu'
        )
        print("✓ Quantization completed")
    except Exception as e:
        print(f"✗ Quantization failed: {e}")
        raise
    
    # Export to ESP-DL format
    print("Step 4: Exporting to ESP-DL format...")
    try:
        export_ppq_graph(
            graph=quantized,
            platform=TargetPlatform.ESPDL_INT8,
            graph_save_to=output_path
        )
        print(f"✓ Export completed: {output_path}.espdl")
    except Exception as e:
        print(f"✗ Export failed: {e}")
        raise
    
    print(f"\n{'='*60}")
    print("Conversion successful!")
    print(f"{'='*60}\n")
    
    # Check output file
    espdl_file = Path(f"{output_path}.espdl")
    if espdl_file.exists():
        size_mb = espdl_file.stat().st_size / (1024 * 1024)
        print(f"Output file: {espdl_file}")
        print(f"File size: {size_mb:.2f} MB")
        return str(espdl_file)
    else:
        print("Warning: Output file not found")
        return None


def main():
    parser = argparse.ArgumentParser(
        description="Convert ONNX model to ESP-DL format (.espdl)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Example usage:
  # Convert UHD N model
  python convert_to_espdl.py \\
    --model model_conversion/onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx \\
    --output model_conversion/uhd_n_w64 \\
    --input-shape 1,3,64,64
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
        help="Output path (without .espdl extension)"
    )
    parser.add_argument(
        "--input-shape",
        default="1,3,64,64",
        type=str,
        help="Input shape as N,C,H,W (default: 1,3,64,64 for UHD)"
    )
    
    args = parser.parse_args()
    
    # Parse input shape
    try:
        input_shape = tuple(map(int, args.input_shape.split(',')))
        if len(input_shape) != 4:
            raise ValueError("Input shape must have 4 dimensions (N,C,H,W)")
    except ValueError as e:
        print(f"Error parsing input shape: {e}")
        return 1
    
    # Check input file exists
    if not Path(args.model).exists():
        print(f"Error: Input file not found: {args.model}")
        return 1
    
    # Convert
    try:
        output_file = convert_onnx_to_espdl(
            onnx_path=args.model,
            output_path=args.output,
            input_shape=input_shape
        )
        return 0 if output_file else 1
    except Exception as e:
        print(f"\nConversion failed: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
