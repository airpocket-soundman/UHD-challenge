# モデルファイルのデプロイ手順

## 必要なファイル

以下のファイルをmicroSDカードにコピーしてください：

### 1. モデルファイル
**元の場所**: `model_conversion/w_ESE+IoU-aware+ReLU/translated/uhd_relu_w64.espdl`  
**SDカード配置先**: `model.espdl` (ルートディレクトリ)  
**サイズ**: 約1.68MB

### 2. 定数ファイル
**元の場所**: `model_conversion/w_ESE+IoU-aware+ReLU/translated/uhd_relu_w64_constants.bin`  
**SDカード配置先**: `model.bin` (ルートディレクトリ)  
**サイズ**: 128 bytes

### 3. テスト画像（既存）
**SDカード配置先**: `image.jpg` (ルートディレクトリ)  
**推奨サイズ**: 320×240 または任意のサイズ

## SDカードの最終構成

```
microSD (root)/
├── model.espdl    # ESP-DLモデル (1.68 MB)
├── model.bin      # anchors/wh_scale定数 (128 bytes)
└── image.jpg      # テスト画像
```

## PowerShellコマンド例

```powershell
# SDカードのドライブレターを確認（例：E:）
$SD_DRIVE = "E:"

# ファイルをコピー
copy ..\model_conversion\w_ESE+IoU-aware+ReLU\translated\uhd_relu_w64.espdl ${SD_DRIVE}\model.espdl
copy ..\model_conversion\w_ESE+IoU-aware+ReLU\translated\uhd_relu_w64_constants.bin ${SD_DRIVE}\model.bin

# 既存のimage.jpgも確認
dir ${SD_DRIVE}\
```

## モデル情報

- **入力**: [1, 3, 64, 64] - RGB画像、64×64ピクセル、正規化 [0, 1]
- **出力**: 出力テンソル形状: [1, 56, 8, 8]
UltraTinyOD（w ESE + IoU-aware + 1 クラス person）の構造

チャンネル構成（56 = 8 anchors × 7 channels）

各 anchor（全8個）につき 7チャネルを出力する：

index	意味	説明
0	tx	center-x offset（sigmoid）
1	ty	center-y offset（sigmoid）
2	tw	width logit（softplus）
3	th	height logit（softplus）
4	obj	objectness ロジット
5	quality	IoU-aware quality ロジット
6	cls	person クラスのロジット（1クラスのみ）

➡ (7 チャンネル × 8 anchors = 56 チャンネル)

全体グリッドは 8×8（合計 64 cell）

つまり出力は：

[BATCH=1, CHANNELS=56, H=8, W=8]


YOLO系の構造だが、classes=1 & IoU-aware 対応の UltraTinyOD 専用設計モデル。
- **量子化**: INT8
- **フレームワーク**: ESP-DL

## 定数値（参考）

### Anchors [8, 2]
```
[2.219e-06, 4.825e-06]  # Anchor 0
[4.420e-06, 8.911e-06]  # Anchor 1
[5.508e-06, 2.023e-05]  # Anchor 2
[1.044e-05, 1.459e-05]  # Anchor 3
[1.141e-05, 3.574e-05]  # Anchor 4
[1.995e-05, 5.522e-05]  # Anchor 5
[3.516e-05, 7.315e-05]  # Anchor 6
[6.757e-05, 8.796e-05]  # Anchor 7
```

### WH Scale [8, 2]
```
[0.9887, 0.9887]  # Scale 0-6
...
[6.7339, 7.4563]  # Scale 7
```
