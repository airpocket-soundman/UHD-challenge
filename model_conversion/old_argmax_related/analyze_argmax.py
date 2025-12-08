import onnx

model = onnx.load('model_conversion/onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx')
argmax = [n for n in model.graph.node if n.op_type == 'ArgMax'][0]

print('ArgMax node:')
print(f'  Output: {argmax.output[0]}')

print('\nNodes that use ArgMax output (direct dependents):')
direct_deps = []
for n in model.graph.node:
    if argmax.output[0] in n.input:
        direct_deps.append(n)
        print(f'  {n.op_type}: {n.name}')
        print(f'    Inputs: {list(n.input)}')
        print(f'    Outputs: {list(n.output)}')

print(f'\nTotal direct dependents: {len(direct_deps)}')

# Trace all subsequent nodes
print('\nAll nodes removed (ArgMax and its dependents):')
removed_outputs = set([argmax.output[0]])
removed_nodes = [argmax]

changed = True
while changed:
    changed = False
    for n in model.graph.node:
        if n in removed_nodes:
            continue
        if any(inp in removed_outputs for inp in n.input):
            removed_nodes.append(n)
            for out in n.output:
                if out not in removed_outputs:
                    removed_outputs.add(out)
                    changed = True

for n in removed_nodes:
    print(f'  {n.op_type}: {n.name}')

print(f'\nTotal removed: {len(removed_nodes)} nodes')

print('\nFinal model output:')
for o in model.graph.output:
    print(f'  {o.name}')
    if o.name in removed_outputs:
        print('    ★ This output was removed!')
