package com.sachk.spool;

import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;
import android.util.Log;
import androidx.core.content.FileProvider;
import java.io.File;

public final class AndroidUpdateBridge {
    private static final String TAG = "SpoolUpdate";
    private static final String APK_MIME_TYPE = "application/vnd.android.package-archive";

    private AndroidUpdateBridge() { }

    public static boolean canRequestPackageInstalls(Context context)
    {
        return context != null && context.getPackageManager().canRequestPackageInstalls();
    }

    public static boolean openInstallSettings(Context context)
    {
        if (context == null)
            return false;
        Intent intent
            = new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:" + context.getPackageName()));
        return startResolvedActivity(context, intent);
    }

    public static boolean installApk(Context context, String path)
    {
        if (context == null || path == null)
            return false;
        try {
            File apk = new File(path);
            if (!apk.isFile())
                return false;
            Uri uri = FileProvider.getUriForFile(context, context.getPackageName() + ".qtprovider", apk);
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setDataAndType(uri, APK_MIME_TYPE);
            intent.setClipData(ClipData.newRawUri("Spool update", uri));
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            return startResolvedActivity(context, intent);
        } catch (RuntimeException exception) {
            Log.e(TAG, "Could not launch APK installer", exception);
            return false;
        }
    }

    private static boolean startResolvedActivity(Context context, Intent intent)
    {
        try {
            if (intent.resolveActivity(context.getPackageManager()) == null)
                return false;
            if (!(context instanceof Activity))
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
            return true;
        } catch (RuntimeException exception) {
            Log.e(TAG, "Could not launch Android activity", exception);
            return false;
        }
    }
}
