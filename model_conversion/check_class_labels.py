"""
Check class labels in ONNX model metadata
"""
import onnx
import json

model_path = "onnx/ultratinyod_res_anc8_w64_64x64_quality_multi_output.onnx"

print(f"Loading ONNX model: {model_path}")
model = onnx.load(model_path)

print("\n" + "="*60)
print("MODEL METADATA")
print("="*60)

# Check metadata properties
if model.metadata_props:
    print("\nMetadata Properties:")
    for prop in model.metadata_props:
        print(f"  {prop.key}: {prop.value}")
else:
    print("\nNo metadata properties found")

# Check doc string
if model.doc_string:
    print("\nDoc String:")
    print(f"  {model.doc_string}")
else:
    print("\nNo doc string found")

# Check producer name
if model.producer_name:
    print(f"\nProducer: {model.producer_name}")

# Check domain
if model.domain:
    print(f"Domain: {model.domain}")

# Try to find class labels in various places
print("\n" + "="*60)
print("SEARCHING FOR CLASS LABELS")
print("="*60)

# Check graph inputs/outputs
print("\nGraph Outputs:")
for output in model.graph.output:
    print(f"  - {output.name}: {output.type}")
    if output.doc_string:
        print(f"    Doc: {output.doc_string}")

# Check for any string tensors or initializers
print("\nChecking Initializers for strings:")
for init in model.graph.initializer:
    if "class" in init.name.lower() or "label" in init.name.lower():
        print(f"  Found: {init.name}")
        print(f"    Type: {init.data_type}")
        print(f"    Shape: {init.dims}")

# Check JSON files
print("\n" + "="*60)
print("CHECKING JSON FILES")
print("="*60)

import os
json_files = [f for f in os.listdir('.') if f.endswith('.json')]
for json_file in json_files:
    print(f"\nFile: {json_file}")
    try:
        with open(json_file, 'r') as f:
            data = json.load(f)
            if 'classes' in data:
                print(f"  Found 'classes' key!")
                print(f"  Number of classes: {len(data['classes'])}")
                print(f"  First 5 classes: {data['classes'][:5]}")
            elif 'class_names' in data:
                print(f"  Found 'class_names' key!")
                print(f"  Number of classes: {len(data['class_names'])}")
                print(f"  First 5 classes: {data['class_names'][:5]}")
            else:
                print(f"  Keys: {list(data.keys())}")
    except Exception as e:
        print(f"  Error reading: {e}")

print("\n" + "="*60)
print("CONCLUSION")
print("="*60)
print("""
If no class labels are found in the ONNX model metadata,
the labels are typically:
1. Provided separately in a JSON file
2. Standard COCO 80 classes (default for YOLOv8-based models)
3. Specified in the training configuration

The UHD model is trained on COCO dataset, so it uses
the standard COCO 80 class labels by default.
""")
