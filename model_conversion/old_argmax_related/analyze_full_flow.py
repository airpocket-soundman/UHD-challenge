import onnx

model = onnx.load('model_conversion/onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx')

print("=== Full processing flow analysis ===\n")

# Find all nodes that lead to final output
output_name = model.graph.output[0].name
print(f"Final output: {output_name}\n")

# Build reverse dependency graph
print("Nodes contributing to final output (reverse order):\n")

def find_producers(target_var, depth=0):
    indent = "  " * depth
    for node in model.graph.node:
        if target_var in node.output:
            print(f"{indent}{node.op_type}: {node.name}")
            print(f"{indent}  Inputs: {list(node.input)}")
            print(f"{indent}  Outputs: {list(node.output)}")
            # Recursively find producers of inputs
            for inp in node.input:
                # Skip constants
                if any(init.name == inp for init in model.graph.initializer):
                    continue
                find_producers(inp, depth + 1)
            return node
    return None

final_node = find_producers(output_name)

print("\n" + "="*50)
print("Analysis:")
print("="*50)

# Find ArgMax and what it needs
argmax = [n for n in model.graph.node if n.op_type == 'ArgMax'][0]
print(f"\nArgMax input: {argmax.input[0]}")

# Find what produces ArgMax input
for node in model.graph.node:
    if argmax.input[0] in node.output:
        print(f"ArgMax input produced by: {node.op_type} - {node.name}")
        print(f"  This node outputs: {node.output}")

# Find all inputs to the removed chain
print("\n\nAll inputs needed by removed nodes:")
removed_node_names = ['/ArgMax', '/Reshape_4', '/GatherElements', '/Cast_2', '/Unsqueeze_2', '/Concat']
all_inputs = set()
for node in model.graph.node:
    if node.name in removed_node_names:
        for inp in node.input:
            if not any(init.name == inp for init in model.graph.initializer):
                all_inputs.add(inp)
                
for inp in sorted(all_inputs):
    print(f"  - {inp}")
    # Find producer
    for node in model.graph.node:
        if inp in node.output:
            print(f"      Produced by: {node.op_type} - {node.name}")
