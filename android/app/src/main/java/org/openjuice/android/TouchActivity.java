package org.openjuice.android;

import android.app.*;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.*;
import android.widget.*;

public final class TouchActivity extends Activity {
  private TouchConfig config;
  private TouchView preview;
  private Spinner action;
  private TextView selected;

  @Override
  public void onCreate(Bundle b) {
    super.onCreate(b);
    config = TouchConfig.load(getFilesDir());
    render();
  }

  @Override
  public void onConfigurationChanged(Configuration c) {
    super.onConfigurationChanged(c);
    render();
  }

  private void render() {
    LinearLayout root =
        Ui.editor(
            this,
            "Touch controls",
            () -> {
              try {
                config.save(getFilesDir());
                finish();
              } catch (Exception e) {
                new AlertDialog.Builder(this)
                    .setMessage(e.getMessage())
                    .setPositiveButton("Close", null)
                    .show();
              }
            });
    boolean wide = getResources().getConfiguration().screenWidthDp >= 600;
    LinearLayout pane = new LinearLayout(this);
    pane.setOrientation(wide ? 0 : 1);
    root.addView(pane, new LinearLayout.LayoutParams(-1, 0, 1));
    LinearLayout visual = Ui.column(this);
    LinearLayout body = Ui.column(this);
    body.setPadding(Ui.dp(this, wide ? 20 : 0), Ui.dp(this, 12), 0, 0);
    ScrollView scroll = new ScrollView(this);
    scroll.addView(body);
    if (wide) {
      pane.addView(visual, new LinearLayout.LayoutParams(0, -1, 1.65f));
      pane.addView(scroll, new LinearLayout.LayoutParams(0, -1, 1));
    } else {
      pane.addView(scroll, new LinearLayout.LayoutParams(-1, -1));
      body.addView(visual);
    }
    Ui.add(
        body,
        Ui.label(
            this,
            "Choose when controls appear. Tap a driving button to remap it, or drag it to a"
                + " comfortable position.",
            14,
            Ui.MUTED));
    Ui.add(body, Ui.label(this, "Show on-screen controls", 17, Ui.INK));
    Spinner visibility = Ui.spinner(this, new String[] {"Auto (no gamepad)", "Always", "Off"});
    visibility.setSelection(config.visibility);
    visibility.setOnItemSelectedListener(
        new AdapterView.OnItemSelectedListener() {
          public void onItemSelected(AdapterView<?> p, View v, int n, long id) {
            config.visibility = n;
          }

          public void onNothingSelected(AdapterView<?> p) {}
        });
    Ui.add(body, visibility);
    LinearLayout modes = new LinearLayout(this);
    Button drive =
        Ui.button(
            this,
            "Driving layout",
            true,
            () -> {
              preview.driving(true);
              action.setEnabled(preview.selected >= 0);
            });
    Button menu =
        Ui.button(
            this,
            "Menu layout",
            false,
            () -> {
              preview.driving(false);
              action.setEnabled(false);
              selected.setText("Menu navigation stays available with every driving layout.");
            });
    drive.setOnClickListener(
        v -> {
          preview.driving(true);
          Ui.style(drive, true);
          Ui.style(menu, false);
          action.setEnabled(preview.selected >= 0);
          selected.setText("Tap a driving button to change its action. Drag to move it.");
        });
    menu.setOnClickListener(
        v -> {
          preview.driving(false);
          Ui.style(drive, false);
          Ui.style(menu, true);
          action.setEnabled(false);
          selected.setText("Menu navigation stays available with every driving layout.");
        });
    modes.addView(drive, new LinearLayout.LayoutParams(0, -2, 1));
    modes.addView(menu, new LinearLayout.LayoutParams(0, -2, 1));
    Ui.add(visual, modes);
    preview = new TouchView(this, config, true, (k, d) -> {});
    visual.addView(
        preview, new LinearLayout.LayoutParams(-1, wide ? 0 : Ui.dp(this, 220), wide ? 1 : 0));
    selected =
        Ui.label(this, "Tap a driving button to change its action. Drag to move it.", 15, Ui.MUTED);
    Ui.add(body, selected);
    action = Ui.spinner(this, TouchConfig.ACTIONS);
    action.setEnabled(false);
    Ui.add(body, action);
    preview.selection =
        i -> {
          action.setEnabled(i >= 0);
          if (i >= 0) {
            selected.setText("Button " + (i + 1) + " · " + TouchConfig.ACTIONS[config.actions[i]]);
            action.setSelection(config.actions[i]);
          }
        };
    action.setOnItemSelectedListener(
        new AdapterView.OnItemSelectedListener() {
          public void onItemSelected(AdapterView<?> p, View v, int n, long id) {
            if (preview.selected >= 0 && preview.driving) {
              config.actions[preview.selected] = n;
              selected.setText("Button " + (preview.selected + 1) + " · " + TouchConfig.ACTIONS[n]);
              preview.invalidate();
            }
          }

          public void onNothingSelected(AdapterView<?> p) {}
        });
    slider(
        body,
        "Button size",
        75,
        135,
        Math.round(config.size * 100),
        n -> {
          config.size = n / 100f;
          preview.invalidate();
        });
    slider(
        body,
        "Opacity",
        25,
        90,
        Math.round(config.opacity * 100),
        n -> {
          config.opacity = n / 100f;
          preview.invalidate();
        });
    Ui.add(
        body,
        Ui.button(
            this,
            "Reset touch layout",
            false,
            () -> {
              new AlertDialog.Builder(this)
                  .setTitle("Reset touch controls?")
                  .setMessage("Restore the default positions, actions, size and opacity.")
                  .setNegativeButton("Cancel", null)
                  .setPositiveButton(
                      "Reset",
                      (d, w) -> {
                        int visibilityMode = config.visibility;
                        config = new TouchConfig();
                        config.visibility = visibilityMode;
                        render();
                      })
                  .show();
            }));
  }

  private void slider(
      LinearLayout body,
      String label,
      int min,
      int max,
      int value,
      java.util.function.IntConsumer change) {
    TextView text = Ui.label(this, label + ": " + value + "%", 16, Ui.INK);
    body.addView(text);
    SeekBar bar = new SeekBar(this);
    bar.setMax(max - min);
    bar.setProgress(value - min);
    bar.setContentDescription(label);
    bar.setProgressTintList(android.content.res.ColorStateList.valueOf(Ui.ORANGE));
    bar.setThumbTintList(android.content.res.ColorStateList.valueOf(Ui.ORANGE));
    bar.setOnSeekBarChangeListener(
        new SeekBar.OnSeekBarChangeListener() {
          public void onProgressChanged(SeekBar s, int v, boolean user) {
            text.setText(label + ": " + (v + min) + "%");
            change.accept(v + min);
          }

          public void onStartTrackingTouch(SeekBar s) {}

          public void onStopTrackingTouch(SeekBar s) {}
        });
    Ui.add(body, bar);
  }
}
