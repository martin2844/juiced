package org.openjuice.android;

import android.os.Bundle;
import android.view.InputDevice;
import android.view.View;
import android.view.ViewGroup;
import org.libsdl.app.SDLActivity;

public final class JuicedActivity extends SDLActivity {
  private static native void nativeTouchAction(int action, boolean down);

  private java.io.RandomAccessFile installLockFile;
  private java.nio.channels.FileLock installLock;

  public void openControllerSettings() {
    runOnUiThread(() -> startActivity(new android.content.Intent(this, ControllerActivity.class)));
  }

  private TouchView touchLabels;
  private TouchConfig touchConfig;
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
    if (touchLabels != null) {
      touchLabels.clear();
      boolean show = touchConfig.visibility == 1 || (touchConfig.visibility == 0 && !controller);
      touchLabels.setVisibility(show ? View.VISIBLE : View.GONE);
    }
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
    if (mLayout != null && !"diagnostics".equals(getIntent().getStringExtra("mode"))) {
      touchConfig = TouchConfig.load(getFilesDir());
      touchLabels =
          new TouchView(
              this,
              touchConfig,
              false,
              (key, down) -> {
                if (key >= 1000) nativeTouchAction(key - 1000, down);
                else if (down) onNativeKeyDown(key);
                else onNativeKeyUp(key);
              });
      mLayout.addView(touchLabels, new ViewGroup.LayoutParams(-1, -1));
      inputManager = (android.hardware.input.InputManager) getSystemService(INPUT_SERVICE);
      inputManager.registerInputDeviceListener(devices, new android.os.Handler(getMainLooper()));
      updateLabels();
    }
  }

  public void setTouchDriving(boolean driving) {
    runOnUiThread(
        () -> {
          if (touchLabels != null) touchLabels.driving(driving);
        });
  }

  @Override
  protected void onPause() {
    if (touchLabels != null) touchLabels.clear();
    super.onPause();
  }

  @Override
  protected void onResume() {
    super.onResume();
    if (touchLabels != null) {
      touchConfig = TouchConfig.load(getFilesDir());
      touchLabels.config = touchConfig;
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
        && "garage-check".equals(mode))
      return new String[] {root, "--screen", "instant_selcar", "--frames", "360", "--capture", root + "/android-garage.bmp"};
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
