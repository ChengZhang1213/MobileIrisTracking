package com.chengzhang.iristracking;

import android.content.Context;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

public class NativeSurfaceView  extends SurfaceView implements SurfaceHolder.Callback {
    private Context context;
    public NativeSurfaceView(Context context) {
        super(context);
        this.context= context;
        NativeOpengl.setAssetManager(context.getAssets());
        getHolder().addCallback(this);
    }

    public NativeSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        this.context= context;
        getHolder().addCallback(this);
    }

    public NativeSurfaceView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        this.context= context;
        getHolder().addCallback(this);
    }

    public NativeSurfaceView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        this.context= context;
        getHolder().addCallback(this);
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {
        NativeOpengl.setAssetManager(context.getAssets());
        NativeOpengl.surfaceCreate(surfaceHolder.getSurface());
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
        NativeOpengl.surfaceChange(width,height);
    }


    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {
        NativeOpengl.surfaceDestroy();
    }
}
