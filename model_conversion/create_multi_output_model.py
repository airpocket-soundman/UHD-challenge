"""
Create a multi-output ONNX model that excludes ArgMax but keeps all other outputs
needed to reconstruct the final detection result.
"""
import onnx
from onnx import helper
import argparse


def create_multi_output_model(input_model, output_model):
    """
    Modify the model to output all 6 elements needed for final Concat,
    except replace the ArgMax-dependent element with its input (class scores).
    
    The 6 elements for Concat are:
    1. /TopK_output_0 (detection scores)
    2. /Cast_2_output_0 (class IDs) <- depends on ArgMax, replace with /Mul_3_output_0
    3. /GatherElements_1_output_0 (bbox x1)
    4. /GatherElements_2_output_0 (bbox y1) 
    5. /GatherElements_3_output_0 (bbox x2)
    6. /GatherElements_4_output_0 (bbox y2)
    """
    print(f"Loading model: {input_model}")
    model = onnx.load(input_model)
    
    # Define the 6 outputs we need (5 original + 1 replacement)
    output_names = [
        '/Unsqueeze_1_output_0',  # Element 1: detection scores
        '/Mul_3_output_0',         # Element 2: class scores (replaces ArgMax output)
        '/Unsqueeze_3_output_0',  # Element 3: bbox coordinate
        '/Unsqueeze_4_output_0',  # Element 4: bbox coordinate
        '/Unsqueeze_5_output_0',  # Element 5: bbox coordinate
        '/Unsqueeze_6_output_0',  # Element 6: bbox coordinate
    ]
    
    print("\nTarget outputs:")
    for i, name in enumerate(output_names, 1):
        print(f"  {i}. {name}")
    
    # Remove ArgMax and its dependent nodes
    argmax_node = [n for n in model.graph.node if n.op_type == 'ArgMax'][0]
    argmax_output = argmax_node.output[0]
    
    # Find all nodes that depend on ArgMax
    dependent_nodes = [argmax_node]
    dependent_outputs = set([argmax_output])
    
    changed = True
    while changed:
        changed = False
        for node in model.graph.node:
            if node in dependent_nodes:
                continue
            if any(inp in dependent_outputs for inp in node.input):
                dependent_nodes.append(node)
                for out in node.output:
                    if out not in dependent_outputs:
                        dependent_outputs.add(out)
                        changed = True
    
    print(f"\nRemoving {len(dependent_nodes)} ArgMax-dependent nodes...")
    
    # Keep only nodes that are not ArgMax-dependent
    nodes_to_keep = [n for n in model.graph.node if n not in dependent_nodes]
    
    print(f"Kept {len(nodes_to_keep)}/{len(model.graph.node)} nodes")
    
    # Create output value infos
    new_outputs = []
    for output_name in output_names:
        # Try to find existing value info
        value_info = None
        for vi in model.graph.value_info:
            if vi.name == output_name:
                value_info = vi
                break
        
        if value_info is None:
            # Create new value info
            value_info = helper.make_tensor_value_info(
                output_name,
                onnx.TensorProto.FLOAT,
                None  # Shape will be inferred
            )
        
        new_outputs.append(value_info)
    
    print(f"\nCreating model with {len(new_outputs)} outputs")
    
    # Create new graph with multiple outputs
    new_graph = helper.make_graph(
        nodes_to_keep,
        model.graph.name,
        model.graph.input,
        new_outputs,  # Multiple outputs
        model.graph.initializer
    )
    
    # Create new model
    new_model = helper.make_model(new_graph)
    
    # Copy opset
    del new_model.opset_import[:]
    for opset in model.opset_import:
        new_opset = new_model.opset_import.add()
        new_opset.domain = opset.domain
        new_opset.version = opset.version
    new_model.ir_version = model.ir_version
    
    # Validate and save
    try:
        onnx.checker.check_model(new_model)
        print("\n✓ Model validation passed")
    except Exception as e:
        print(f"\n⚠ Model validation warning: {e}")
        print("  Proceeding anyway...")
    
    onnx.save(new_model, output_model)
    print(f"\n✓ Saved multi-output model: {output_model}")
    
    print("\n" + "="*60)
    print("ESP32 Implementation:")
    print("="*60)
    print("""
The model now outputs 6 tensors:
  1. detection_scores  (from TopK)
  2. class_scores      (for ArgMax - replaces class_ids)
  3. bbox_coord_1      (from GatherElements_1)
  4. bbox_coord_2      (from GatherElements_2)
  5. bbox_coord_3      (from GatherElements_3)
  6. bbox_coord_4      (from GatherElements_4)

On ESP32, you need to:
  1. Run model inference -> get 6 outputs
  2. Apply ArgMax on output[1] (class_scores) to get class_ids
  3. Concatenate all 6 elements (with class_ids replacing class_scores)
  4. Apply NMS and other post-processing
""")
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Create multi-output model without ArgMax"
    )
    parser.add_argument(
        "--input",
        required=True,
        help="Input ONNX model path"
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Output ONNX model path"
    )
    
    args = parser.parse_args()
    
    success = create_multi_output_model(args.input, args.output)
    return 0 if success else 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
