package org.openjuice.android;

import android.app.Instrumentation;
import android.graphics.Bitmap;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import java.io.FileOutputStream;
import java.util.HashSet;
import java.util.Set;

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
    android.content.Intent intent =
        new android.content.Intent(test.getTargetContext(), TestLaunchActivity.class)
            .putExtra("mode", "race")
            .addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK);
    test.getTargetContext().startActivity(intent);
    Thread.sleep(20000);
    long start = SystemClock.uptimeMillis();
    inject(test, event(start, 0, new float[][] {{1766, 778}}));
    Thread.sleep(2500);
    inject(test, event(start, 5 | (1 << 8), new float[][] {{1766, 778}, {403, 886}}));
    Thread.sleep(700);
    screenshot(test, "touch-two-fingers.png");
    inject(test, event(start, 6 | (1 << 8), new float[][] {{1766, 778}, {403, 886}}));
    Thread.sleep(1500);
    inject(test, event(start, 1, new float[][] {{1766, 778}}));
    Thread.sleep(300);
    screenshot(test, "touch-released.png");
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
