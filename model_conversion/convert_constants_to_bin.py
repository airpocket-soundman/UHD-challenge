"""
Convert .npz constants file to .bin format for easy loading on ESP32
"""
import numpy as np
import argparse
from pathlib import Path


def convert_npz_to_bin(npz_path, bin_path=None):
    """
    Convert .npz constants file to .bin format
    
    Args:
        npz_path: Path to input .npz file
        bin_path: Path to output .bin file (optional, auto-generated if None)
    """
    print("=" * 70)
    print("NPZ to BIN Converter")
    print("=" * 70)
    
    # Load .npz file
    print(f"\nLoading: {npz_path}")
    data = np.load(npz_path)
    
    # Extract arrays
    anchors = data['anchors'].astype(np.float32)
    wh_scale = data['wh_scale'].astype(np.float32)
    
    print(f"\nAnchors shape: {anchors.shape}")
    print(f"WH Scale shape: {wh_scale.shape}")
    
    # Display values
    print("\nAnchors:")
    print(anchors)
    print("\nWH Scale:")
    print(wh_scale)
    
    # Generate output path if not provided
    if bin_path is None:
        bin_path = str(Path(npz_path).with_suffix('.bin'))
    
    # Save to .bin file
    print(f"\nSaving to: {bin_path}")
    with open(bin_path, 'wb') as f:
        anchors.tofile(f)
        wh_scale.tofile(f)
    
    # Verify file size
    file_size = Path(bin_path).stat().st_size
    expected_size = (8 * 2 + 8 * 2) * 4  # (anchors + wh_scale) * sizeof(float)
    
    print(f"\n✓ Binary file created:")
    print(f"  File size: {file_size} bytes")
    print(f"  Expected size: {expected_size} bytes")
    
    if file_size == expected_size:
        print("  ✓ Size verification passed")
    else:
        print(f"  ⚠ Warning: Size mismatch!")
    
    print("\n" + "=" * 70)
    print("ESP32 C++ Loading Code:")
    print("=" * 70)
    print(f"""
struct ModelConstants {{
    float anchors[8][2];
    float wh_scale[8][2];
}};

ModelConstants load_constants(const char* path) {{
    ModelConstants consts;
    FILE* f = fopen(path, "rb");
    if (f) {{
        fread(consts.anchors, sizeof(float), 8 * 2, f);
        fread(consts.wh_scale, sizeof(float), 8 * 2, f);
        fclose(f);
        ESP_LOGI(TAG, "Loaded constants from %s", path);
    }} else {{
        ESP_LOGE(TAG, "Failed to open %s", path);
    }}
    return consts;
}}

// Usage:
ModelConstants consts = load_constants("/sdcard/{Path(bin_path).name}");
""")
    
    return bin_path


def main():
    parser = argparse.ArgumentParser(
        description="Convert .npz constants to .bin format for ESP32"
    )
    parser.add_argument(
        "--input",
        required=True,
        help="Input .npz file path"
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output .bin file path (optional, auto-generated if not provided)"
    )
    
    args = parser.parse_args()
    
    # Check input file exists
    if not Path(args.input).exists():
        print(f"Error: Input file not found: {args.input}")
        return 1
    
    # Convert
    try:
        output_path = convert_npz_to_bin(args.input, args.output)
        print(f"\n✓ Conversion complete: {output_path}")
        return 0
    except Exception as e:
        print(f"\n✗ Conversion failed: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
