#pragma once

/**
 * SurroundViewParams
 * 对应 Python: surround_view/param_settings.py
 * 定义环视拼接图像的尺寸和区域坐标。
 */
struct SurroundViewParams {
    // --- 基本参数（与 param_settings.py 保持一致）---
    int shift_w     = 100;
    int shift_h     = 100;
    int inn_shift_w = 60;
    int inn_shift_h = 60;

    int LEFT1  = 170;
    int LEFT2  = 230;
    int RIGHT1 = 170;
    int RIGHT2 = 230;
    int FRONT1 = 98;
    int FRONT2 = 98;
    int BACK1  = 98;
    int BACK2  = 98;
    int GRID   = 20;

    // --- 计算值（调用 compute() 后有效）---
    int total_w = 0;
    int total_h = 0;
    int car_w   = 0;
    int car_h   = 0;

    // 像素坐标（汽车四角在鸟瞰图中的位置）
    // xl_px: 汽车左边界列号 (pixel col)
    // xr_px: 汽车右边界列号
    // yt_px: 汽车上边界行号 (pixel row)
    // yb_px: 汽车下边界行号
    int xl_px = 0;
    int xr_px = 0;
    int yt_px = 0;
    int yb_px = 0;

    // 世界坐标（浮点，原点在图像中心，x右正y上正）
    float xl = 0.f, xr = 0.f, yt = 0.f, yb = 0.f;

    // project_shapes: 每个相机投影后的图像尺寸 (width, height)
    // 顺序: front, back, left, right
    int proj_front_w = 0, proj_front_h = 0;
    int proj_back_w  = 0, proj_back_h  = 0;
    int proj_left_w  = 0, proj_left_h  = 0;
    int proj_right_w = 0, proj_right_h = 0;

    void compute() {
        total_w = GRID*5 + FRONT1 + GRID*7 + FRONT2 + GRID*5 + 2 * shift_w;
        total_h = GRID*5 + LEFT1  + GRID*7 + LEFT2  + GRID*5 + 2 * shift_h;

        car_w = total_w - 2*shift_w - 2*5*GRID - 2*inn_shift_w;
        car_h = total_h - 2*shift_h - 2*5*GRID - 2*inn_shift_h;

        // 世界坐标（浮点）
        xl = -(float)car_w / 2.f;
        xr =  (float)car_w / 2.f;
        yt =  (float)car_h / 2.f;
        yb = -(float)car_h / 2.f;

        // 像素坐标: 对应 birdview.py 中 xl_px/xr_px/yt_px/yb_px
        xl_px = (int)(total_w / 2 + xl);
        xr_px = (int)(total_w / 2 + xr);
        yt_px = (int)(total_h / 2 - yt);
        yb_px = (int)(total_h / 2 - yb);

        // project_shapes (width, height)
        proj_front_w = total_w;
        proj_front_h = (total_h - car_h) / 2;
        proj_back_w  = total_w;
        proj_back_h  = (total_h - car_h) / 2;
        proj_left_w  = (total_w - car_w) / 2;
        proj_left_h  = total_h;
        proj_right_w = (total_w - car_w) / 2;
        proj_right_h = total_h;
    }

    // 单例，全局访问
    static SurroundViewParams& instance() {
        static SurroundViewParams p;
        static bool inited = false;
        if (!inited) { p.compute(); inited = true; }
        return p;
    }
};
