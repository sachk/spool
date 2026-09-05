package com.sachk.spool;

import android.app.Activity;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;

/**
 * The video plane, underneath everything Qt draws.
 *
 * Direct playback hands MediaCodec a Surface of its own and lets the system
 * compositor put the interface on top, rather than decoding into a texture and
 * asking the GPU to scale, tone-map and composite every frame through the Qt
 * scene graph. On a television box that second path saturates the GPU badly
 * enough that even the player's own controls stutter.
 *
 * Ordering is the whole trick. This view is added at index 0 so it sits below
 * Qt's surface, and it must not call setZOrderOnTop: that would raise it above
 * the window and hide the interface behind the video. Qt's own surface is
 * raised instead, by giving its window Qt::WindowStaysOnTopHint, which is what
 * makes Qt ask for setZOrderMediaOverlay.
 */
public final class VideoSurfaceBridge {
    private static SurfaceView view;

    private VideoSurfaceBridge()
    {
    }

    /** Called on the Qt thread; the view work is posted to the UI thread. */
    public static void show(Activity activity)
    {
        activity.runOnUiThread(() -> {
            if (view != null)
                return;
            SurfaceView created = new SurfaceView(activity);
            // Never setZOrderOnTop: see the class comment.
            created.setZOrderMediaOverlay(false);
            created.setClickable(false);
            created.setFocusable(false);
            created.getHolder().addCallback(new SurfaceHolder.Callback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder)
                {
                    nativeSurfaceReady(holder.getSurface());
                }

                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
                {
                    nativeSurfaceResized(width, height);
                }

                @Override
                public void surfaceDestroyed(SurfaceHolder holder)
                {
                    // Once this returns the Surface is gone, so whoever is
                    // drawing into it has to have let go before then.
                    nativeSurfaceLost();
                }
            });
            ViewGroup content = activity.findViewById(android.R.id.content);
            content.addView(created, 0,
                new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            view = created;
        });
    }

    public static void hide(Activity activity)
    {
        activity.runOnUiThread(() -> {
            if (view == null)
                return;
            ViewGroup content = activity.findViewById(android.R.id.content);
            content.removeView(view);
            view = null;
        });
    }

    private static native void nativeSurfaceReady(Object surface);
    private static native void nativeSurfaceResized(int width, int height);
    private static native void nativeSurfaceLost();
}
