package com.margelo.nitro.transcoder;

import androidx.annotation.Nullable;

import com.facebook.react.BaseReactPackage;
import com.facebook.react.bridge.NativeModule;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.module.model.ReactModuleInfoProvider;

import java.util.HashMap;

public class NitroTranscoderPackage extends BaseReactPackage {
  @Nullable
  @Override
  public NativeModule getModule(String name, ReactApplicationContext reactContext) {
    return null;
  }

  @Override
  public ReactModuleInfoProvider getReactModuleInfoProvider() {
    return () -> new HashMap<>();
  }

  static {
    NitroTranscoderOnLoad.initializeNative();
  }
}
