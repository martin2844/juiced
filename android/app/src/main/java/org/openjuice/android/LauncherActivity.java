package org.openjuice.android;

import android.app.*;
import android.content.*;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.util.*;

public final class LauncherActivity extends Activity {
  private LinearLayout content;
  private TextView status;
  private final Handler handler = new Handler();
  private String page = "Play", lastState = "";
  private final Runnable refresh =
      new Runnable() {
        public void run() {
          String state = ImportJob.busy ? "busy" : "idle:" + ImportJob.status;
          if (!state.equals(lastState)) {
            lastState = state;
            render();
          } else if (status != null) {
            status.setText(ImportJob.status);
          }
          handler.postDelayed(this, 350);
        }
      };

  private int dp(int n) {
    return Math.round(n * getResources().getDisplayMetrics().density);
  }

  private File game() {
    return new File(getFilesDir(), "game");
  }

  private File mods() {
    return new File(getFilesDir(), "mods");
  }

  private TextView text(String value, int size) {
    TextView t = new TextView(this);
    t.setText(value);
    t.setTextSize(size);
    t.setTextColor(0xffedf2f7);
    t.setPadding(0, dp(6), 0, dp(10));
    content.addView(t);
    return t;
  }

  private void styleButton(Button b) {
    android.graphics.drawable.StateListDrawable bg =
        new android.graphics.drawable.StateListDrawable();
    int[][] states = {
      new int[] {android.R.attr.state_focused},
      new int[] {android.R.attr.state_selected},
      new int[] {-android.R.attr.state_enabled},
      new int[] {}
    };
    int[] colors = {0xffffd18a, 0xfff5b855, 0xff22384d, 0xff2d4964};
    for (int i = 0; i < states.length; i++) {
      android.graphics.drawable.GradientDrawable d =
          new android.graphics.drawable.GradientDrawable();
      d.setColor(colors[i]);
      d.setCornerRadius(dp(5));
      if (i == 0) d.setStroke(dp(2), 0xffffffff);
      bg.addState(states[i], d);
    }
    b.setBackground(bg);
    b.setTextColor(
        new android.content.res.ColorStateList(
            states, new int[] {0xff14283e, 0xff14283e, 0xff91a0ae, 0xffedf2f7}));
    b.setPadding(dp(12), dp(8), dp(12), dp(8));
  }

  private Button button(String label, Runnable run, boolean enabled) {
    Button b = new Button(this);
    b.setText(label);
    b.setAllCaps(false);
    styleButton(b);
    b.setMinHeight(dp(48));
    b.setEnabled(enabled);
    b.setOnClickListener(v -> run.run());
    LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
    params.bottomMargin = dp(8);
    content.addView(b, params);
    return b;
  }

  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    ImportJob.lockFile = new File(getFilesDir(), "installation.lock");
    if (state != null) page = state.getString("page", "Play");
    try {
      ModStore.recover(getFilesDir());
      GameDataImport.recover(getFilesDir());
      SaveBackup.recover(getFilesDir());
    } catch (IOException e) {
      ImportJob.status = e.getMessage();
    }
    render();
  }

  @Override
  protected void onSaveInstanceState(Bundle state) {
    super.onSaveInstanceState(state);
    state.putString("page", page);
  }

  @Override
  public void onResume() {
    super.onResume();
    File error = new File(getFilesDir(), "last-error.txt");
    if (error.isFile())
      try {
        ImportJob.status =
            "Last launch: "
                + new String(
                    java.nio.file.Files.readAllBytes(error.toPath()),
                    java.nio.charset.StandardCharsets.UTF_8);
        error.delete();
      } catch (IOException ignored) {
      }
    handler.post(refresh);
  }

  @Override
  public void onPause() {
    handler.removeCallbacks(refresh);
    super.onPause();
  }

  private void restoreFocus(View view, String label) {
    if (view instanceof Button
        && ((Button) view).getText().toString().equals(label)
        && view.isEnabled()) view.requestFocus();
    if (view instanceof android.view.ViewGroup) {
      android.view.ViewGroup group = (android.view.ViewGroup) view;
      for (int i = 0; i < group.getChildCount(); i++) restoreFocus(group.getChildAt(i), label);
    }
  }

  private void render() {
    View focused = getCurrentFocus();
    String focusLabel = focused instanceof Button ? ((Button) focused).getText().toString() : "";
    LinearLayout root = new LinearLayout(this);
    root.setOrientation(LinearLayout.VERTICAL);
    root.setBackgroundColor(0xff14283e);
    root.setPadding(dp(20), dp(18), dp(20), dp(12));
    root.setOnApplyWindowInsetsListener(
        (view, insets) -> {
          view.setPadding(
              dp(20) + insets.getSystemWindowInsetLeft(),
              dp(18) + insets.getSystemWindowInsetTop(),
              dp(20) + insets.getSystemWindowInsetRight(),
              dp(12) + insets.getSystemWindowInsetBottom());
          return insets;
        });
    TextView heading = new TextView(this);
    heading.setText("OpenJuice");
    heading.setTextSize(30);
    heading.setTypeface(Typeface.create("sans-serif-condensed", Typeface.BOLD));
    heading.setTextColor(0xfff5b855);
    root.addView(heading);
    LinearLayout tabs = new LinearLayout(this);
    root.addView(tabs);
    for (String name : new String[] {"Play", "Mods", "Controls", "Settings"}) {
      Button tab = new Button(this);
      tab.setText(name);
      tab.setAllCaps(false);
      tab.setSelected(page.equals(name));
      styleButton(tab);
      tab.setOnClickListener(
          v -> {
            page = name;
            render();
          });
      LinearLayout.LayoutParams tp = new LinearLayout.LayoutParams(0, dp(48), 1);
      tp.setMargins(0, dp(8), dp(6), 0);
      tabs.addView(tab, tp);
    }
    ScrollView scroll = new ScrollView(this);
    root.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
    content = new LinearLayout(this);
    content.setOrientation(LinearLayout.VERTICAL);
    content.setPadding(0, dp(12), 0, 0);
    scroll.addView(content);
    boolean ready = GameDataImport.valid(game()), idle = !ImportJob.busy;
    status = text(ImportJob.status, 15);
    status.setTextColor(0xfff5b855);
    if (ImportJob.busy) {
      ProgressBar bar = new ProgressBar(this);
      content.addView(bar);
      button("Cancel import", () -> ImportJob.cancel = true, true);
    }
    if (page.equals("Play")) {
      text(ready ? "Your garage is ready" : "Bring your copy of Juiced", 25);
      text(
          ready
              ? "Start a career or jump into a race. Manage installed packages in Mods."
              : "Choose juiced.zip to install the game data. Your archive stays untouched. Mods and"
                  + " saves are stored separately.",
          16);
      if (ready) {
        button("Play", () -> launch("game"), idle);
        button("Instant race", () -> launch("race"), idle);
        button("Play without mods", () -> launch("stock"), idle);
      }
      button(ready ? "Replace game data…" : "Import juiced.zip…", () -> pick(1), idle);
      text(
          "Free app storage: "
              + getFilesDir().getUsableSpace() / (1024 * 1024)
              + " MiB. Import uses temporary space before replacing an existing installation.",
          14);
    } else if (page.equals("Mods")) {
      text("Your mod collection", 25);
      text(
          "Choose a mods folder or a package ZIP. Imports are checked against your game before"
              + " activation. Updating a folder preserves your enabled/disabled choices.",
          16);
      button("Choose mods folder…", () -> pick(3), ready && idle);
      String folder = getPreferences(0).getString("mods-tree", "");
      if (!folder.isEmpty())
        button("Refresh selected folder", () -> importMods(Uri.parse(folder), true), ready && idle);
      button("Import mod ZIP…", () -> pick(2), ready && idle);
      List<File> packages = ModStore.packages(mods());
      if (packages.isEmpty()) text("No mods installed. The game works without them.", 16);
      for (File pkg : packages) {
        try {
          Properties manifest = ModStore.manifest(pkg);
          boolean enabled = !new File(pkg, "disabled").exists();
          button(
              (enabled ? "On  ·  " : "Off  ·  ") + manifest.getProperty("name", pkg.getName()),
              () -> toggle(pkg),
              idle);
        } catch (IOException e) {
          text(pkg.getName() + ": " + e.getMessage(), 16);
        }
      }
      button("Check active mods", () -> validate(), ready && idle);
    } else if (page.equals("Controls")) {
      text("Play your way", 25);
      text(
          "Retroid Pocket 5 and standard Android gamepads: left stick steers, RT accelerates, LT"
              + " brakes. D-pad navigates menus. South button selects; east goes back. Start"
              + " pauses. Touch controls appear when no analog controller is connected.",
          16);
      button("Test buttons, sticks and graphics", () -> launch("diagnostics"), ready && idle);
      button(
          "Controller mapping and dead zone",
          () -> startActivity(new Intent(this, ControllerActivity.class)),
          idle);
    } else {
      text("Installation & saves", 25);
      text(
          "Native ARM64 · OpenJuice 1.02.5\n"
              + "720p internal rendering cap\n"
              + "Original and modded careers have separate save folders.",
          16);
      button("Check installed game", () -> validate(), ready && idle);
      button(
          "Export saves…",
          () -> {
            Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            i.setType("application/zip");
            i.putExtra(Intent.EXTRA_TITLE, "OpenJuice-saves.zip");
            startActivityForResult(i, 4);
          },
          idle);
      button(
          "Restore saves…",
          () ->
              new AlertDialog.Builder(this)
                  .setMessage(
                      "Replace local saves with a backup? A failed restore keeps your current"
                          + " saves.")
                  .setNegativeButton("Cancel", null)
                  .setPositiveButton("Choose backup", (d, w) -> pick(5))
                  .show(),
          idle);
      button("Licenses", () -> licenses(), idle);
    }
    setContentView(root);
    if (!focusLabel.isEmpty()) restoreFocus(root, focusLabel);
  }

  private void launch(String mode) {
    startActivity(new Intent(this, JuicedActivity.class).putExtra("mode", mode));
  }

  private void pick(int code) {
    Intent i =
        new Intent(code == 3 ? Intent.ACTION_OPEN_DOCUMENT_TREE : Intent.ACTION_OPEN_DOCUMENT);
    if (code != 3) {
      i.addCategory(Intent.CATEGORY_OPENABLE);
      i.setType("*/*");
    }
    i.addFlags(
        Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
    startActivityForResult(i, code);
  }

  private void validate() {
    File game = game(), mods = mods();
    ImportJob.start(
        () -> NativeBridge.validate(game.getPath(), mods.isDirectory() ? mods.getPath() : ""));
  }

  private void toggle(File pkg) {
    File files = getFilesDir(), game = game();
    ImportJob.start(
        () -> {
          File marker = new File(pkg, "disabled");
          boolean old = marker.exists();
          if (old) {
            if (!marker.delete()) throw new IOException("Cannot enable package");
          } else if (!marker.createNewFile()) throw new IOException("Cannot disable package");
          try {
            String result =
                NativeBridge.validate(game.getPath(), new File(files, "mods").getPath());
            if (!result.startsWith("OK:")) throw new IOException(result);
            return result;
          } catch (Exception | LinkageError e) {
            if (old) marker.createNewFile();
            else marker.delete();
            throw e;
          }
        });
  }

  private void importMods(Uri uri, boolean tree) {
    File files = getFilesDir();
    ContentResolver resolver = getContentResolver();
    ImportJob.start(
        () -> {
          File stage = new File(files, "mods-import");
          ModStore.delete(stage);
          stage.mkdirs();
          try {
            if (tree) new FolderImport().copy(resolver, uri, stage);
            else
              try (InputStream in = resolver.openInputStream(uri)) {
                if (in == null) throw new IOException("Cannot open ZIP");
                ModStore.unpack(
                    in,
                    stage,
                    b -> ImportJob.progress("Reading mods · " + b / (1024 * 1024) + " MiB"));
              }
            ImportJob.progress("Validating mods against your game…");
            return ModStore.install(
                files,
                stage,
                f -> {
                  String validation =
                      NativeBridge.validate(new File(files, "game").getPath(), f.getPath());
                  ImportJob.progress("Activating validated mods…");
                  return validation;
                });
          } finally {
            ModStore.delete(stage);
          }
        });
  }

  @Override
  protected void onActivityResult(int request, int result, Intent data) {
    super.onActivityResult(request, result, data);
    if (result != RESULT_OK || data == null || data.getData() == null) return;
    Uri uri = data.getData();
    if (request == 3) {
      try {
        getContentResolver()
            .takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        getPreferences(0).edit().putString("mods-tree", uri.toString()).apply();
      } catch (SecurityException e) {
        ImportJob.status = "Folder must be selected again after restarting.";
      }
      importMods(uri, true);
    } else if (request == 2) importMods(uri, false);
    else if (request == 1) {
      File files = getFilesDir();
      ContentResolver resolver = getContentResolver();
      ImportJob.start(
          () -> {
            try (InputStream in = resolver.openInputStream(uri)) {
              if (in == null) throw new IOException("Cannot open archive");
              GameDataImport.extract(
                  in,
                  files,
                  (n, b) ->
                      ImportJob.progress(
                          "Importing game · " + n + " files · " + b / (1024 * 1024) + " MiB"),
                  root -> {
                    ImportJob.progress("Checking game data…");
                    String check = NativeBridge.validate(root.getPath(), "");
                    ImportJob.progress("Activating validated game…");
                    if (!check.startsWith("OK:")) throw new IOException(check);
                  });
              return "Game imported and validated. Ready to play.";
            }
          });
    } else if (request == 4 || request == 5) {
      File files = getFilesDir();
      ContentResolver resolver = getContentResolver();
      ImportJob.start(
          () -> {
            if (request == 4) {
              try (OutputStream out = resolver.openOutputStream(uri)) {
                if (out == null) throw new IOException("Cannot create backup");
                SaveBackup.export(files, out);
              }
              return "Saves exported.";
            }
            try (InputStream in = resolver.openInputStream(uri)) {
              if (in == null) throw new IOException("Cannot read backup");
              SaveBackup.restore(files, in);
            }
            return "Saves restored.";
          });
    }
  }

  private void licenses() {
    StringBuilder value = new StringBuilder();
    try {
      for (String name : getAssets().list("licenses")) {
        value.append("\n\n").append(name).append('\n');
        try (BufferedReader r =
            new BufferedReader(new InputStreamReader(getAssets().open("licenses/" + name)))) {
          String line;
          while ((line = r.readLine()) != null) value.append(line).append('\n');
        }
      }
    } catch (IOException e) {
      value.append(e.getMessage());
    }
    new AlertDialog.Builder(this)
        .setTitle("Licenses")
        .setMessage(value)
        .setPositiveButton("Close", null)
        .show();
  }
}
