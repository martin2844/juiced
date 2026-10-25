package org.openjuice.android;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.os.Bundle;
import android.view.InputDevice;
import android.view.View;
import android.view.ViewGroup;
import org.libsdl.app.SDLActivity;

public final class JuicedActivity extends SDLActivity {
  private java.io.RandomAccessFile installLockFile;
  private java.nio.channels.FileLock installLock;

  public void openControllerSettings() {
    runOnUiThread(() -> startActivity(new android.content.Intent(this, ControllerActivity.class)));
  }

  private View touchLabels;
  private android.hardware.input.InputManager inputManager;
  private final android.hardware.input.InputManager.InputDeviceListener devices =
      new android.hardware.input.InputManager.InputDeviceListener() {
        public void onInputDeviceAdded(int id) {
          updateLabels();
        }

        public void onInputDeviceRemoved(int id) {
          updateLabels();
        }

        public void onInputDeviceChanged(int id) {
          updateLabels();
        }
      };

  private void updateLabels() {
    boolean controller = false;
    for (int id : InputDevice.getDeviceIds()) {
      InputDevice d = InputDevice.getDevice(id);
      if (d != null
          && d.getMotionRange(android.view.MotionEvent.AXIS_X, InputDevice.SOURCE_JOYSTICK) != null
          && d.getMotionRange(android.view.MotionEvent.AXIS_Y, InputDevice.SOURCE_JOYSTICK) != null)
        controller = true;
    }
    if (touchLabels != null) touchLabels.setVisibility(controller ? View.GONE : View.VISIBLE);
  }

  private volatile java.util.concurrent.CountDownLatch imageWait;
  private volatile String imageResult;
  private java.io.File imageFolder;
  private boolean imagePortrait;

  public String importImage(String folder, boolean portrait) {
    imageFolder = new java.io.File(folder);
    imageFolder.mkdirs();
    imagePortrait = portrait;
    imageResult = "No image selected.";
    java.util.concurrent.CountDownLatch wait = new java.util.concurrent.CountDownLatch(1);
    imageWait = wait;
    runOnUiThread(
        () -> {
          try {
            android.content.Intent i =
                new android.content.Intent(android.content.Intent.ACTION_OPEN_DOCUMENT);
            i.setType("image/*");
            i.addCategory(android.content.Intent.CATEGORY_OPENABLE);
            startActivityForResult(i, 42);
          } catch (Exception e) {
            imageResult = e.getMessage();
            wait.countDown();
          }
        });
    try {
      wait.await();
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
      return "Image import cancelled.";
    } finally {
      imageWait = null;
    }
    return imageResult;
  }

  @Override
  protected void onActivityResult(int request, int result, android.content.Intent data) {
    super.onActivityResult(request, result, data);
    if (request != 42) return;
    final java.util.concurrent.CountDownLatch wait = imageWait;
    if (wait == null) return;
    if (result != RESULT_OK || data == null || data.getData() == null) {
      wait.countDown();
      return;
    }
    final android.net.Uri uri = data.getData();
    new Thread(
            () -> {
              try {
                imageResult =
                    ImageImport.decode(getContentResolver(), uri, imageFolder, imagePortrait);
              } catch (Exception | OutOfMemoryError e) {
                imageResult = "Cannot import image: " + e.getMessage();
              } finally {
                wait.countDown();
              }
            },
            "Image import")
        .start();
  }

  @Override
  protected void onDestroy() {
    if (imageWait != null) imageWait.countDown();
    if (inputManager != null) inputManager.unregisterInputDeviceListener(devices);
    super.onDestroy();
    try {
      if (installLock != null) installLock.release();
      if (installLockFile != null) installLockFile.close();
    } catch (java.io.IOException ignored) {
    }
    // The engine owns process-wide singletons. Finish its dedicated process after
    // SDL has joined the native thread, so the next Play starts with fresh state.
    if (isFinishing()) android.os.Process.killProcess(android.os.Process.myPid());
  }

  @Override
  protected void onCreate(Bundle state) {
    try {
      installLockFile =
          new java.io.RandomAccessFile(new java.io.File(getFilesDir(), "installation.lock"), "rw");
      installLock = installLockFile.getChannel().tryLock();
      if (installLock == null) throw new java.io.IOException("Import in progress");
    } catch (java.io.IOException e) {
      super.onCreate(state);
      getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
      finish();
      return;
    }
    super.onCreate(state);
    getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    boolean controller = false;
    for (int id : InputDevice.getDeviceIds()) {
      InputDevice device = InputDevice.getDevice(id);
      if (device != null
          && (device.supportsSource(InputDevice.SOURCE_GAMEPAD)
              || device.supportsSource(InputDevice.SOURCE_JOYSTICK))) controller = true;
    }
    if (mLayout != null && !"diagnostics".equals(getIntent().getStringExtra("mode"))) {
      View labels =
          new View(this) {
            final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

            @Override
            protected void onDraw(Canvas canvas) {
              float width = getWidth(), height = getHeight();
              float[][] boxes = {
                {0, .65f, .18f, 1},
                {.18f, .65f, .36f, 1},
                {.64f, .65f, .82f, 1},
                {.82f, .65f, 1, 1},
                {0, 0, .18f, .2f},
                {.82f, 0, 1, .2f},
                {.82f, .22f, 1, .40f},
                {.64f, .22f, .82f, .40f},
                {.18f, .22f, .36f, .40f},
                {0, .22f, .18f, .40f},
                {0, .42f, .18f, .60f},
                {.82f, .42f, 1, .60f}
              };
              String[] names = {
                "LEFT",
                "RIGHT",
                "BRAKE / DOWN",
                "GAS / UP",
                "BACK / PAUSE",
                "SELECT",
                "NOS",
                "HANDBRAKE",
                "GEAR +",
                "GEAR -",
                "CAMERA",
                "REVERSE"
              };
              paint.setTextSize(12 * getResources().getDisplayMetrics().scaledDensity);
              paint.setTextAlign(Paint.Align.CENTER);
              for (int i = 0; i < boxes.length; i++) {
                float[] b = boxes[i];
                paint.setColor(0x45FFFFFF);
                paint.setStyle(Paint.Style.STROKE);
                paint.setStrokeWidth(2);
                canvas.drawRoundRect(
                    b[0] * width + 8,
                    b[1] * height + 8,
                    b[2] * width - 8,
                    b[3] * height - 8,
                    16,
                    16,
                    paint);
                paint.setColor(0xAAFFFFFF);
                paint.setStyle(Paint.Style.FILL);
                canvas.drawText(
                    names[i], (b[0] + b[2]) * width / 2, (b[1] + b[3]) * height / 2, paint);
              }
            }
          };
      touchLabels = labels;
      labels.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
      mLayout.addView(labels, new ViewGroup.LayoutParams(-1, -1));
      inputManager = (android.hardware.input.InputManager) getSystemService(INPUT_SERVICE);
      inputManager.registerInputDeviceListener(devices, new android.os.Handler(getMainLooper()));
      updateLabels();
    }
  }

  @Override
  public void onBackPressed() {
    if (mBrokenLibraries) {
      super.onBackPressed();
      return;
    }
    onNativeKeyDown(android.view.KeyEvent.KEYCODE_ESCAPE);
    new android.os.Handler(getMainLooper())
        .postDelayed(() -> onNativeKeyUp(android.view.KeyEvent.KEYCODE_ESCAPE), 50);
  }

  @Override
  protected String[] getLibraries() {
    return new String[] {"SDL3", "main"};
  }

  @Override
  protected String[] getArguments() {
    String mode = getIntent().getStringExtra("mode");
    String root = getFilesDir().getAbsolutePath();
    if ("race".equals(mode)) return new String[] {root, "-i"};
    if ("diagnostics".equals(mode)) return new String[] {root, "--diagnostics"};
    if ("stock".equals(mode)) return new String[] {root, "--no-mods"};
    if ("mods".equals(mode)) return new String[] {root, "--mods-menu"};
    if ((getApplicationInfo().flags & android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE) != 0
        && "smoke".equals(mode))
      return new String[] {
        root,
        "--track",
        "Angel SuperSpeedway/Speedway_r1",
        "--car",
        getIntent().getStringExtra("car") == null
            ? "FERRARI_458"
            : getIntent().getStringExtra("car"),
        "--autopilot",
        "--frames",
        getIntent().getStringExtra("frames") == null ? "900" : getIntent().getStringExtra("frames"),
        "--capture",
        root + "/android-race.bmp"
      };
    return new String[] {root};
  }
}
