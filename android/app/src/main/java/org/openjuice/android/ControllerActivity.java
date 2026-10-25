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
    LinearLayout layout = new LinearLayout(this);
    layout.setOrientation(1);
    layout.setPadding(24, 24, 24, 24);
    TextView title = new TextView(this);
    title.setText("Controller setup");
    title.setTextSize(25);
    layout.addView(title);
    monitor = new TextView(this);
    monitor.setText(
        "Press a button or move a stick. Retroid and Bluetooth gamepads use the same native input"
            + " path.");
    layout.addView(monitor);
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
      TextView t = new TextView(this);
      t.setText(labels[i]);
      layout.addView(t);
      Spinner spinner = new Spinner(this);
      spinner.setAdapter(
          new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, buttons));
      int value = defaults[i];
      try {
        value = Integer.parseInt(config.getProperty(keys[i], "" + value));
      } catch (NumberFormatException ignored) {
      }
      spinner.setSelection(Math.max(0, Math.min(10, value)));
      layout.addView(spinner);
      mappings.add(spinner);
    }
    TextView deadLabel = new TextView(this);
    layout.addView(deadLabel);
    deadzone = new SeekBar(this);
    deadzone.setMax(30);
    int dz = 12;
    try {
      dz = Integer.parseInt(config.getProperty("deadzone", "12"));
    } catch (NumberFormatException ignored) {
    }
    deadzone.setProgress(dz);
    deadLabel.setText("Stick dead zone: " + dz + "%");
    deadzone.setOnSeekBarChangeListener(
        new SeekBar.OnSeekBarChangeListener() {
          public void onProgressChanged(SeekBar s, int v, boolean user) {
            deadLabel.setText("Stick dead zone: " + v + "%");
          }

          public void onStartTrackingTouch(SeekBar s) {}

          public void onStopTrackingTouch(SeekBar s) {}
        });
    layout.addView(deadzone);
    Button save = new Button(this);
    save.setText("Save controls");
    layout.addView(save);
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
    ScrollView scroll = new ScrollView(this);
    scroll.addView(layout);
    setContentView(scroll);
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
