package com.sachk.spool;

import android.os.Build;
import android.os.Bundle;
import org.qtproject.qt.android.bindings.QtActivity;

/**
 * The activity exists only to take the launch screen's exit into our own hands.
 *
 * From Android 12 the platform animates its launch frame away while the
 * activity's window comes up behind it. Both frames here are the same picture,
 * so that animation cross-fades an image with itself: two half-transparent
 * copies never add back to one, and the mark visibly dims and returns. There is
 * nothing to reveal, so there is nothing to animate -- removing the platform's
 * frame the moment it offers hands over on a single frame instead.
 */
public class SpoolActivity extends QtActivity {
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // Registered before the base class starts anything, so the platform
            // never gets as far as playing its own exit.
            getSplashScreen().setOnExitAnimationListener(view -> view.remove());
        }
        super.onCreate(savedInstanceState);
    }
}
