package org.openjuice.android;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

/** Imports only into app-owned staging, then swaps a completely validated dataset into place. */
public final class GameDataImport {
  public interface Progress {
    void update(int files, long bytes) throws IOException;
  }

  private static final long MAX_BYTES = 6L * 1024 * 1024 * 1024;
  private static final Set<String> OMIT =
      new java.util.HashSet<>(
          java.util.Arrays.asList("exe", "dll", "bat", "cmd", "lnk", "msi", "com"));

  public static boolean valid(File root) {
    File[] children = root.listFiles();
    if (children == null) return false;
    boolean tracks = false, cars = false, scripts = false;
    for (File child : children) {
      String name = child.getName().toLowerCase(Locale.ROOT);
      tracks |= name.equals("tracks") && child.isDirectory();
      cars |= name.equals("carmodels.dat") && child.isFile();
      scripts |= name.equals("scripts.dat") && child.isFile();
    }
    return tracks && cars && scripts;
  }

  public interface Validator {
    void check(File root) throws IOException;
  }

  public static void recover(File filesDir) throws IOException {
    File game = new File(filesDir, "game"), backup = new File(filesDir, "game-previous");
    if (!game.exists() && backup.exists() && !backup.renameTo(game))
      throw new IOException("Cannot restore previous game data");
  }

  public static void extract(InputStream source, File filesDir, Progress progress)
      throws IOException {
    extract(source, filesDir, progress, root -> {});
  }

  public static synchronized void extract(
      InputStream source, File filesDir, Progress progress, Validator validator)
      throws IOException {
    File stage = new File(filesDir, "import-staging");
    File game = new File(filesDir, "game"), backup = new File(filesDir, "game-previous");
    // Recover an interrupted directory swap before starting another import.
    if (!game.exists() && backup.exists() && !backup.renameTo(game))
      throw new IOException("Cannot restore previous data");
    delete(stage);
    if (!stage.mkdirs()) throw new IOException("Cannot create import staging folder");
    try {
      int entries = 0, files = 0;
      long bytes = 0;
      String prefix = stage.getCanonicalPath() + File.separator;
      try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(source))) {
        ZipEntry entry;
        byte[] buffer = new byte[128 * 1024];
        while ((entry = zip.getNextEntry()) != null) {
          if (++entries > 100000) throw new IOException("Archive has too many entries");
          String name = entry.getName().replace('\\', '/');
          File out = new File(stage, name);
          if (name.startsWith("/") || !out.getCanonicalPath().startsWith(prefix))
            throw new IOException("Unsafe archive path: " + name);
          String ext = name.substring(name.lastIndexOf('.') + 1).toLowerCase(Locale.ROOT);
          if (OMIT.contains(ext)) continue;
          if (entry.isDirectory()) {
            if (!out.isDirectory() && !out.mkdirs())
              throw new IOException("Cannot create directory");
            continue;
          }
          File parent = out.getParentFile();
          if (!parent.isDirectory() && !parent.mkdirs())
            throw new IOException("Cannot create directory");
          if (out.exists()) throw new IOException("Duplicate archive path: " + name);
          try (OutputStream dest = new BufferedOutputStream(new FileOutputStream(out))) {
            int n;
            while ((n = zip.read(buffer)) != -1) {
              bytes += n;
              if (bytes > MAX_BYTES) throw new IOException("Archive exceeds 6 GiB safety limit");
              if (Thread.currentThread().isInterrupted()) throw new IOException("Import cancelled");
              if (bytes % (16 * 1024 * 1024) < n && filesDir.getUsableSpace() < 32 * 1024 * 1024)
                throw new IOException("Not enough storage. Free space and try again.");
              dest.write(buffer, 0, n);
              if (bytes % (1024 * 1024) < n) progress.update(files, bytes);
            }
          }
          if (++files % 20 == 0) progress.update(files, bytes);
        }
        progress.update(files, bytes);
      }
      File root = valid(stage) ? stage : null;
      File[] children = stage.listFiles();
      if (root == null && children != null)
        for (File child : children) {
          if (valid(child)) {
            if (root != null) throw new IOException("Archive contains multiple game folders");
            root = child;
          }
        }
      if (root == null)
        throw new IOException("No Juiced data found: need tracks/, carmodels.dat and scripts.dat");
      validator.check(root);
      delete(backup);
      if (game.exists() && !game.renameTo(backup))
        throw new IOException("Cannot preserve existing data");
      if (!root.renameTo(game)) {
        if (backup.exists()) backup.renameTo(game);
        throw new IOException("Cannot activate imported data");
      }
      delete(backup);
    } finally {
      delete(stage);
    }
  }

  private static void delete(File file) throws IOException {
    if (!file.exists()) return;
    File[] children = file.listFiles();
    if (children != null) for (File child : children) delete(child);
    if (!file.delete()) throw new IOException("Cannot remove staging file: " + file);
  }
}
