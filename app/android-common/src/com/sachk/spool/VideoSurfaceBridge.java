package com.sachk.spool;

import android.app.Activity;
import android.view.Gravity;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.FrameLayout;

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
    private static VideoSurface view;

    private VideoSurfaceBridge()
    {
    }

    /**
     * A surface shaped like the video in it.
     *
     * Nothing scales the picture on this path: MediaCodec fills the whole
     * Surface, and the Surface is whatever size the view was laid out at. So
     * a view that filled the screen would stretch every video that is not
     * exactly the panel's shape. Letterboxing is this view's job, and it is
     * the only place it can be done.
     */
    private static final class VideoSurface extends SurfaceView {
        private int videoWidth;
        private int videoHeight;

        VideoSurface(Activity activity)
        {
            super(activity);
        }

        void setVideoSize(int width, int height)
        {
            if (width == videoWidth && height == videoHeight)
                return;
            videoWidth = width;
            videoHeight = height;
            requestLayout();
        }

        @Override
        protected void onMeasure(int widthSpec, int heightSpec)
        {
            int width = MeasureSpec.getSize(widthSpec);
            int height = MeasureSpec.getSize(heightSpec);
            if (videoWidth > 0 && videoHeight > 0 && width > 0 && height > 0) {
                // Long multiplication rather than a float ratio, so a 4K frame
                // in a 1080p window lands on the same answer either way round.
                if ((long) width * videoHeight > (long) height * videoWidth)
                    width = (int) ((long) height * videoWidth / videoHeight);
                else
                    height = (int) ((long) width * videoHeight / videoWidth);
            }
            setMeasuredDimension(width, height);
        }
    }

    /** Called on the Qt thread; the view work is posted to the UI thread. */
    public static void show(Activity activity)
    {
        activity.runOnUiThread(() -> {
            if (view != null)
                return;
            VideoSurface created = new VideoSurface(activity);
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
            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT, Gravity.CENTER);
            content.addView(created, 0, params);
            view = created;
        });
    }

    /** The video's display size, so the plane can be letterboxed to match. */
    public static void setVideoSize(Activity activity, int width, int height)
    {
        activity.runOnUiThread(() -> {
            if (view != null)
                view.setVideoSize(width, height);
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
