package com.chengzhang.iristracking;

import android.content.res.AssetManager;
import android.view.Surface;

public class NativeOpengl {
    static {
        System.loadLibrary("iris_tracking");
    }

    public native static void  setAssetManager(AssetManager assetManager);

    public native static void surfaceCreate(Surface surface);

    public native static void surfaceChange(int width, int height);

    public native static void surfaceDestroy();

}
