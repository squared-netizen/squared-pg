package dev.squarednetizen.generated;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;

import {{PACKAGE_NAME}}.BuildConfig;

import org.libsdl.app.SDLActivity;

public final class SquaredActivity extends SDLActivity {
    private static final String PREFERENCES = "squared.android.special_access";
    private static final String REQUESTED_ALL_FILES = "requested_all_files";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestAllFilesAccessOnce();
    }

    private void requestAllFilesAccessOnce() {
        if (!BuildConfig.MANAGE_ALL_FILES_ON_FIRST_START ||
                android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.R ||
                Environment.isExternalStorageManager()) {
            return;
        }

        SharedPreferences preferences = getSharedPreferences(
            PREFERENCES,
            MODE_PRIVATE
        );
        if (preferences.getBoolean(REQUESTED_ALL_FILES, false)) {
            return;
        }

        boolean launched = launchSettings(new Intent(
            Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
            Uri.parse("package:" + getPackageName())
        ));
        if (!launched) {
            launched = launchSettings(new Intent(
                Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION
            ));
        }
        if (launched) {
            preferences.edit().putBoolean(REQUESTED_ALL_FILES, true).commit();
        }
    }

    private boolean launchSettings(Intent intent) {
        try {
            startActivity(intent);
            return true;
        } catch (ActivityNotFoundException exception) {
            return false;
        }
    }
}
