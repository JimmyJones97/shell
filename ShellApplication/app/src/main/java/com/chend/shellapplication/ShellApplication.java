package com.chend.shellapplication;

import android.app.Application;

public class ShellApplication extends Application {
    @Override
    public void onCreate() {
        System.loadLibrary("shell");
    }
}
