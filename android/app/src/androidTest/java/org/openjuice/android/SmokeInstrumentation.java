package org.openjuice.android;

import android.app.*;
import android.graphics.*;
import android.net.Uri;
import android.os.Bundle;
import java.io.*;
import java.nio.*;
import java.nio.file.*;

public final class SmokeInstrumentation extends Instrumentation {
  private String mode;

  private void check(boolean value, String message) {
    if (!value) throw new AssertionError(message);
    Bundle b = new Bundle();
    b.putString("stream", "PASS: " + message + "\n");
    sendStatus(0, b);
  }

  @Override
  public void onCreate(Bundle args) {
    super.onCreate(args);
    mode = args == null ? null : args.getString("mode");
    start();
  }

  @Override
  public void onStart() {
    Bundle result = new Bundle();
    try {
      if ("save-editor".equals(mode)) {
        SaveEditorInstrumentedTest.run(getTargetContext().getFilesDir());
        result.putString(
            "stream",
            "PASS: real-save clone edit, money/car/respect roundtrip, backup, stale edit rejection,"
                + " restore, original untouched\n");
        finishResult(Activity.RESULT_OK, result);
        return;
      }
      if ("touch".equals(mode) || "touch-race".equals(mode)) {
        TouchInstrumentedTest.views(this);
        if ("touch-race".equals(mode)) TouchInstrumentedTest.race(this);
        result.putString(
            "stream",
            ("touch-race".equals(mode) ? "PASS: live race Go, acceleration, steering, partial release, braking and neutral; " : "PASS: ")
                + "touch view multitouch, partial release, cancellation, menu transition and menu"
                + " key isolation\n");
        finishResult(Activity.RESULT_OK, result);
        return;
      }
      File files = getTargetContext().getFilesDir(),
          game = new File(files, "game"),
          mods = new File(files, "mods");
      String base = NativeBridge.validate(game.getPath(), "");
      check(base.startsWith("OK:"), "stock native data: " + base);
      String modded = NativeBridge.validate(game.getPath(), mods.getPath());
      check(modded.startsWith("OK:"), "modded native data: " + modded);
      // The added-car packs have documented contiguous stable IDs: Ferrari 53,
      // Street Cars 54..56, Wave 2 57..63. A suffix without its prerequisite is
      // deliberately invalid; testing it as a successful catalogue hides the contract.
      String[] ids = {"ferrari", "street-cars", "street-cars-wave2", "wheels"};
      boolean[] disabled = new boolean[ids.length];
      for (int i = 0; i < ids.length; ++i)
        disabled[i] = new File(mods, ids[i] + "/disabled").exists();
      boolean fullCarPack = new File(mods, "street-cars/mod.ini").isFile()
          && new File(mods, "street-cars-wave2/mod.ini").isFile();
      try {
        int[] counts = {53, 54, 57, 64};
        for (int prefix = 0; prefix <= (fullCarPack ? 3 : 1); ++prefix)
          for (boolean wheels : new boolean[] {false, true}) {
            for (int i = 0; i < 3; ++i)
              if (new File(mods, ids[i] + "/mod.ini").isFile())
                enabled(mods, ids[i], prefix > i);
            enabled(mods, "wheels", wheels);
            String value = NativeBridge.validate(game.getPath(), mods.getPath());
            check(value.startsWith("OK: " + counts[prefix] + " cars loaded."),
                "car-prefix=" + prefix + ", wheels=" + wheels + ": " + value);
          }
        if (fullCarPack) {
          enabled(mods, "ferrari", false);
          String rejected = NativeBridge.validate(game.getPath(), mods.getPath());
          check(rejected.startsWith("ERROR:"), "missing car-pack prerequisite rejected: " + rejected);
        }
      } finally {
        for (int i = 0; i < ids.length; ++i)
          if (new File(mods, ids[i] + "/mod.ini").isFile()) enabled(mods, ids[i], !disabled[i]);
      }
      File picture = new File(files, "test-image.png"),
          output = new File(files, "image-check/images");
      output.mkdirs();
      Bitmap bitmap = Bitmap.createBitmap(8, 4, Bitmap.Config.ARGB_8888);
      bitmap.eraseColor(Color.RED);
      try (OutputStream out = new FileOutputStream(picture)) {
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
      }
      bitmap.recycle();
      String name =
          ImageImport.decode(
              getTargetContext().getContentResolver(), Uri.fromFile(picture), output, false);
      byte[] data = Files.readAllBytes(new File(output, name + ".ddt").toPath());
      ByteBuffer b = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
      check(
          data.length == 44 + 512 * 512 * 4 && b.getInt(4) == 512 && b.getInt(8) == 512,
          "native image dimensions and DDT length");
      check(b.getInt(44) == 0, "logo transparent padding");
      check(b.getInt(44 + (256 * 512 + 256) * 4) == 0xffff0000, "image red channel preserved");
      ModStore.delete(output.getParentFile());
      picture.delete();
      result.putString(
          "stream", "Android native catalogues, package combinations and image import passed.\n");
      finishResult(Activity.RESULT_OK, result);
    } catch (Throwable e) {
      result.putString("stream", "FAIL: " + android.util.Log.getStackTraceString(e));
      finishResult(Activity.RESULT_CANCELED, result);
    }
  }

  private void finishResult(int code, Bundle result) {
    // Keep a device-side receipt even if the host's ADB connection is interrupted.
    try {
      String name = mode != null && mode.matches("[a-z-]+") ? mode : "catalogues";
      Files.write(new File(getTargetContext().getFilesDir(), "instrumentation-" + name + ".txt").toPath(),
          ("result=" + code + "\n" + result.getString("stream", "")).getBytes(java.nio.charset.StandardCharsets.UTF_8));
    } catch (IOException e) {
      throw new RuntimeException("Cannot preserve instrumentation result", e);
    }
    finish(code, result);
  }

  private static void enabled(File mods, String id, boolean enabled) throws IOException {
    File marker = new File(mods, id + "/disabled");
    if (enabled) Files.deleteIfExists(marker.toPath());
    else if (!marker.exists()) Files.createFile(marker.toPath());
  }
}
