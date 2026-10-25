package org.openjuice.android;

import android.app.Activity;
import android.os.Bundle;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.util.*;

public final class ControllerActivity extends Activity {
  private TextView monitor;
  private final List<Spinner> mappings = new ArrayList<>();
  private SeekBar deadzone;
  private final String[] keys = {
    "select",
    "back",
    "action1",
    "action2",
    "leftstick",
    "rightstick",
    "pause",
    "leftshoulder",
    "rightshoulder"
  };
  private final String[] labels = {
    "Select / south",
    "Back / east",
    "Action / north",
    "Action / west",
    "Left stick click",
    "Right stick click",
    "Pause",
    "Left shoulder",
    "Right shoulder"
  };
  private final int[] defaults = {0, 1, 3, 2, 7, 8, 6, 9, 10};

  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    LinearLayout root = Ui.editor(this, "Gamepad setup", null);
    boolean wide = getResources().getConfiguration().screenWidthDp >= 600;
    LinearLayout pane = new LinearLayout(this);
    pane.setOrientation(wide ? 0 : 1);
    root.addView(pane, new LinearLayout.LayoutParams(-1, 0, 1));
    LinearLayout testing = Ui.column(this);
    testing.setPadding(0, Ui.dp(this, 16), Ui.dp(this, wide ? 24 : 0), 0);
    ScrollView scroll = new ScrollView(this);
    if (wide) {
      pane.addView(testing, new LinearLayout.LayoutParams(0, -1, 1));
      pane.addView(scroll, new LinearLayout.LayoutParams(0, -1, 1.5f));
    } else pane.addView(scroll, new LinearLayout.LayoutParams(-1, -1));
    LinearLayout layout = Ui.column(this);
    layout.setPadding(0, Ui.dp(this, 16), 0, Ui.dp(this, 16));
    scroll.addView(layout);
    if (!wide) layout.addView(testing);
    monitor = Ui.label(this, "Press a button or move a stick to test your gamepad.", 15, Ui.WHITE);
    monitor.setPadding(Ui.dp(this, 16), Ui.dp(this, 16), Ui.dp(this, 16), Ui.dp(this, 16));
    monitor.setMinHeight(Ui.dp(this, 100));
    monitor.setBackground(Ui.background(this, Ui.INK, 0));
    Ui.add(testing, monitor);
    Ui.add(
        testing,
        Ui.label(
            this,
            "Left stick steers. RT accelerates; LT brakes. Assign the remaining buttons here.",
            15,
            Ui.MUTED));
    Ui.add(layout, Ui.label(this, "Button assignments", 22, Ui.INK));
    Properties config = new Properties();
    try (FileReader r = new FileReader(new File(getFilesDir(), "controller.ini"))) {
      config.load(r);
    } catch (IOException ignored) {
    }
    String[] buttons = {
      "South (A)",
      "East (B)",
      "West (X)",
      "North (Y)",
      "Back / Select",
      "Guide",
      "Start",
      "Left stick",
      "Right stick",
      "Left shoulder",
      "Right shoulder"
    };
    for (int i = 0; i < keys.length; i++) {
      LinearLayout row = new LinearLayout(this);
      row.setGravity(Gravity.CENTER_VERTICAL);
      TextView t = Ui.label(this, labels[i], 16, Ui.INK);
      row.addView(t, new LinearLayout.LayoutParams(0, -2, 1));
      Spinner spinner = Ui.spinner(this, buttons);
      int value = defaults[i];
      try {
        value = Integer.parseInt(config.getProperty(keys[i], "" + value));
      } catch (NumberFormatException ignored) {
      }
      if (state != null) value = state.getInt(keys[i], value);
      spinner.setSelection(Math.max(0, Math.min(10, value)));
      row.addView(spinner, new LinearLayout.LayoutParams(0, Ui.dp(this, 52), 1));
      Ui.add(layout, row);
      mappings.add(spinner);
    }
    Ui.divider(testing);
    TextView deadLabel = Ui.label(this, "", 17, Ui.INK);
    testing.addView(deadLabel);
    deadzone = new SeekBar(this);
    deadzone.setMax(30);
    int dz = 12;
    try {
      dz = Integer.parseInt(config.getProperty("deadzone", "12"));
    } catch (NumberFormatException ignored) {
    }
    if (state != null) dz = state.getInt("deadzone", dz);
    dz = Math.max(0, Math.min(30, dz));
    deadzone.setProgress(dz);
    deadzone.setProgressTintList(android.content.res.ColorStateList.valueOf(Ui.ORANGE));
    deadzone.setThumbTintList(android.content.res.ColorStateList.valueOf(Ui.ORANGE));
    deadLabel.setText("Stick dead zone: " + dz + "%");
    deadzone.setOnSeekBarChangeListener(
        new SeekBar.OnSeekBarChangeListener() {
          public void onProgressChanged(SeekBar s, int v, boolean user) {
            deadLabel.setText("Stick dead zone: " + v + "%");
          }

          public void onStartTrackingTouch(SeekBar s) {}

          public void onStopTrackingTouch(SeekBar s) {}
        });
    testing.addView(deadzone);
    Button save = Ui.button(this, "Save gamepad settings", true, () -> {});
    Ui.add(root, save);
    save.setOnClickListener(
        v -> {
          try {
            File next = new File(getFilesDir(), "controller.ini.tmp");
            try (PrintWriter out = new PrintWriter(next)) {
              for (int i = 0; i < keys.length; i++)
                out.println(keys[i] + "=" + mappings.get(i).getSelectedItemPosition());
              out.println("deadzone=" + deadzone.getProgress());
            }
            java.nio.file.Files.move(
                next.toPath(),
                new File(getFilesDir(), "controller.ini").toPath(),
                java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            finish();
          } catch (IOException e) {
            monitor.setText(e.getMessage());
          }
        });
  }

  @Override
  protected void onSaveInstanceState(Bundle state) {
    super.onSaveInstanceState(state);
    for (int i = 0; i < keys.length; i++)
      state.putInt(keys[i], mappings.get(i).getSelectedItemPosition());
    state.putInt("deadzone", deadzone.getProgress());
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent e) {
    if (e.getDevice() != null && e.getDevice().supportsSource(InputDevice.SOURCE_GAMEPAD))
      monitor.setText(
          e.getDevice().getName()
              + "\n"
              + KeyEvent.keyCodeToString(e.getKeyCode())
              + (e.getAction() == 0 ? " pressed" : " released"));
    return super.dispatchKeyEvent(e);
  }

  @Override
  public boolean dispatchGenericMotionEvent(MotionEvent e) {
    if ((e.getSource() & InputDevice.SOURCE_JOYSTICK) != 0) {
      monitor.setText(
          String.format(
              java.util.Locale.ROOT,
              "%s\nLeft %.2f / %.2f · Right %.2f / %.2f\nLT %.2f · RT %.2f",
              e.getDevice().getName(),
              e.getAxisValue(0),
              e.getAxisValue(1),
              e.getAxisValue(11),
              e.getAxisValue(14),
              e.getAxisValue(e.getDevice().getMotionRange(17) == null ? 23 : 17),
              e.getAxisValue(e.getDevice().getMotionRange(18) == null ? 22 : 18)));
    }
    return super.dispatchGenericMotionEvent(e);
  }
}
