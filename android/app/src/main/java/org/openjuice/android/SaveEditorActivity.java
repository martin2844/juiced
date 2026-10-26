package org.openjuice.android;

import android.app.*;
import android.os.Bundle;
import android.text.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.util.*;
import org.json.*;

public final class SaveEditorActivity extends Activity {
  private boolean modded = false, busy = false;
  private File profile;
  private JSONObject catalog, saved;
  private byte[] snapshot;
  private String money = "0", tab = "Money";
  private final String[] respect = new String[8];
  private static final String[] LEADERS = {
    "TK", "Biggi", "Sue", "Maria", "Jack", "Melody", "Poppa", "Carlos"
  };
  private final ArrayList<String> added = new ArrayList<>();
  private LinearLayout body;
  private String message = "";

  interface Work {
    void run() throws Exception;
  }

  private void work(Work task) {
    if (busy || ImportJob.busy) {
      error("Another task is running. Wait for it to finish.");
      return;
    }
    busy = true;
    render();
    ImportJob.start(
        () -> {
          try {
            task.run();
          } catch (Exception e) {
            runOnUiThread(() -> error(e.getMessage()));
          }

          return "";
        });
    // ImportJob can reject before calling the task if the game still holds its lock.
    new android.os.Handler()
        .postDelayed(
            new Runnable() {
              public void run() {
                if (ImportJob.busy) {
                  new android.os.Handler().postDelayed(this, 200);
                  return;
                }
                if (busy) {
                  busy = false;
                  if (!ImportJob.status.isEmpty()) message = ImportJob.status;
                  render();
                }
              }
            },
            250);
  }

  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    ImportJob.lockFile = new File(getFilesDir(), "installation.lock");
    getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
    render();
    loadCatalog();
  }

  @Override
  public void onConfigurationChanged(android.content.res.Configuration config) {
    super.onConfigurationChanged(config);
    render();
  }

  private void error(String text) {
    if (!isFinishing())
      new AlertDialog.Builder(this)
          .setTitle("Save was not changed")
          .setMessage(text == null ? "Unable to read this save." : text)
          .setPositiveButton("OK", null)
          .show();
  }

  private void loadCatalog() {
    profile = null;
    saved = null;
    catalog = null;
    snapshot = null;
    added.clear();
    message = "";
    work(
        () -> {
          JSONObject result =
              new JSONObject(NativeBridge.saveCatalog(getFilesDir().getPath(), modded));
          runOnUiThread(() -> catalog = result);
        });
  }

  private void open(File selected) {
    work(
        () -> {
          byte[] bytes = SaveEditorStore.read(selected);
          JSONObject result =
              new JSONObject(NativeBridge.inspectSave(getFilesDir().getPath(), modded, bytes));
          runOnUiThread(
              () -> {
                profile = selected;
                snapshot = bytes;
                saved = result;
                added.clear();
                tab = "Money";
                money = Integer.toString(result.optInt("money"));
                for (int i = 0; i < 8; i++)
                  respect[i] = Integer.toString(result.optJSONArray("respect").optInt(i));
                message = "";
              });
        });
  }

  private void label(String text, int size, int colour) {
    body.addView(Ui.label(this, text, size, colour));
  }

  private Button button(String text, boolean primary, Runnable action) {
    Button b = Ui.button(this, text, primary, action);
    b.setEnabled(!busy && !ImportJob.busy);
    Ui.add(body, b);
    return b;
  }

  private EditText number(String title, String value, java.util.function.Consumer<String> changed) {
    label(title, 16, Ui.INK);
    EditText edit = new EditText(this);
    edit.setSingleLine(true);
    edit.setInputType(android.text.InputType.TYPE_CLASS_NUMBER);
    edit.setTextColor(Ui.INK);
    edit.setText(value);
    edit.setEnabled(!busy);
    edit.setContentDescription(title);
    edit.setTextSize(22);
    edit.setFilters(new InputFilter[] {new InputFilter.LengthFilter(9)});
    edit.addTextChangedListener(
        new TextWatcher() {
          public void beforeTextChanged(CharSequence s, int st, int c, int a) {}

          public void onTextChanged(CharSequence s, int st, int before, int count) {
            changed.accept(s.toString());
          }

          public void afterTextChanged(Editable e) {}
        });
    Ui.add(body, edit);
    return edit;
  }

  private String carName(String model) {
    JSONArray cars = catalog.optJSONArray("cars");
    for (int i = 0; i < cars.length(); i++) {
      JSONObject car = cars.optJSONObject(i);
      if (model.equals(car.optString("model"))) return car.optString("name");
    }
    return model;
  }

  private void render() {
    LinearLayout root = Ui.column(this);
    root.setBackgroundColor(Ui.PAPER);
    Ui.insets(root, 16);
    int padding = Ui.dp(this, 16);
    root.setPadding(padding, padding, padding, padding);
    root.addView(Ui.title(this, "Save editor"));
    root.addView(
        Ui.label(
            this,
            "Recover your garage. Changes are saved only when you apply them.",
            14,
            Ui.MUTED));
    ScrollView scroll = new ScrollView(this);
    body = Ui.column(this);
    scroll.addView(body);
    root.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
    if (busy) {
      label("Reading game and save data…", 16, Ui.MUTED);
      body.addView(new ProgressBar(this));
    } else if (profile == null) {
      LinearLayout modes = new LinearLayout(this);
      modes.addView(
          Ui.button(
              this,
              "Original game",
              !modded,
              () -> {
                modded = false;
                loadCatalog();
              }),
          new LinearLayout.LayoutParams(0, -2, 1));
      modes.addView(
          Ui.button(
              this,
              "Enabled mods",
              modded,
              () -> {
                modded = true;
                loadCatalog();
              }),
          new LinearLayout.LayoutParams(0, -2, 1));
      body.addView(modes);
      label(
          modded ? "Your profiles · editing with enabled mods" : "Your profiles · editing with original game data",
          18,
          Ui.INK);
      if (catalog != null) {
        File[] profiles = new File(catalog.optString("root"), "SAVE").listFiles();
        int count = 0;
        if (profiles != null) {
          Arrays.sort(profiles, Comparator.comparing(File::getName));
          for (File p : profiles)
            if (p.isDirectory()
                && p.getName().startsWith("P")
                && new File(p, "USERDATA").isFile()) {
              count++;
              button(SaveEditorStore.name(p), false, () -> open(p));
            }
        }
        if (count == 0)
          label(
              "No saved profiles yet. Save a career in the game first.",
              16,
              Ui.MUTED);
      }
      if (!message.isEmpty()) label(message, 15, Ui.ORANGE);
    } else {
      label(
          SaveEditorStore.name(profile) + (modded ? " · Enabled mods" : " · Original game"),
          21,
          Ui.INK);
      LinearLayout tabs = new LinearLayout(this);
      for (String name : new String[] {"Money", "Cars", "Respect"})
        tabs.addView(
            Ui.button(
                this,
                name,
                tab.equals(name),
                () -> {
                  tab = name;
                  render();
                }),
            new LinearLayout.LayoutParams(0, -2, 1));
      body.addView(tabs);
      if (tab.equals("Money")) {
        label(
            "Saved balance: $" + String.format(java.util.Locale.US, "%,d", saved.optInt("money")),
            16,
            Ui.MUTED);
        number("New balance ($)", money, v -> money = v);
        button(
            "Add $10,000",
            false,
            () -> {
              try {
                money = Long.toString(Math.min(999999999L, Long.parseLong(money) + 10000));
                render();
              } catch (NumberFormatException e) {
                error("Enter a balance first.");
              }
            });
      } else if (tab.equals("Respect")) {
        label("Set respect for each rival crew (0–1,500).", 16, Ui.MUTED);
        label(
            "Race results and host challenges stay unchanged. The game applies its normal respect"
                + " thresholds and minimums.",
            14,
            Ui.MUTED);
        for (int i = 0; i < 8; i++) {
          final int index = i;
          number(LEADERS[i], respect[i], v -> respect[index] = v);
        }
      } else {
        JSONArray garage = saved.optJSONArray("garage");
        label("Your garage · " + garage.length() + " / 55 cars", 18, Ui.INK);
        for (int i = 0; i < garage.length(); i++) label(garage.optString(i), 16, Ui.MUTED);
        button("Add a car…", false, this::chooseCar);
        for (int i = 0; i < added.size(); i++) {
          final int index = i;
          button(
              "Remove pending: " + carName(added.get(i)),
              false,
              () -> {
                added.remove(index);
                render();
              });
        }
      }
      label(
          "A recovery copy is created before each change. Existing cars and race results are kept.",
          14,
          Ui.MUTED);
      button("Restore previous editor backup…", false, this::restore);
    }
    LinearLayout footer = new LinearLayout(this);
    Button back =
        Ui.button(
            this,
            profile == null ? "Back to launcher" : "Choose another profile",
            false,
            this::onBackPressed);
    back.setEnabled(!busy);
    footer.addView(back, new LinearLayout.LayoutParams(0, -2, 1));
    if (profile != null) {
      Button apply = Ui.button(this, "Review changes", true, this::review);
      apply.setEnabled(!busy);
      footer.addView(apply, new LinearLayout.LayoutParams(0, -2, 1));
    }
    root.addView(footer);
    setContentView(root);
  }

  private void chooseCar() {
    if (saved.optJSONArray("garage").length() + added.size() >= 55) {
      error("Your garage is full (55 cars).");
      return;
    }
    JSONArray cars = catalog.optJSONArray("cars");
    ArrayList<JSONObject> sorted = new ArrayList<>();
    for (int i = 0; i < cars.length(); i++) sorted.add(cars.optJSONObject(i));
    sorted.sort(Comparator.comparing(o -> o.optString("name")));
    String[] names = new String[sorted.size()];
    for (int i = 0; i < names.length; i++) names[i] = sorted.get(i).optString("name");
    new AlertDialog.Builder(this)
        .setTitle("Add a stock car to your garage")
        .setItems(
            names,
            (d, index) -> {
              added.add(sorted.get(index).optString("model"));
              render();
            })
        .setNegativeButton("Cancel", null)
        .show();
  }

  private void review() {
    try {
      int balance = Integer.parseInt(money);
      int[] values = new int[8];
      if (balance < 0 || balance > 999999999)
        throw new IllegalArgumentException("Balance must be 0–999,999,999.");
      StringBuilder summary = new StringBuilder();
      if (balance != saved.optInt("money"))
        summary
            .append("Money: $")
            .append(saved.optInt("money"))
            .append(" → $")
            .append(balance)
            .append('\n');
      for (int i = 0; i < 8; i++) {
        values[i] = Integer.parseInt(respect[i]);
        if (values[i] != saved.optJSONArray("respect").optInt(i)
            && (values[i] < 0 || values[i] > 1500))
          throw new IllegalArgumentException("Respect must be 0–1,500.");
        if (values[i] != saved.optJSONArray("respect").optInt(i))
          summary
              .append(LEADERS[i])
              .append(": ")
              .append(saved.optJSONArray("respect").optInt(i))
              .append(" → ")
              .append(values[i])
              .append('\n');
      }
      for (String model : added) summary.append("Add: ").append(carName(model)).append('\n');
      if (summary.length() == 0) {
        error("There are no changes to apply.");
        return;
      }
      String[] cars = added.toArray(new String[0]);
      new AlertDialog.Builder(this)
          .setTitle("Apply changes to " + SaveEditorStore.name(profile) + "?")
          .setMessage(summary + "\nA recovery backup will be kept automatically.")
          .setNegativeButton("Keep editing", null)
          .setPositiveButton("Back up and apply", (d, w) -> apply(balance, values, cars))
          .show();
    } catch (IllegalArgumentException e) {
      error(
          e instanceof NumberFormatException
              ? "Fill in every number before continuing."
              : e.getMessage());
    }
  }

  private void apply(int balance, int[] values, String[] cars) {
    File selected = profile;
    byte[] original = snapshot;
    work(
        () -> {
          JSONObject current =
              new JSONObject(NativeBridge.saveCatalog(getFilesDir().getPath(), modded));
          if (!current.toString().equals(catalog.toString()))
            throw new IOException("The enabled mods changed. Reopen the editor.");
          byte[] next =
              NativeBridge.editSave(
                  getFilesDir().getPath(), modded, original, balance, values, cars);
          SaveEditorStore.apply(getFilesDir(), selected, original, next);
          runOnUiThread(
              () -> {
                profile = null;
                snapshot = null;
                added.clear();
                message =
                    "Changes saved. A recovery backup is available when you reopen this profile.";
              });
        });
  }

  private void restore() {
    new AlertDialog.Builder(this)
        .setTitle("Restore the previous editor backup?")
        .setMessage(
            "This replaces this profile with its most recent editor backup. Your current save will"
                + " be backed up first.")
        .setNegativeButton("Cancel", null)
        .setPositiveButton(
            "Restore",
            (d, w) ->
                work(
                    () -> {
                      File backup = SaveEditorStore.latest(getFilesDir(), profile);
                      if (backup == null)
                        throw new IOException("No editor backup exists for this profile yet.");
                      byte[] previous = SaveEditorStore.read(backup);
                      NativeBridge.inspectSave(getFilesDir().getPath(), modded, previous);
                      SaveEditorStore.apply(getFilesDir(), profile, snapshot, previous);
                      runOnUiThread(
                          () -> {
                            profile = null;
                            snapshot = null;
                            added.clear();
                            message = "Previous save restored.";
                          });
                    }))
        .show();
  }

  @Override
  public void onBackPressed() {
    if (busy) return;
    if (profile == null) {
      finish();
      return;
    }
    new AlertDialog.Builder(this)
        .setTitle("Leave this profile?")
        .setMessage("Any edits you have not applied will be discarded.")
        .setNegativeButton("Keep editing", null)
        .setPositiveButton(
            "Leave",
            (d, w) -> {
              profile = null;
              snapshot = null;
              added.clear();
              render();
            })
        .show();
  }
}
