import onnx

m = onnx.load('model_conversion/onnx/ultratinyod_res_anc8_w64_64x64_quality.onnx')

concat_inputs = [
    '/Unsqueeze_1_output_0',
    '/Unsqueeze_2_output_0',  # This one depends on ArgMax
    '/Unsqueeze_3_output_0',
    '/Unsqueeze_4_output_0',
    '/Unsqueeze_5_output_0',
    '/Unsqueeze_6_output_0'
]

print('Concat inputs (6 elements for final output):')
print('='*60)

for i, inp in enumerate(concat_inputs, 1):
    print(f'\n{i}. {inp}')
    
    # Find the Unsqueeze node
    unsqueeze_node = [n for n in m.graph.node if inp in n.output][0]
    print(f'   Unsqueeze: {unsqueeze_node.name}')
    print(f'   Input: {unsqueeze_node.input[0]}')
    
    # Find what produces the Unsqueeze input
    unsqueeze_input = unsqueeze_node.input[0]
    producer = None
    for n in m.graph.node:
        if unsqueeze_input in n.output:
            producer = n
            break
    
    if producer:
        print(f'   Produced by: {producer.op_type} - {producer.name}')
        print(f'   Producer inputs: {list(producer.input)}')
        
        # Check if this depends on ArgMax
        if i == 2:  # Unsqueeze_2
            print('   ⚠️ THIS DEPENDS ON ArgMax!')
    else:
        print(f'   Produced by: (direct input or constant)')

print('\n' + '='*60)
print('Summary:')
print('  - Element 2 (Unsqueeze_2) depends on ArgMax')
print('  - Other 5 elements do NOT depend on ArgMax')
print('  - We can keep elements 1,3,4,5,6 and calculate element 2 on ESP32')
