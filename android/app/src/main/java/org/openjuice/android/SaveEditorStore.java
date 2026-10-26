package org.openjuice.android;

import android.util.AtomicFile;
import java.io.*;
import java.nio.file.Files;
import java.util.*;

final class SaveEditorStore {
  static byte[] read(File profile) throws IOException {
    AtomicFile file = new AtomicFile(new File(profile, "juicey.dat"));
    try (InputStream in = file.openRead();
        ByteArrayOutputStream out = new ByteArrayOutputStream()) {
      byte[] buffer = new byte[8192];
      int n;
      while ((n = in.read(buffer)) != -1) {
        if (out.size() + n > 2 * 1024 * 1024) throw new IOException("Save is too large");
        out.write(buffer, 0, n);
      }
      return out.toByteArray();
    }
  }

  static String name(File profile) {
    try {
      byte[] b = Files.readAllBytes(new File(profile, "USERDATA").toPath());
      if (b.length > 4 && b.length < 4096) {
        String name = new String(b, 4, b.length - 4, java.nio.charset.StandardCharsets.UTF_16LE);
        int end = name.indexOf(0);
        return end < 0 ? name : name.substring(0, end);
      }
    } catch (IOException ignored) {
    }
    return profile.getName();
  }

  static String relative(File files, File profile) throws IOException {
    String base = files.getCanonicalPath() + File.separator, path = profile.getCanonicalPath();
    if (!path.startsWith(base)) throw new IOException("Invalid profile folder");
    String relative = path.substring(base.length());
    if (!(relative.startsWith("saves/") || relative.startsWith("ModSaves/")))
      throw new IOException("Invalid save location");
    return relative;
  }

  static File latest(File files, File profile) throws IOException {
    String target = relative(files, profile);
    File[] list = new File(files, "save-edits").listFiles();
    if (list == null) return null;
    Arrays.sort(list, Comparator.comparing(File::getName).reversed());
    for (File backup : list) {
      File meta = new File(backup, "target.txt"), data = new File(backup, "juicey.dat");
      if (meta.isFile()
          && data.isFile()
          && target.equals(
              new String(
                  Files.readAllBytes(meta.toPath()), java.nio.charset.StandardCharsets.UTF_8)))
        return backup;
    }
    return null;
  }

  static File apply(File files, File profile, byte[] expected, byte[] replacement)
      throws IOException {
    String target = relative(files, profile);
    if (!Arrays.equals(read(profile), expected))
      throw new IOException("This save changed. Reopen it before applying edits.");
    File backup =
        new File(files, "save-edits/" + System.currentTimeMillis() + "-" + UUID.randomUUID());
    if (!backup.mkdirs()) throw new IOException("Cannot create recovery backup");
    try (FileOutputStream out = new FileOutputStream(new File(backup, "juicey.dat"))) {
      out.write(expected);
      out.getFD().sync();
    }
    try (FileOutputStream out = new FileOutputStream(new File(backup, "target.txt"))) {
      out.write(target.getBytes(java.nio.charset.StandardCharsets.UTF_8));
      out.getFD().sync();
    }
    AtomicFile archive = new AtomicFile(new File(profile, "juicey.dat"));
    FileOutputStream output = null;
    try {
      output = archive.startWrite();
      output.write(replacement);
      archive.finishWrite(output);
    } catch (IOException e) {
      archive.failWrite(output);
      throw e;
    }
    return backup;
  }
}
