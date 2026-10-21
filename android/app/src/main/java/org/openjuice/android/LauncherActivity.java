package org.openjuice.android;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.*;
import java.io.*;
import java.util.concurrent.*;

public final class LauncherActivity extends Activity {
    private TextView status;
    private Button importButton, launchButton, raceButton, diagnosticsButton;
    private ProgressBar progress;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(32, 64, 32, 32);
        TextView title = new TextView(this); title.setText("Juiced · Android development build"); title.setTextSize(25);
        layout.addView(title);
        TextView intro = new TextView(this);
        intro.setText("Import your Juiced ZIP, then play using the built-in controls or a gamepad. Allow about 3 GB of free storage.\n\nLeft stick: steer · RT: accelerate · LT: brake · A: select · B: back · Start: pause.");
        intro.setPadding(0, 24, 0, 24); layout.addView(intro);
        status = new TextView(this); layout.addView(status);
        progress = new ProgressBar(this); progress.setVisibility(View.GONE); layout.addView(progress);
        importButton = new Button(this); importButton.setText("Import Juiced ZIP"); layout.addView(importButton);
        importButton.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE); intent.setType("*/*");
            startActivityForResult(intent, 1);
        });
        launchButton = new Button(this); launchButton.setText("Play Juiced"); layout.addView(launchButton);
        launchButton.setOnClickListener(v -> startActivity(new Intent(this, JuicedActivity.class)));
        raceButton = new Button(this); raceButton.setText("Instant race"); layout.addView(raceButton);
        raceButton.setOnClickListener(v -> startActivity(new Intent(this, JuicedActivity.class).putExtra("mode", "race")));
        diagnosticsButton = new Button(this); diagnosticsButton.setText("Check controls and graphics"); layout.addView(diagnosticsButton);
        diagnosticsButton.setOnClickListener(v -> startActivity(new Intent(this, JuicedActivity.class).putExtra("mode", "diagnostics")));
        Button licenses = new Button(this); licenses.setText("Licenses"); layout.addView(licenses);
        licenses.setOnClickListener(v -> {
            StringBuilder text = new StringBuilder();
            try {
                String[] names = getAssets().list("licenses");
                if (names != null) for (String name : names) {
                    text.append("\n\n").append(name).append("\n");
                    try (BufferedReader reader = new BufferedReader(new InputStreamReader(getAssets().open("licenses/" + name)))) {
                        String line; while ((line = reader.readLine()) != null) text.append(line).append('\n');
                    }
                }
            } catch (IOException e) { text.append(e.getMessage()); }
            new android.app.AlertDialog.Builder(this).setTitle("Licenses").setMessage(text.toString())
                .setPositiveButton("Close", null).show();
        });
        ScrollView scroll = new ScrollView(this); scroll.addView(layout); setContentView(scroll); refresh();
    }
    private void refresh() {
        boolean ready = GameDataImport.valid(new File(getFilesDir(), "game"));
        launchButton.setEnabled(ready); raceButton.setEnabled(ready); diagnosticsButton.setEnabled(ready); status.setText(ready ? "Game data imported." : "No game data imported yet.");
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != 1 || result != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        importButton.setEnabled(false); launchButton.setEnabled(false); raceButton.setEnabled(false); diagnosticsButton.setEnabled(false); progress.setVisibility(View.VISIBLE);
        status.setText("Importing game data…");
        worker.execute(() -> {
            String error = null;
            try (InputStream in = getContentResolver().openInputStream(uri)) {
                if (in == null) throw new IOException("Cannot open selected archive");
                GameDataImport.extract(in, getFilesDir(), (files, bytes) -> runOnUiThread(() -> {
                    if (!isDestroyed()) status.setText("Imported " + files + " files · " + bytes / (1024 * 1024) + " MiB");
                }));
            } catch (Exception e) { error = e.getMessage(); }
            String failure = error;
            runOnUiThread(() -> {
                if (isDestroyed()) return;
                progress.setVisibility(View.GONE); importButton.setEnabled(true); refresh();
                if (failure != null) status.setText("Import failed: " + failure);
            });
        });
    }
    @Override protected void onDestroy() { worker.shutdown(); super.onDestroy(); }
}
