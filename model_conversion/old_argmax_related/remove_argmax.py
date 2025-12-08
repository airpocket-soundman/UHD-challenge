"""
Remove ArgMax and subsequent operations from UHD ONNX model
to make it compatible with ESP-DL export
"""
import onnx
from onnx import helper
import argparse


def remove_argmax_and_after(input_model, output_model):
    """
    Remove ArgMax and all subsequent operations from the model.
    Set the output to be right before ArgMax (class scores).
    
    Args:
        input_model: Path to input ONNX model
        output_model: Path to output ONNX model
    """
    print(f"Loading model: {input_model}")
    model = onnx.load(input_model)
    
    # Find ArgMax node
    argmax_nodes = [n for n in model.graph.node if n.op_type == 'ArgMax']
    if not argmax_nodes:
        print("No ArgMax node found. Model may already be compatible.")
        return False
    
    argmax_node = argmax_nodes[0]
    print(f"\nFound ArgMax node: {argmax_node.name}")
    print(f"  Input: {argmax_node.input[0]}")
    print(f"  Output: {argmax_node.output[0]}")
    
    # The input to ArgMax will become our new output
    new_output_name = argmax_node.input[0]
    
    # Find all nodes that come after ArgMax (nodes that depend on ArgMax output)
    nodes_to_keep = []
    argmax_output = argmax_node.output[0]
    
    # Build dependency graph
    dependent_outputs = set([argmax_output])
    changed = True
    while changed:
        changed = False
        for node in model.graph.node:
            if node.op_type == 'ArgMax':
                continue
            # If this node uses any dependent output, its outputs are also dependent
            if any(inp in dependent_outputs for inp in node.input):
                for out in node.output:
                    if out not in dependent_outputs:
                        dependent_outputs.add(out)
                        changed = True
    
    # Keep only nodes that don't produce dependent outputs
    for node in model.graph.node:
        if node.op_type == 'ArgMax':
            print(f"  Removing: {node.op_type} - {node.name}")
            continue
        if any(out in dependent_outputs for out in node.output):
            print(f"  Removing: {node.op_type} - {node.name}")
            continue
        nodes_to_keep.append(node)
    
    print(f"\nKept {len(nodes_to_keep)}/{len(model.graph.node)} nodes")
    
    # Find the value info for new output
    new_output_value_info = None
    for value_info in model.graph.value_info:
        if value_info.name == new_output_name:
            new_output_value_info = value_info
            break
    
    # If not in value_info, check if it's a node output
    if new_output_value_info is None:
        for node in nodes_to_keep:
            if new_output_name in node.output:
                # Create value info from node output
                # We need to infer the shape - for now, create without shape
                new_output_value_info = helper.make_tensor_value_info(
                    new_output_name,
                    onnx.TensorProto.FLOAT,
                    None  # Shape will be inferred
                )
                break
    
    if new_output_value_info is None:
        print(f"Warning: Could not find value info for {new_output_name}")
        # Create a generic output
        new_output_value_info = helper.make_tensor_value_info(
            new_output_name,
            onnx.TensorProto.FLOAT,
            None
        )
    
    print(f"\nNew output: {new_output_name}")
    
    # Create new graph
    new_graph = helper.make_graph(
        nodes_to_keep,
        model.graph.name,
        model.graph.input,
        [new_output_value_info],  # New single output
        model.graph.initializer
    )
    
    # Copy opset
    new_model = helper.make_model(new_graph)
    # Clear and copy opset imports
    del new_model.opset_import[:]
    for opset in model.opset_import:
        new_opset = new_model.opset_import.add()
        new_opset.domain = opset.domain
        new_opset.version = opset.version
    new_model.ir_version = model.ir_version
    
    # Check and save
    try:
        onnx.checker.check_model(new_model)
        print("\n✓ Model validation passed")
    except Exception as e:
        print(f"\n⚠ Model validation warning: {e}")
        print("  Proceeding anyway...")
    
    onnx.save(new_model, output_model)
    print(f"\n✓ Saved modified model: {output_model}")
    print(f"\nNote: The ArgMax operation (class index selection) should be")
    print(f"      implemented on the ESP32 side using the model output.")
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Remove ArgMax from UHD model for ESP-DL compatibility"
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
    
    success = remove_argmax_and_after(args.input, args.output)
    return 0 if success else 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
