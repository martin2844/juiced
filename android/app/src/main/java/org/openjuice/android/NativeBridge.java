package org.openjuice.android;

final class NativeBridge {
  static {
    System.loadLibrary("SDL3");
    System.loadLibrary("main");
  }

  static synchronized native String validate(String game, String mods);

  static synchronized native String saveCatalog(String files, boolean modded)
      throws java.io.IOException;

  static synchronized native String inspectSave(String files, boolean modded, byte[] bytes)
      throws java.io.IOException;

  static synchronized native byte[] editSave(
      String files, boolean modded, byte[] bytes, int money, int[] respect, String[] cars)
      throws java.io.IOException;
}
