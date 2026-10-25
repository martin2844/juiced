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
      if ("touch".equals(mode) || "touch-race".equals(mode)) {
        TouchInstrumentedTest.views(this);
        if ("touch-race".equals(mode)) TouchInstrumentedTest.race(this);
        result.putString(
            "stream",
            "PASS: touch view multitouch, partial release, cancellation, menu transition and menu"
                + " key isolation\n");
        finish(Activity.RESULT_OK, result);
        return;
      }
      File files = getTargetContext().getFilesDir(),
          game = new File(files, "game"),
          mods = new File(files, "mods");
      String base = NativeBridge.validate(game.getPath(), "");
      check(base.startsWith("OK:"), "stock native data: " + base);
      String modded = NativeBridge.validate(game.getPath(), mods.getPath());
      check(modded.startsWith("OK:"), "modded native data: " + modded);
      for (boolean ferrari : new boolean[] {false, true})
        for (boolean wheels : new boolean[] {false, true}) {
          File f = new File(mods, "ferrari/disabled"), w = new File(mods, "wheels/disabled");
          boolean oldF = f.exists(), oldW = w.exists();
          try {
            if (ferrari) f.delete();
            else f.createNewFile();
            if (wheels) w.delete();
            else w.createNewFile();
            String value = NativeBridge.validate(game.getPath(), mods.getPath());
            check(
                value.startsWith("OK:"),
                "Ferrari=" + ferrari + ", wheels=" + wheels + ": " + value);
          } finally {
            if (oldF) f.createNewFile();
            else f.delete();
            if (oldW) w.createNewFile();
            else w.delete();
          }
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
      finish(Activity.RESULT_OK, result);
    } catch (Throwable e) {
      result.putString("stream", "FAIL: " + android.util.Log.getStackTraceString(e));
      finish(Activity.RESULT_CANCELED, result);
    }
  }
}
