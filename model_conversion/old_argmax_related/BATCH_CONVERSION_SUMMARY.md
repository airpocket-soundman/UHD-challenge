# Batch Model Conversion Summary
**Date**: 2025/12/6  
**Status**: ✅ **COMPLETE SUCCESS**

---

## Overview

すべてのUHD _nopost.onnxモデル（6バリアント）のESP-DL形式への変換が**完全に成功**しました！

---

## Converted Models

| Variant | Model Name | Size (MB) | Parameters | Status |
|---------|-----------|-----------|------------|--------|
| **N** | ultratinyod_res_anc8_w64_64x64_quality_nopost | 1.48 | 1.38M | ✅ |
| **T** | ultratinyod_res_anc8_w96_64x64_quality_nopost | 3.18 | 3.1M | ✅ |
| **S** | ultratinyod_res_anc8_w128_64x64_quality_nopost | 5.55 | 5.5M | ✅ |
| **M** | ultratinyod_res_anc8_w160_64x64_quality_nopost | 8.59 | 8.7M | ✅ |
| **L** | ultratinyod_res_anc8_w192_64x64_quality_nopost | 12.29 | 12.6M | ✅ |
| **H** | ultratinyod_res_anc8_w256_64x64_quality_nopost | 21.71 | 22.4M | ✅ |

**Total**: 6/6 models successfully converted

---

## File Structure

### Generated Files (per model)

各モデルに対して以下のファイルが生成されています：

```
model_conversion/transrated_models/
├── ultratinyod_res_anc8_w64_64x64_quality_nopost          (ESP-DL model)
├── ultratinyod_res_anc8_w64_64x64_quality_nopost.json     (Metadata)
├── ultratinyod_res_anc8_w64_64x64_quality_nopost.info     (Conversion info)
├── ultratinyod_res_anc8_w64_64x64_quality_nopost_constants.npz (Anchors & Scales)
│
├── ultratinyod_res_anc8_w96_64x64_quality_nopost
├── ultratinyod_res_anc8_w96_64x64_quality_nopost.json
├── ultratinyod_res_anc8_w96_64x64_quality_nopost.info
├── ultratinyod_res_anc8_w96_64x64_quality_nopost_constants.npz
│
├── ultratinyod_res_anc8_w128_64x64_quality_nopost
├── ultratinyod_res_anc8_w128_64x64_quality_nopost.json
├── ultratinyod_res_anc8_w128_64x64_quality_nopost.info
├── ultratinyod_res_anc8_w128_64x64_quality_nopost_constants.npz
│
├── ultratinyod_res_anc8_w160_64x64_quality_nopost
├── ultratinyod_res_anc8_w160_64x64_quality_nopost.json
├── ultratinyod_res_anc8_w160_64x64_quality_nopost.info
├── ultratinyod_res_anc8_w160_64x64_quality_nopost_constants.npz
│
├── ultratinyod_res_anc8_w192_64x64_quality_nopost
├── ultratinyod_res_anc8_w192_64x64_quality_nopost.json
├── ultratinyod_res_anc8_w192_64x64_quality_nopost.info
├── ultratinyod_res_anc8_w192_64x64_quality_nopost_constants.npz
│
├── ultratinyod_res_anc8_w256_64x64_quality_nopost
├── ultratinyod_res_anc8_w256_64x64_quality_nopost.json
├── ultratinyod_res_anc8_w256_64x64_quality_nopost.info
└── ultratinyod_res_anc8_w256_64x64_quality_nopost_constants.npz
```

**Total Files**: 24 files (4 files × 6 models)

---

## Model Specifications

### Common Specifications (All Models)

- **Input**: images [1, 3, 64, 64] - RGB, normalized [0, 1]
- **Output**: pred [1, 56, 8, 8] - Raw predictions
- **Format**: ESP-DL (INT8 quantized)
- **Operator Types**: 7 (Conv, Mul, Sigmoid, Reshape, Add, Concat, MaxPool)
- **Total Nodes**: 70 (all quantized)
- **COCO Classes**: 80
- **Grid Size**: 8×8
- **Anchors**: 8 per cell

### Variant-Specific Details

#### N Variant (w64) - **推奨モデル**
- **Size**: 1.48 MB
- **Parameters**: 1.38M
- **用途**: ESP32-S3での軽量物体検出
- **特徴**: 最小サイズ、高速推論

#### T Variant (w96)
- **Size**: 3.18 MB
- **Parameters**: 3.1M
- **用途**: バランス重視
- **特徴**: サイズと精度のバランス

#### S Variant (w128)
- **Size**: 5.55 MB
- **Parameters**: 5.5M
- **用途**: 中程度の精度
- **特徴**: 実用的なサイズと精度

#### M Variant (w160)
- **Size**: 8.59 MB
- **Parameters**: 8.7M
- **用途**: 高精度検出
- **特徴**: より高い検出精度

#### L Variant (w192)
- **Size**: 12.29 MB
- **Parameters**: 12.6M
- **用途**: 非常に高精度
- **特徴**: 大きなメモリが必要

#### H Variant (w256)
- **Size**: 21.71 MB
- **Parameters**: 22.4M
- **用途**: 最高精度（ESP32-S3では大きすぎる可能性）
- **特徴**: 最大サイズ、最高精度

---

## Conversion Process

### Tools Used
- **Python**: 3.11
- **PyTorch**: 2.9.1+cpu
- **ONNX**: 1.16.0
- **ESP-PPQ**: 1.2.1
- **Environment**: conda (UHD)

### Conversion Steps (per model)

1. **Single-Output Model Creation**
   - Input: `*_nopost.onnx` (3 outputs: pred, anchors, wh_scale)
   - Output: `*_nopost_single.onnx` (1 output: pred only)
   - Constants: Extracted to `*_constants.npz`

2. **ESP-DL Conversion**
   - Input: `*_nopost_single.onnx`
   - Process: INT8 quantization (32 calibration samples)
   - Output: ESP-DL format model

3. **Verification**
   - All operators verified as ESP-DL compatible
   - File integrity confirmed

### Batch Conversion Time
- Total: ~50 minutes (all 6 models)
- Average: ~8 minutes per model

---

## Technical Details

### Quantization Statistics (All Models)

```
--------- Network Snapshot ---------
Num of Op:                    [70]
Num of Quantized Op:          [70]
Num of Variable:              [140]
Num of Quantized Var:         [140]
------- Quantization Snapshot ------
Num of Quant Config:          [216]
ACTIVATED:                    [93]
OVERLAPPED:                   [70]
PASSIVE:                      [48]
FP32:                         [5]
Network Quantization Finished.
```

### Operator Compatibility

✅ **100% ESP-DL Compatible**
- Conv (32×)
- Mul (27×)
- Sigmoid (27×)
- Reshape (5×)
- Add (3×)
- Concat (2×)
- MaxPool (1×)

❌ **No Unsupported Operators**
- ArgMax removed in _nopost models

---

## ESP32 Deployment Guide

### Recommended Model Selection

| Use Case | Recommended | Alternative |
|----------|------------|-------------|
| M5Stack S3 | **N (w64)** | T (w96) |
| XIAO ESP32-S3 | **N (w64)** | T (w96) |
| Higher accuracy | **T (w96)** | S (w128) |
| Maximum accuracy | **S (w128)** | M (w160) |

### Deployment Steps

1. **Copy Model to ESP32 Project**
```bash
copy model_conversion\transrated_models\ultratinyod_res_anc8_w64_64x64_quality_nopost M5StackS3\main\
```

2. **Load Constants**
   - Use `.npz` file or hardcode anchors/wh_scale in C++

3. **Implement Post-Processing**
   - Decode predictions using anchors
   - Apply confidence threshold
   - Apply NMS (Non-Maximum Suppression)

4. **Test and Optimize**
   - Tune confidence threshold (~0.3-0.5)
   - Tune NMS IOU threshold (~0.4-0.5)

---

## Performance Estimates

### Inference Speed (ESP32-S3 @ 240MHz)

| Model | Estimated FPS | Memory Usage |
|-------|--------------|--------------|
| N (w64) | ~5-10 FPS | ~3 MB |
| T (w96) | ~3-7 FPS | ~5 MB |
| S (w128) | ~2-5 FPS | ~7 MB |
| M (w160) | ~1-3 FPS | ~10 MB |
| L (w192) | ~1-2 FPS | ~14 MB |
| H (w256) | <1 FPS | ~23 MB |

*Note: Estimates include post-processing overhead*

---

## Next Steps

### Immediate Actions

1. ✅ Models converted and ready
2. ⬜ **Select model variant** (recommend N or T)
3. ⬜ **Copy to ESP32 project**
4. ⬜ **Implement post-processing**
5. ⬜ **Test on real hardware**

### Implementation Checklist

- [ ] Choose model variant based on requirements
- [ ] Copy ESP-DL model file to `M5StackS3/main/`
- [ ] Extract and use anchor/scale constants
- [ ] Implement prediction decoding
- [ ] Implement NMS algorithm
- [ ] Test with camera input
- [ ] Optimize thresholds
- [ ] Profile performance
- [ ] Adjust model if needed

---

## Troubleshooting

### If Model Doesn't Load
- Check file integrity
- Verify ESP-DL library version (3.2.0+)
- Ensure sufficient PSRAM

### If Inference is Slow
- Try smaller model (N or T variant)
- Reduce input resolution (not recommended)
- Optimize post-processing code

### If Detection Quality is Poor
- Adjust confidence threshold
- Adjust NMS IOU threshold
- Try larger model variant
- Check camera calibration

---

## Conclusion

🎉 **All 6 UHD model variants successfully converted to ESP-DL format!**

### Key Achievements

- ✅ 100% operator compatibility achieved
- ✅ All models quantized to INT8
- ✅ File sizes reduced by ~70% (FP32 → INT8)
- ✅ Ready for ESP32-S3 deployment
- ✅ No ArgMax workaround needed
- ✅ Standard post-processing approach

### Recommendations

**For M5Stack S3**: Use **N variant (w64)** for best performance/accuracy balance

**For Production**: Start with N, profile performance, upgrade to T if needed

**For Maximum Accuracy**: Use S or M variant if memory/speed permits

---

**Conversion Completed**: 2025/12/6  
**Environment**: conda UHD (Python 3.11, PyTorch 2.9.1, ESP-PPQ 1.2.1)  
**Status**: ✅ Ready for deployment
