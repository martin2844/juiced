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
            status.setVisibility(ImportJob.status.isEmpty() ? View.GONE : View.VISIBLE);
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
  public void onConfigurationChanged(android.content.res.Configuration c) {
    super.onConfigurationChanged(c);
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
    boolean wide = getResources().getConfiguration().screenWidthDp >= 600;
    LinearLayout root = new LinearLayout(this);
    root.setOrientation(wide ? 0 : 1);
    root.setBackgroundColor(Ui.INK);
    Ui.insets(root, 0);
    LinearLayout nav = Ui.column(this);
    nav.setPadding(dp(16), dp(18), dp(16), dp(12));
    TextView brand = Ui.label(this, "OpenJuice", 24, Ui.WHITE);
    brand.setTypeface(Typeface.create("sans-serif-condensed", Typeface.BOLD_ITALIC));
    nav.addView(brand);
    LinearLayout tabs = new LinearLayout(this);
    tabs.setOrientation(wide ? 1 : 0);
    nav.addView(tabs);
    for (String name : new String[] {"Play", "Mods", "Controls", "Settings"}) {
      Button tab =
          Ui.button(
              this,
              name,
              page.equals(name),
              () -> {
                page = name;
                render();
              });
      if (!wide) {
        tab.setPadding(0, 0, 0, 0);
        tab.setTextSize(13);
        tab.setSingleLine(true);
      }
      if (!page.equals(name)) {
        tab.setBackgroundColor(Ui.INK);
        tab.setTextColor(Ui.WHITE);
        tab.setOnFocusChangeListener(
            (v, f) -> {
              tab.setBackground(Ui.background(this, f ? 0xff465054 : Ui.INK, 0));
            });
      }
      LinearLayout.LayoutParams t =
          new LinearLayout.LayoutParams(wide ? -1 : 0, dp(48), wide ? 0 : 1);
      t.bottomMargin = dp(8);
      tabs.addView(tab, t);
    }
    root.addView(nav, new LinearLayout.LayoutParams(wide ? dp(146) : -1, wide ? -1 : -2));
    ScrollView scroll = new ScrollView(this);
    scroll.setFillViewport(true);
    scroll.setBackgroundColor(Ui.PAPER);
    root.addView(scroll, new LinearLayout.LayoutParams(wide ? 0 : -1, wide ? -1 : 0, 1));
    content = Ui.column(this);
    content.setPadding(dp(wide ? 28 : 20), dp(20), dp(wide ? 28 : 20), dp(24));
    scroll.addView(content);
    boolean ready = GameDataImport.valid(game()), idle = !ImportJob.busy;
    status = Ui.label(this, ImportJob.status, 14, Ui.ORANGE);
    status.setVisibility(ImportJob.status.isEmpty() ? View.GONE : View.VISIBLE);
    content.addView(status);
    if (ImportJob.busy) {
      ProgressBar progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
      progress.setIndeterminate(true);
      Ui.add(content, progress);
      action("Cancel import", false, () -> ImportJob.cancel = true, true);
    }
    if (page.equals("Play")) {
      heading(
          ready ? "Back in the driver's seat" : "Your game. Your garage.",
          ready
              ? "Juiced is installed and ready to race."
              : "Import your copy of Juiced to get started.");
      if (ready) {
        LinearLayout homeContent = content;
        LinearLayout hero = Ui.column(this);
        hero.setPadding(dp(20), dp(16), dp(20), dp(16));
        hero.setBackground(Ui.background(this, Ui.INK, 0));
        TextView logo = Ui.label(this, "Juiced", 44, Ui.WHITE);
        logo.setTypeface(Typeface.create("sans-serif-condensed", Typeface.BOLD_ITALIC));
        hero.addView(logo);
        hero.addView(
            Ui.label(this, "Build a crew. Fill your garage. Own the streets.", 15, 0xffc8ceca));
        Button play = Ui.button(this, "Start playing", true, () -> launch("game"));
        play.setEnabled(idle);
        Ui.add(hero, play);
        if (wide) {
          LinearLayout columns = new LinearLayout(this);
          columns.setGravity(Gravity.TOP);
          columns.addView(hero, new LinearLayout.LayoutParams(0, -2, 1.2f));
          LinearLayout side = Ui.column(this);
          side.setPadding(dp(20), 0, 0, 0);
          columns.addView(side, new LinearLayout.LayoutParams(0, -2, 1));
          Ui.add(content, columns);
          content = side;
        } else Ui.add(content, hero);
        action("Quick race", false, () -> launch("race"), idle);
        List<File> packs = ModStore.packages(mods());
        int active = 0;
        for (File p : packs) if (!new File(p, "disabled").exists()) active++;
        note(active + " of " + packs.size() + " mods enabled. Manage your collection in Mods.");
        action("Play original game", false, () -> launch("stock"), idle);
        content = homeContent;
      } else {
        note(
            "Choose juiced.zip from your files. The app checks the game before installing it; your"
                + " ZIP stays untouched.");
        action("Choose juiced.zip", true, () -> pick(1), idle);
        Ui.divider(content);
        note(
            "You will need your own PC game files and at least 3 GB of free space. Mods can be"
                + " added after installation.");
      }
    } else if (page.equals("Mods")) {
      heading("Make it yours", "Install packages, then choose what you want in your game.");
      LinearLayout modBody = content;
      if (wide) {
        LinearLayout toolbar = new LinearLayout(this);
        toolbar.setOrientation(0);
        Ui.add(content, toolbar);
        content = toolbar;
      }
      action("Add mods from ZIP", true, () -> pick(2), ready && idle);
      action("Choose a mods folder", false, () -> pick(3), ready && idle);
      if (!getPreferences(0).getString("mods-tree", "").isEmpty())
        action(
            "Refresh linked folder",
            false,
            () -> importMods(Uri.parse(getPreferences(0).getString("mods-tree", "")), true),
            ready && idle);
      content = modBody;
      if (!ready) note("Install Juiced from Play before adding mods.");
      Ui.divider(content);
      List<File> packs = ModStore.packages(mods());
      if (packs.isEmpty())
        note("No packages yet. Add a ZIP or select a folder containing your mods.");
      for (File pkg : packs)
        try {
          Properties m = ModStore.manifest(pkg);
          boolean enabled = !new File(pkg, "disabled").exists();
          LinearLayout row = new LinearLayout(this);
          row.setGravity(Gravity.CENTER_VERTICAL);
          row.setPadding(0, dp(8), 0, dp(8));
          LinearLayout copy = Ui.column(this);
          copy.addView(Ui.label(this, m.getProperty("name", pkg.getName()), 18, Ui.INK));
          copy.addView(
              Ui.label(
                  this, m.getProperty("description", modDescription(pkg.getName())), 13, Ui.MUTED));
          row.addView(copy, new LinearLayout.LayoutParams(0, -2, 1));
          Button toggle =
              Ui.button(this, enabled ? "Enabled" : "Disabled", enabled, () -> toggle(pkg));
          toggle.setContentDescription(
              (enabled ? "Disable " : "Enable ") + m.getProperty("name", pkg.getName()));
          toggle.setEnabled(idle);
          row.addView(toggle, new LinearLayout.LayoutParams(dp(108), dp(48)));
          Ui.add(content, row);
        } catch (IOException e) {
          note(e.getMessage());
        }
      if (!packs.isEmpty()) action("Check active mods", false, this::validate, ready && idle);
    } else if (page.equals("Controls")) {
      heading(
          "Choose how you drive",
          "Use the screen, a handheld's built-in controls or a connected gamepad.");
      LinearLayout controlBody = content;
      LinearLayout right = null;
      if (wide) {
        LinearLayout columns = new LinearLayout(this);
        LinearLayout left = Ui.column(this);
        right = Ui.column(this);
        right.setPadding(dp(24), 0, 0, 0);
        columns.addView(left, new LinearLayout.LayoutParams(0, -2, 1));
        columns.addView(right, new LinearLayout.LayoutParams(0, -2, 1));
        Ui.add(content, columns);
        content = left;
      }
      section(
          "Touch controls",
          "A menu pad and driving controls, with layouts you can move and remap.");
      action(
          "Set up touch controls",
          true,
          () -> startActivity(new Intent(this, TouchActivity.class)),
          idle);
      if (right != null) content = right;
      else Ui.divider(content);
      section("Gamepad", "Steer with the left stick. RT accelerates, LT brakes and Start pauses.");
      action(
          "Set up gamepad",
          false,
          () -> startActivity(new Intent(this, ControllerActivity.class)),
          idle);
      note("Live button and stick testing is included in gamepad setup.");
      content = controlBody;
    } else {
      heading("Keep your garage safe", "Manage game files, save backups and app information.");
      LinearLayout settingsBody = content;
      LinearLayout saves = null;
      if (wide) {
        LinearLayout columns = new LinearLayout(this);
        LinearLayout install = Ui.column(this);
        saves = Ui.column(this);
        saves.setPadding(dp(24), 0, 0, 0);
        columns.addView(install, new LinearLayout.LayoutParams(0, -2, 1));
        columns.addView(saves, new LinearLayout.LayoutParams(0, -2, 1));
        Ui.add(content, columns);
        content = install;
      }
      section(
          "Game installation",
          ready
              ? "Juiced is installed. Replacing the game data keeps your saves."
              : "No game installed yet.");
      note(
          String.format(
              java.util.Locale.ROOT,
              "%.1f GB available",
              getFilesDir().getUsableSpace() / 1073741824.0));
      action(ready ? "Replace game ZIP" : "Install game ZIP", false, () -> pick(1), idle);
      action("Check game files", false, this::validate, ready && idle);
      if (saves != null) content = saves;
      else Ui.divider(content);
      section(
          "Saved games",
          "Original and modded profiles are kept separately. Export a backup before uninstalling"
              + " the app.");
      action(
          "Export save backup",
          true,
          () -> {
            Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            i.setType("application/zip");
            i.putExtra(Intent.EXTRA_TITLE, "OpenJuice-saves.zip");
            startActivityForResult(i, 4);
          },
          idle);
      action(
          "Restore save backup",
          false,
          () ->
              new AlertDialog.Builder(this)
                  .setTitle("Restore saved games?")
                  .setMessage(
                      "This replaces local saves with the backup you choose. Export your current"
                          + " saves first if you want to keep them.")
                  .setNegativeButton("Cancel", null)
                  .setPositiveButton("Choose backup", (d, w) -> pick(5))
                  .show(),
          idle);
      content = settingsBody;
      Ui.divider(content);
      section("About OpenJuice", "Version 1.02.5 · Native Android engine");
      action("Open-source licenses", false, this::licenses, true);
    }
    setContentView(root);
    if (!focusLabel.isEmpty()) restoreFocus(root, focusLabel);
  }

  private void heading(String title, String detail) {
    content.addView(Ui.title(this, title));
    note(detail);
  }

  private void section(String title, String detail) {
    TextView t = Ui.label(this, title, 20, Ui.INK);
    t.setTypeface(null, Typeface.BOLD);
    content.addView(t);
    note(detail);
  }

  private void note(String value) {
    content.addView(Ui.label(this, value, 15, Ui.MUTED));
  }

  private void action(String label, boolean primary, Runnable run, boolean enabled) {
    Button b = Ui.button(this, label, primary, run);
    b.setEnabled(enabled);
    if (content.getOrientation() == 0) {
      LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, -2, 1);
      p.rightMargin = dp(8);
      content.addView(b, p);
    } else Ui.add(content, b);
  }

  private String modDescription(String id) {
    switch (id) {
      case "ferrari":
        return "Add the Ferrari 458 to your car collection.";
      case "wheels":
        return "More wheel choices for your builds.";
      case "neon":
        return "Customize underglow colours.";
      case "controls":
        return "Modern keyboard bindings.";
      case "driver-photo":
        return "Use your own driver portrait.";
      case "custom-logo":
        return "Import a custom crew logo.";
      default:
        return "Optional game package.";
    }
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
