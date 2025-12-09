#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <cmath>
#include <algorithm>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"
#include "esp_timer.h"

#include "bsp/m5stack_core_s3.h"
#include "lvgl.h"
#include "jpeg_decoder.h"
#include "dl_model_base.hpp"

using namespace dl;

static const char *TAG = "UHD_DET";

const int INPUT_SIZE   = 64;
const int GRID_SIZE    = 8;
const int NUM_ANCHORS  = 8;
const int NUM_CLASSES  = 1;

struct Detection {
    float x, y;
    float w, h;
    int   class_id;
    float confidence;
    const char* label;
};

static const char* CLASS_LABELS[NUM_CLASSES] = {"person"};

struct ModelConstants {
    float anchors[NUM_ANCHORS][2];
    float wh_scale[NUM_ANCHORS][2];
};

// sigmoid clamp
static inline float sigmoid(float x){
    if (x < -80.f) return 0.f;
    if (x > 80.f)  return 1.f;
    return 1.f / (1.f + expf(-x));
}

static inline float softplus(float x){
    float ax=fabsf(x);
    return log1pf(expf(-ax)) + fmaxf(x,0.f);
}

float calculate_iou(const Detection &a, const Detection &b){
    float ax1=a.x-a.w*0.5f, ay1=a.y-a.h*0.5f;
    float ax2=a.x+a.w*0.5f, ay2=a.y+a.h*0.5f;
    float bx1=b.x-b.w*0.5f, by1=b.y-b.h*0.5f;
    float bx2=b.x+b.w*0.5f, by2=b.y+b.h*0.5f;

    float xx1=fmaxf(ax1,bx1);
    float yy1=fmaxf(ay1,by1);
    float xx2=fminf(ax2,bx2);
    float yy2=fminf(ay2,by2);

    float w=fmaxf(0.f,xx2-xx1);
    float h=fmaxf(0.f,yy2-yy1);
    float inter=w*h;

    float A=a.w*a.h;
    float B=b.w*b.h;

    return inter / fmaxf(A+B-inter,1e-6f);
}

std::vector<Detection> apply_nms(std::vector<Detection>& dets,float iou_t){
    std::sort(dets.begin(), dets.end(),
              [](auto&a,auto&b){ return a.confidence>b.confidence; });

    std::vector<Detection> out;
    for(auto &d:dets){
        bool ok=true;
        for(auto &o:out){
            if(calculate_iou(d,o)>iou_t){
                ok=false; break;
            }
        }
        if(ok) out.push_back(d);
    }
    return out;
}


// ------------------------------------------------------
// UltraTinyOD decode
// ------------------------------------------------------
std::vector<Detection> decode_utod(
    const ModelConstants& MC,
    const float* map,
    int Cc, int H, int W,
    float conf_thresh)
{
    std::vector<Detection> out;

    const int per_anchor = Cc / NUM_ANCHORS; // normally 56/8 = 7

    ESP_LOGI(TAG,"decode_utod: H=%d W=%d Cc=%d per_anchor=%d",
             H,W,Cc,per_anchor);

    int candidate = 0;

    for(int a=0;a<NUM_ANCHORS;a++){

        float pw = MC.anchors[a][0] * MC.wh_scale[a][0];
        float ph = MC.anchors[a][1] * MC.wh_scale[a][1];

        ESP_LOGI(TAG,
          "Anchor[%d]: base=(%.4f,%.4f) scale=(%.4f,%.4f) size=(%.4f,%.4f)",
          a,MC.anchors[a][0],MC.anchors[a][1],
          MC.wh_scale[a][0],MC.wh_scale[a][1],pw,ph);

        for(int gy=0;gy<H;gy++){
            for(int gx=0;gx<W;gx++){

                auto idx=[&](int ch){
                    int c = a*per_anchor + ch;
                    return map[(c*H + gy)*W + gx];
                };

                float tx=idx(0);
                float ty=idx(1);
                float tw=idx(2);
                float th=idx(3);
                float obj=idx(4);
                float q  =idx(5);
                float cls=idx(6);

                float score = sigmoid(obj)*sigmoid(q)*sigmoid(cls);
                if(score < conf_thresh) continue;

                candidate++;

                float cx = (sigmoid(tx)+gx)/W;
                float cy = (sigmoid(ty)+gy)/H;
                float bw = pw * softplus(tw);
                float bh = ph * softplus(th);

                if(bw<=0 || bh<=0) continue;

                Detection d;
                d.x=cx; d.y=cy; d.w=bw; d.h=bh;
                d.class_id=0;
                d.confidence=score;
                d.label=CLASS_LABELS[0];
                out.push_back(d);
            }
        }
    }

    ESP_LOGI(TAG,"decode_utod: candidates=%d out=%d",
             candidate,(int)out.size());

    return out;
}


// ------------------------------------------------------
// Resize RGB565 → RGB888
// ------------------------------------------------------
void resize_rgb565_to_rgb888(uint16_t* src,int sw,int sh,
                             uint8_t* dst,int dw,int dh){
    float rx=(float)sw/dw;
    float ry=(float)sh/dh;

    for(int y=0;y<dh;y++){
        for(int x=0;x<dw;x++){
            int ix=(int)(x*rx); if(ix>=sw)ix=sw-1;
            int iy=(int)(y*ry); if(iy>=sh)iy=sh-1;

            uint16_t p=src[iy*sw+ix];
            uint8_t r=((p>>11)&0x1F)<<3;
            uint8_t g=((p>>5)&0x3F)<<2;
            uint8_t b=(p&0x1F)<<3;

            int di=(y*dw+x)*3;
            dst[di+0]=r;
            dst[di+1]=g;
            dst[di+2]=b;
        }
    }
}


// ------------------------------------------------------
// Normalize (HWC→CHW float)
// ------------------------------------------------------
void normalize_rgb888_to_float(uint8_t* src,float* dst,int size){
    int px = size*size;
    for(int c=0;c<3;c++){
        for(int i=0;i<px;i++){
            dst[c*px + i] = src[i*3 + c] / 255.f;
        }
    }
}


// ------------------------------------------------------
// app_main
// ------------------------------------------------------
extern "C" void app_main(void){

    ESP_LOGI(TAG,"Start UltraTinyOD");
    lv_display_t* disp=bsp_display_start();
    (void)disp;

    // --- SD mounting ---
    ESP_LOGI(TAG,"Mount SD card...");
    if(bsp_sdcard_mount()!=ESP_OK){
        ESP_LOGE(TAG,"SD mount error");
        return;
    }
    ESP_LOGI(TAG,"SD mounted");

    // --- read model constants ---
    ModelConstants MC;
    FILE* f=fopen("/sdcard/model.bin","rb");
    fread(MC.anchors,sizeof(float),NUM_ANCHORS*2,f);
    fread(MC.wh_scale,sizeof(float),NUM_ANCHORS*2,f);
    fclose(f);

    for(int i=0;i<NUM_ANCHORS;i++){
        ESP_LOGI(TAG,"A[%d] anchor=(%.4f,%.4f) scale=(%.4f,%.4f)",
            i,MC.anchors[i][0],MC.anchors[i][1],
              MC.wh_scale[i][0],MC.wh_scale[i][1]);
    }

    // --- load ESP-DL model ---
    ESP_LOGI(TAG,"Load ESP-DL model...");
    Model* model=new Model("/sdcard/model.espdl",
                           fbs::MODEL_LOCATION_IN_SDCARD);
    ESP_LOGI(TAG,"Model loaded");

    // --- JPEG decode ---
    f=fopen("/sdcard/image.jpg","rb");
    fseek(f,0,SEEK_END); int jsz=ftell(f);
    fseek(f,0,SEEK_SET);
    uint8_t* jpg=(uint8_t*)malloc(jsz);
    fread(jpg,1,jsz,f);
    fclose(f);

    int out_w=320, out_h=240;
    uint8_t* rgb565=(uint8_t*)heap_caps_malloc(out_w*out_h*2,
                                               MALLOC_CAP_SPIRAM);

    esp_jpeg_image_cfg_t cfg={
        .indata=jpg,
        .indata_size=(uint32_t)jsz,
        .outbuf=rgb565,
        .outbuf_size=(uint32_t)(out_w*out_h*2),
        .out_format=JPEG_IMAGE_FORMAT_RGB565,
        .out_scale=JPEG_IMAGE_SCALE_0,
    };
    esp_jpeg_image_output_t dec;

    int64_t t0=esp_timer_get_time();
    esp_jpeg_decode(&cfg,&dec);
    int64_t t1=esp_timer_get_time();
    free(jpg);

    ESP_LOGI(TAG,"JPEG decoded w=%d h=%d bytes=%u time=%.2fms",
        dec.width, dec.height, (unsigned)dec.output_len,
        (t1-t0)/1000.f);

    // --- resize + normalize ---
    uint8_t* rgb888=(uint8_t*)heap_caps_malloc(64*64*3,
                                               MALLOC_CAP_SPIRAM);
    float* input=(float*)heap_caps_malloc(64*64*3*sizeof(float),
                                          MALLOC_CAP_SPIRAM);

    int64_t t2=esp_timer_get_time();
    resize_rgb565_to_rgb888((uint16_t*)rgb565,dec.width,dec.height,
                            rgb888,64,64);
    normalize_rgb888_to_float(rgb888,input,64);
    int64_t t3=esp_timer_get_time();
    ESP_LOGI(TAG,"Resize+normalize = %.2fms",(t3-t2)/1000.f);

    // --- copy input → ESP-DL tensor (NCHW→NHWC) ---
    TensorBase* tin=model->get_input();
    ESP_LOGI(TAG,"Input tensor shape = [%d,%d,%d,%d]",
        tin->shape[0],tin->shape[1],tin->shape[2],tin->shape[3]);

    {
        float* src = input; // CHW (3,64,64)
        float* dst = (float*)tin->get_element_ptr(); // NHWC

        for(int h=0; h<64; h++){
            for(int w=0; w<64; w++){
                for(int c=0; c<3; c++){
                    int src_index = c*64*64 + h*64 + w;
                    int dst_index = h*64*3 + w*3 + c;
                    dst[dst_index] = src[src_index];
                }
            }
        }
    }

    // --- inference ---
    int64_t t4=esp_timer_get_time();
    model->run();
    int64_t t5=esp_timer_get_time();
    ESP_LOGI(TAG,"Inference = %.2fms",(t5-t4)/1000.f);

    // --- output ---
    TensorBase* tout=model->get_output();

    int N = tout->shape[0];
    int H = tout->shape[1];
    int W = tout->shape[2];
    int Cc= tout->shape[3];

    float scale = DL_SCALE(tout->exponent);
    int total = Cc*H*W;

    ESP_LOGI(TAG,"Output shape = [%d,%d,%d,%d] scale=%g total=%d",
             N,H,W,Cc,scale,total);

    std::vector<float> fmap(total);
    int8_t* qout=(int8_t*)tout->get_element_ptr();
    for(int i=0;i<total;i++) fmap[i] = qout[i] * scale;

    auto raw = decode_utod(MC, fmap.data(), Cc, H, W, 0.25f);
    auto det = apply_nms(raw, 0.5f);

    ESP_LOGI(TAG,"Detections=%d",(int)det.size());

    // --- draw ---
    bsp_display_lock(0);
    lv_obj_t* canvas=lv_canvas_create(lv_screen_active());
    lv_canvas_set_buffer(canvas,rgb565,dec.width,dec.height,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_center(canvas);

    for(auto &d:det){
        float cx=d.x*dec.width;
        float cy=d.y*dec.height;
        float w =d.w*dec.width;
        float h =d.h*dec.height;

        int X=(int)(cx-w*0.5f);
        int Y=(int)(cy-h*0.5f);

        lv_obj_t* rect=lv_obj_create(lv_screen_active());
        lv_obj_set_size(rect,(int)w,(int)h);
        lv_obj_set_pos(rect,X,Y);
        lv_obj_set_style_bg_opa(rect,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_width(rect,3,0);
        lv_obj_set_style_border_color(rect,
                            lv_color_hex(0xFF0000),0);

        lv_obj_t* lbl=lv_label_create(lv_screen_active());
        char buf[64];
        sprintf(buf,"%s %.0f%%",
                d.label,d.confidence*100);
        lv_label_set_text(lbl,buf);
        lv_obj_set_pos(lbl,X,Y-20);
    }

    bsp_display_unlock();

    while(1){
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}
