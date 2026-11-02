package org.openjuice.android;

import android.app.Instrumentation;
import android.graphics.Bitmap;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import java.io.FileOutputStream;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.HashSet;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class TouchInstrumentedTest {
  static MotionEvent event(long start, int action, float[][] points) {
    MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[points.length];
    MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[points.length];
    for (int i = 0; i < points.length; i++) {
      props[i] = new MotionEvent.PointerProperties();
      props[i].id = i;
      props[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
      coords[i] = new MotionEvent.PointerCoords();
      coords[i].x = points[i][0];
      coords[i].y = points[i][1];
      coords[i].pressure = 1;
      coords[i].size = 1;
    }
    return MotionEvent.obtain(
        start,
        SystemClock.uptimeMillis(),
        action,
        points.length,
        props,
        coords,
        0,
        0,
        1,
        1,
        0,
        0,
        InputDevice.SOURCE_TOUCHSCREEN,
        0);
  }

  static void views(Instrumentation test) {
    test.runOnMainSync(
        () -> {
          TouchConfig config = new TouchConfig();
          Set<Integer> held = new HashSet<>();
          TouchView view =
              new TouchView(
                  test.getTargetContext(),
                  config,
                  false,
                  (key, down) -> {
                    if (down) held.add(key);
                    else held.remove(key);
                  });
          view.layout(0, 0, 1920, 1080);
          view.driving(true);
          long start = SystemClock.uptimeMillis();
          dispatch(view, event(start, 0, new float[][] {{1766, 778}}));
          dispatch(view, event(start, 5 | (1 << 8), new float[][] {{1766, 778}, {154, 886}}));
          check(
              held.contains(1002) && held.contains(1000), "accelerate and steer with two pointers");
          dispatch(view, event(start, 6 | (1 << 8), new float[][] {{1766, 778}, {154, 886}}));
          check(held.contains(1002) && !held.contains(1000), "lifting steer retains accelerator");
          dispatch(view, event(start, 3, new float[][] {{1766, 778}}));
          check(held.isEmpty(), "cancel releases pedal");
          dispatch(view, event(start, 0, new float[][] {{1766, 778}}));
          view.driving(false);
          check(held.isEmpty(), "pause layout releases driving input");
          dispatch(view, event(start, 0, new float[][] {{365, 594}}));
          check(held.contains(19) && !held.contains(1002), "menu arrows do not type letters");
          view.clear();
          check(held.isEmpty(), "activity pause releases everything");
        });
  }

  static void dispatch(TouchView view, MotionEvent event) {
    view.dispatchTouchEvent(event);
    event.recycle();
  }

  static void check(boolean condition, String message) {
    if (!condition) throw new AssertionError(message);
  }

  static void race(Instrumentation test) throws Exception {
    File log = new File(test.getTargetContext().getFilesDir(), "Juiced-android.log");
    long launched = System.currentTimeMillis();
    android.content.Intent intent =
        new android.content.Intent(test.getTargetContext(), TestLaunchActivity.class)
            .putExtra("mode", "race")
            .addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK);
    test.getTargetContext().startActivity(intent);
    // Loading and the fly-by are not playable racing. Wait for the engine's actual
    // countdown, then require applied CarBody inputs from newly written telemetry.
    long deadline = SystemClock.uptimeMillis() + 180000;
    while (!log.isFile() || log.lastModified() < launched || !read(log).contains("race: go")) {
      check(SystemClock.uptimeMillis() < deadline, "race reaches Go before touch driving");
      Thread.sleep(500);
    }
    status(test, "race reached Go; testing live player controls");
    StringBuilder receipt = new StringBuilder();
    awaitPlayer(test, log, read(log).length(), receipt, "neutral", p ->
        Math.abs(p[0]) < 1 && Math.abs(p[1]) < .01f && p[2] < .01f && p[3] < .01f);
    int offset = read(log).length();
    long start = SystemClock.uptimeMillis();
    inject(test, event(start, 0, new float[][] {{1766, 778}}));
    awaitPlayer(test, log, offset, receipt, "accelerate", p -> p[0] > 2 && p[2] > .95f && p[3] < .01f);
    screenshot(test, "touch-accelerating.png");
    offset = read(log).length();
    inject(test, event(start, 5 | (1 << 8), new float[][] {{1766, 778}, {403, 886}}));
    awaitPlayer(test, log, offset, receipt, "accelerate and steer", p -> p[1] > .3f && p[2] > .95f);
    screenshot(test, "touch-two-fingers.png");
    offset = read(log).length();
    inject(test, event(start, 6 | (1 << 8), new float[][] {{1766, 778}, {403, 886}}));
    awaitPlayer(test, log, offset, receipt, "release steer, retain accelerator", p ->
        Math.abs(p[1]) < .01f && p[2] > .95f);
    inject(test, event(start, 1, new float[][] {{1766, 778}}));
    offset = read(log).length();
    start = SystemClock.uptimeMillis();
    inject(test, event(start, 0, new float[][] {{1536, 950}}));
    awaitPlayer(test, log, offset, receipt, "brake", p -> p[2] < .01f && p[3] > .95f);
    screenshot(test, "touch-braking.png");
    offset = read(log).length();
    inject(test, event(start, 1, new float[][] {{1536, 950}}));
    awaitPlayer(test, log, offset, receipt, "release all", p ->
        Math.abs(p[1]) < .01f && p[2] < .01f && p[3] < .01f);
    screenshot(test, "touch-released.png");
    Files.write(new File(test.getTargetContext().getFilesDir(), "touch-driving-receipt.txt").toPath(),
        receipt.toString().getBytes(StandardCharsets.UTF_8));
  }

  private static String read(File file) throws Exception {
    return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
  }

  private static final Pattern PLAYER = Pattern.compile(
      "race: car 0 [^\\r\\n]* speed (-?[0-9.]+) steer (-?[0-9.]+) throttle (-?[0-9.]+) brake (-?[0-9.]+)");

  private static void awaitPlayer(Instrumentation test, File log, int offset, StringBuilder receipt,
      String stage, java.util.function.Predicate<float[]> accepted) throws Exception {
    long deadline = SystemClock.uptimeMillis() + 90000;
    while (SystemClock.uptimeMillis() < deadline) {
      String value = read(log);
      check(value.length() >= offset, "race log not replaced during " + stage);
      Matcher m = PLAYER.matcher(value.substring(offset));
      while (m.find()) {
        float[] sample = new float[4];
        for (int i = 0; i < 4; ++i) sample[i] = Float.parseFloat(m.group(i + 1));
        if (accepted.test(sample)) {
          receipt.append(stage).append(": ").append(m.group()).append('\n');
          status(test, stage + ": " + m.group());
          return;
        }
      }
      Thread.sleep(500);
    }
    throw new AssertionError("No new applied player telemetry for " + stage);
  }

  private static void status(Instrumentation test, String message) {
    android.os.Bundle b = new android.os.Bundle();
    b.putString("stream", message + "\n");
    test.sendStatus(0, b);
  }

  static void inject(Instrumentation test, MotionEvent event) {
    try {
      check(test.getUiAutomation().injectInputEvent(event, true), "touch injection accepted");
    } finally {
      event.recycle();
    }
  }

  static void screenshot(Instrumentation test, String name) throws Exception {
    Bitmap image = test.getUiAutomation().takeScreenshot();
    check(
        image != null && image.getWidth() == 1920 && image.getHeight() == 1080,
        "Retroid race capture dimensions");
    if (name.equals("touch-two-fingers.png")) {
      for (int[] point : new int[][] {{1766, 740}, {403, 850}}) {
        int color = image.getPixel(point[0], point[1]);
        check(
            android.graphics.Color.red(color) > android.graphics.Color.green(color) * 1.5f,
            "both touch buttons visibly held");
      }
    }
    try (FileOutputStream out = test.getTargetContext().openFileOutput(name, 0)) {
      image.compress(Bitmap.CompressFormat.PNG, 100, out);
    } finally {
      image.recycle();
    }
  }
}
