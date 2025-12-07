"""
Batch convert all UHD _nopost.onnx models to ESP-DL format
"""
import os
import subprocess
import sys
from pathlib import Path


def get_variant_info(model_name):
    """Extract variant information from model name"""
    variants = {
        'w64': {'name': 'N', 'params': '1.38M'},
        'w96': {'name': 'T', 'params': '3.1M'},
        'w128': {'name': 'S', 'params': '5.5M'},
        'w160': {'name': 'M', 'params': '8.7M'},
        'w192': {'name': 'L', 'params': '12.6M'},
        'w256': {'name': 'H', 'params': '22.4M'},
    }
    
    for key, info in variants.items():
        if key in model_name:
            return info
    return {'name': 'Unknown', 'params': 'Unknown'}


def convert_model(input_model, output_dir):
    """Convert a single model to ESP-DL format"""
    model_path = Path(input_model)
    model_name = model_path.stem  # e.g., ultratinyod_res_anc8_w64_64x64_quality_nopost
    
    # Extract variant info
    variant = get_variant_info(model_name)
    
    print("\n" + "=" * 80)
    print(f"Converting: {model_path.name}")
    print(f"Variant: {variant['name']} ({variant['params']} params)")
    print("=" * 80)
    
    # Step 1: Create single-output model
    single_model = model_path.parent / f"{model_name}_single.onnx"
    constants_file = output_dir / f"{model_name}_constants.npz"
    
    print(f"\nStep 1: Creating single-output model...")
    
    # Import and call directly instead of using subprocess
    sys.path.insert(0, str(Path.cwd()))
    from model_conversion.create_single_output_model import create_single_output_model as create_single
    
    try:
        success = create_single(str(input_model), str(single_model), str(constants_file))
        if not success:
            print(f"ERROR: Failed to create single-output model")
            return False
    except Exception as e:
        print(f"ERROR: Failed to create single-output model: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    print(f"✓ Single-output model created: {single_model.name}")
    print(f"✓ Constants saved: {constants_file.name}")
    
    # Step 2: Convert to ESP-DL
    output_base = output_dir / model_name
    
    print(f"\nStep 2: Converting to ESP-DL format...")
    
    # Import and call directly
    from model_conversion.convert_to_espdl import convert_onnx_to_espdl
    
    try:
        result = convert_onnx_to_espdl(
            onnx_path=str(single_model),
            output_path=str(output_base),
            input_shape=(1, 3, 64, 64)
        )
        if not result:
            print(f"ERROR: Failed to convert to ESP-DL")
            return False
    except Exception as e:
        print(f"ERROR: Failed to convert to ESP-DL: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    print(f"✓ ESP-DL model created: {output_base.name}")
    
    # Check file sizes
    espdl_file = Path(f"{output_base}")
    if espdl_file.exists():
        size_mb = espdl_file.stat().st_size / (1024 * 1024)
        print(f"✓ Model size: {size_mb:.2f} MB")
    
    print(f"\n✓✓✓ Conversion completed successfully!")
    return True


def main():
    """Main batch conversion function"""
    print("=" * 80)
    print("BATCH CONVERSION: All UHD Models to ESP-DL Format")
    print("=" * 80)
    
    # Setup directories
    models_dir = Path("model_conversion/models")
    output_dir = Path("model_conversion/transrated_models")
    output_dir.mkdir(exist_ok=True)
    
    # Find all _nopost.onnx models (exclude _single.onnx)
    models = sorted([
        f for f in models_dir.glob("*_nopost.onnx")
        if "_single" not in f.name
    ])
    
    if not models:
        print("\nNo models found to convert!")
        return 1
    
    print(f"\nFound {len(models)} models to convert:")
    for i, model in enumerate(models, 1):
        variant = get_variant_info(model.name)
        print(f"  {i}. {model.name} - Variant {variant['name']} ({variant['params']})")
    
    # Convert each model
    results = {}
    successful = 0
    failed = 0
    
    for model in models:
        try:
            success = convert_model(model, output_dir)
            results[model.name] = "SUCCESS" if success else "FAILED"
            if success:
                successful += 1
            else:
                failed += 1
        except Exception as e:
            print(f"\nERROR: Exception during conversion: {e}")
            results[model.name] = f"ERROR: {e}"
            failed += 1
    
    # Summary
    print("\n" + "=" * 80)
    print("CONVERSION SUMMARY")
    print("=" * 80)
    
    for model_name, status in results.items():
        status_icon = "✓" if status == "SUCCESS" else "✗"
        print(f"{status_icon} {model_name}: {status}")
    
    print(f"\n" + "=" * 80)
    print(f"Total: {len(models)} models")
    print(f"Successful: {successful}")
    print(f"Failed: {failed}")
    print("=" * 80)
    
    if failed == 0:
        print("\n🎉 All models converted successfully!")
        print(f"\nOutput directory: {output_dir}")
        print("\nGenerated files for each model:")
        print("  - <model_name> - ESP-DL model file")
        print("  - <model_name>.json - Model metadata")
        print("  - <model_name>.info - Conversion information")
        print("  - <model_name>_constants.npz - Anchors and scales")
        return 0
    else:
        print(f"\n⚠️  {failed} model(s) failed to convert.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
