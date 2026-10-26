package org.openjuice.android;

import java.io.*;
import java.nio.channels.FileLock;
import java.nio.file.Files;
import java.util.*;
import org.json.*;

final class SaveEditorInstrumentedTest {
  static void check(boolean ok, String message) {
    if (!ok) throw new AssertionError(message);
  }

  static void run(File files) throws Exception {
    try (RandomAccessFile lockFile =
            new RandomAccessFile(new File(files, "installation.lock"), "rw");
        FileLock lock = lockFile.getChannel().tryLock()) {
      check(lock != null, "Game must be closed");
      JSONObject catalog = new JSONObject(NativeBridge.saveCatalog(files.getPath(), true));
      File source = new File(catalog.getString("root"), "SAVE/PPlayer");
      byte[] original = SaveEditorStore.read(source);
      JSONObject before = new JSONObject(NativeBridge.inspectSave(files.getPath(), true, original));
      File clone = new File(files, "ModSaves/editor-test-" + UUID.randomUUID());
      ArrayList<File> backups = new ArrayList<>();
      check(clone.mkdirs(), "Clone folder");
      try {
        Files.write(new File(clone, "juicey.dat").toPath(), original);
        int[] respect = new int[8];
        Arrays.fill(respect, 1200);
        String model = catalog.getJSONArray("cars").getJSONObject(0).getString("model");
        byte[] edited =
            NativeBridge.editSave(
                files.getPath(), true, original, 123456, respect, new String[] {model});
        JSONObject after = new JSONObject(NativeBridge.inspectSave(files.getPath(), true, edited));
        check(after.getInt("money") == 123456, "Money");
        check(
            after.getJSONArray("garage").length() == before.getJSONArray("garage").length() + 1,
            "Car");
        for (int i = 0; i < 8; i++)
          check(after.getJSONArray("respect").getInt(i) == 1200, "Respect");
        File backup = SaveEditorStore.apply(files, clone, original, edited);
        backups.add(backup);
        check(Arrays.equals(SaveEditorStore.read(backup), original), "Backup identical");
        check(Arrays.equals(SaveEditorStore.read(clone), edited), "Applied identical");
        check(backup.equals(SaveEditorStore.latest(files, clone)), "Latest backup");
        boolean rejected = false;
        try {
          SaveEditorStore.apply(files, clone, original, edited);
        } catch (IOException expected) {
          rejected = true;
        }
        check(rejected, "Stale edit rejected");
        backups.add(SaveEditorStore.apply(files, clone, edited, SaveEditorStore.read(backup)));
        check(Arrays.equals(SaveEditorStore.read(clone), original), "Restored identical");
      } finally {
        ModStore.delete(clone);
        for (File backup : backups) ModStore.delete(backup);
        check(Arrays.equals(SaveEditorStore.read(source), original), "User save untouched");
      }
    }
  }
}
