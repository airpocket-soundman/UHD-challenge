"""
Check the actual number of classes in the UHD ONNX model
"""
import onnx
import onnxruntime as ort
import numpy as np

# Load model
import os
script_dir = os.path.dirname(os.path.abspath(__file__))
model_path = os.path.join(script_dir, "onnx/ultratinyod_res_anc8_w64_64x64_quality_multi_output.onnx")

print("="*60)
print("CHECKING MODEL CLASS COUNT")
print("="*60)

# Method 1: Check with ONNX
print("\n1. Loading ONNX model...")
model = onnx.load(model_path)

print("\n2. Model outputs:")
for i, output in enumerate(model.graph.output):
    print(f"   Output {i}: {output.name}")
    # Get shape if available
    if output.type.HasField('tensor_type'):
        shape = output.type.tensor_type.shape
        dims = [d.dim_value if d.HasField('dim_value') else '?' for d in shape.dim]
        print(f"      Shape: {dims}")

# Method 2: Run inference to check actual output shape
print("\n3. Running inference to check output shapes...")
session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])

# Create dummy input
input_name = session.get_inputs()[0].name
input_shape = session.get_inputs()[0].shape
print(f"   Input: {input_name}, Shape: {input_shape}")

dummy_input = np.random.rand(1, 3, 64, 64).astype(np.float32)
outputs = session.run(None, {input_name: dummy_input})

print(f"\n4. Actual output shapes:")
for i, output in enumerate(outputs):
    print(f"   Output {i}: shape = {output.shape}")
    
# Check the class scores output (should be output 1)
if len(outputs) > 1:
    class_scores_shape = outputs[1].shape
    print(f"\n{'='*60}")
    print("CLASS SCORES ANALYSIS (Output 1)")
    print("="*60)
    print(f"Shape: {class_scores_shape}")
    
    if len(class_scores_shape) >= 2:
        num_detections = class_scores_shape[0]
        num_classes = class_scores_shape[1]
        print(f"\nNumber of detections: {num_detections}")
        print(f"Number of classes: {num_classes}")
        
        if num_classes == 1:
            print("\n✅ This is a SINGLE-CLASS model (person only)")
            print("   Class ID 0 = 'person'")
        elif num_classes == 80:
            print("\n✅ This is a MULTI-CLASS model (COCO 80 classes)")
            print("   Class IDs 0-79 = COCO classes")
        else:
            print(f"\n⚠️  Unexpected number of classes: {num_classes}")

print("\n" + "="*60)
print("RECOMMENDATION")
print("="*60)

if len(outputs) > 1 and len(outputs[1].shape) >= 2:
    num_classes = outputs[1].shape[1]
    if num_classes == 1:
        print("""
For a SINGLE-CLASS model, update M5StackS3 code:

1. Change NUM_CLASSES:
   #define NUM_CLASSES 1  // Instead of 80

2. Simplify COCO_CLASSES:
   const char* COCO_CLASSES[] = {"person"};

3. ArgMax is unnecessary (only 1 class):
   // Simply use class_id = 0 for all detections
   det.class_id = 0;  // Always "person"
""")
    elif num_classes == 80:
        print("""
For a MULTI-CLASS model (80 classes), current implementation is correct:
   #define NUM_CLASSES 80
   const char* COCO_CLASSES[] = {"person", "bicycle", ...};
""")
