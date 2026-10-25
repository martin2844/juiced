package org.openjuice.android;

final class NativeBridge {
  static {
    System.loadLibrary("SDL3");
    System.loadLibrary("main");
  }

  static native String validate(String game, String mods);
}
