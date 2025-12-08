"""
Convert .npz constants file to JSON format for ESP32
"""
import numpy as np
import json
import argparse
from pathlib import Path


def convert_npz_to_json(npz_path, json_path=None):
    """
    Convert .npz constants file to JSON format
    
    Args:
        npz_path: Path to input .npz file
        json_path: Path to output JSON file (optional, auto-generated if None)
    """
    print("=" * 70)
    print("NPZ to JSON Converter")
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
    if json_path is None:
        json_path = str(Path(npz_path).with_suffix('.json'))
        # Remove '_constants' from filename if present
        json_path = json_path.replace('_constants.json', '_model_constants.json')
    
    # Create JSON structure
    json_data = {
        "anchors": anchors.flatten().tolist(),
        "wh_scale": wh_scale.flatten().tolist(),
        "metadata": {
            "num_anchors": int(anchors.shape[0]),
            "anchor_dims": int(anchors.shape[1]),
            "description": "Model constants for UHD detection model"
        }
    }
    
    # Save to JSON file
    print(f"\nSaving to: {json_path}")
    with open(json_path, 'w') as f:
        json.dump(json_data, f, indent=4)
    
    # Verify file size
    file_size = Path(json_path).stat().st_size
    
    print(f"\n✓ JSON file created:")
    print(f"  File size: {file_size} bytes")
    print(f"  Anchors: {len(json_data['anchors'])} values")
    print(f"  WH Scale: {len(json_data['wh_scale'])} values")
    
    print("\n" + "=" * 70)
    print("ESP32 Arduino Code Example:")
    print("=" * 70)
    print(f"""
// Load from SD card
File f = SD.open("/uhd_relu_w64_model_constants.json");
String json = f.readString();
f.close();

// Parse anchors
int anchors_pos = json.indexOf("\\"anchors\\"");
int array_start = json.indexOf("[", anchors_pos);
int array_end = json.indexOf("]", array_start);
String anchors_str = json.substring(array_start + 1, array_end);

// Parse values (simplified)
float anchors[8][2];
// ... parse comma-separated values ...
""")
    
    return json_path


def main():
    parser = argparse.ArgumentParser(
        description="Convert .npz constants to JSON format for ESP32"
    )
    parser.add_argument(
        "--input",
        required=True,
        help="Input .npz file path"
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output JSON file path (optional, auto-generated if not provided)"
    )
    
    args = parser.parse_args()
    
    # Check input file exists
    if not Path(args.input).exists():
        print(f"Error: Input file not found: {args.input}")
        return 1
    
    # Convert
    try:
        output_path = convert_npz_to_json(args.input, args.output)
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
