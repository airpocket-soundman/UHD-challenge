"""
Check the original UHD ONNX model (before multi-output conversion)
"""
import onnx
import onnxruntime as ort
import numpy as np
import os

# Load original model
script_dir = os.path.dirname(os.path.abspath(__file__))
model_path = os.path.join(script_dir, "onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx")

print("="*60)
print("CHECKING ORIGINAL MODEL")
print("="*60)
print(f"Model: {os.path.basename(model_path)}")

# Load and run inference
print("\nRunning inference...")
session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])

# Get input info
input_name = session.get_inputs()[0].name
input_shape = session.get_inputs()[0].shape
print(f"\nInput: {input_name}")
print(f"  Shape: {input_shape}")

# Get output info
print(f"\nModel has {len(session.get_outputs())} output(s):")
for i, output in enumerate(session.get_outputs()):
    print(f"  Output {i}: {output.name}")
    print(f"    Shape: {output.shape}")

# Run inference
dummy_input = np.random.rand(1, 3, 64, 64).astype(np.float32)
outputs = session.run(None, {input_name: dummy_input})

print(f"\n{'='*60}")
print("ACTUAL OUTPUT SHAPES")
print("="*60)
for i, output in enumerate(outputs):
    print(f"Output {i}: {output.shape}")
    if len(output.shape) == 3:
        batch, num_det, features = output.shape
        print(f"  Batch: {batch}, Detections: {num_det}, Features: {features}")
        
        # Analyze feature dimension
        if features == 6:
            print(f"  Features: [score, cls_id, x1, y1, x2, y2]")
        elif features == 85:
            print(f"  Features: [x, y, w, h, obj_score, ...80 class scores]")
            print(f"  → This is COCO 80-class format!")
        elif features == 5:
            print(f"  Features: [x, y, w, h, score]")
            print(f"  → This is single-class format!")
        else:
            print(f"  Unknown feature format")

# Sample some values
if len(outputs) > 0:
    print(f"\n{'='*60}")
    print("SAMPLE OUTPUT VALUES (first detection)")
    print("="*60)
    out = outputs[0][0]  # First batch, all detections
    if len(out) > 0:
        first_det = out[0]
        print(f"First detection: {first_det}")
        print(f"Number of values: {len(first_det)}")
        
        if len(first_det) == 6:
            print(f"\nInterpretation (assuming [score, cls, x1, y1, x2, y2]):")
            print(f"  Score: {first_det[0]:.4f}")
            print(f"  Class ID: {first_det[1]:.4f}")
            print(f"  BBox: ({first_det[2]:.4f}, {first_det[3]:.4f}, {first_det[4]:.4f}, {first_det[5]:.4f})")
        elif len(first_det) == 85:
            print(f"\nInterpretation (YOLO format):")
            print(f"  Center X: {first_det[0]:.4f}")
            print(f"  Center Y: {first_det[1]:.4f}")
            print(f"  Width: {first_det[2]:.4f}")
            print(f"  Height: {first_det[3]:.4f}")
            print(f"  Objectness: {first_det[4]:.4f}")
            print(f"  Class scores (first 5): {first_det[5:10]}")
            print(f"  Max class score: {np.max(first_det[5:]):.4f}")
            print(f"  Max class ID: {np.argmax(first_det[5:])}")

print("\n" + "="*60)
print("CONCLUSION")
print("="*60)

if len(outputs) > 0 and len(outputs[0].shape) == 3:
    features = outputs[0].shape[2]
    if features == 85:
        print("""
✅ This is a COCO 80-CLASS model!

Output format: [1, 100, 85]
  - 85 = 4 (bbox) + 1 (objectness) + 80 (class scores)
  - Classes: COCO 80 classes (person, bicycle, car, ...)

For M5StackS3 code:
  #define NUM_CLASSES 80
  const char* COCO_CLASSES[] = {"person", "bicycle", "car", ...};
""")
    elif features == 6:
        class_id_value = outputs[0][0, 0, 1] if len(outputs[0][0]) > 0 else 0
        print(f"""
Output format: [1, 100, 6]
  - 6 = score + class_id + bbox (x1, y1, x2, y2)
  - Class ID range: Check actual values

Sample class_id: {class_id_value:.1f}
""")
        if class_id_value == 0.0:
            print("→ Likely SINGLE-CLASS model (person only)")
        else:
            print(f"→ Multi-class model (classes 0-{int(class_id_value)})")
