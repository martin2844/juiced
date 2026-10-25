package org.openjuice.android;

import java.io.*;
import java.util.zip.*;

final class SaveBackup {
  private static final String[] ROOTS = {"saves", "ModSaves", "reg.cf1"};

  private static void zip(File f, String name, ZipOutputStream out) throws IOException {
    if (f.isDirectory()) {
      File[] list = f.listFiles();
      if (list == null) throw new IOException("Cannot read saves");
      for (File child : list) zip(child, name + "/" + child.getName(), out);
    } else {
      out.putNextEntry(new ZipEntry(name));
      try (InputStream in = new FileInputStream(f)) {
        byte[] buffer = new byte[65536];
        int count;
        while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
      }
      out.closeEntry();
    }
  }

  static void export(File files, OutputStream dest) throws IOException {
    try (ZipOutputStream out = new ZipOutputStream(dest)) {
      out.putNextEntry(new ZipEntry("openjuice-save-backup-v1"));
      out.closeEntry();
      for (String name : ROOTS) {
        File f = new File(files, name);
        if (f.exists()) zip(f, name, out);
      }
    }
  }

  static void recover(File files) throws IOException {
    File backup = new File(files, "save-previous");
    if (new File(backup, "ready").exists()) {
      for (String name : ROOTS) {
        File live = new File(files, name), old = new File(backup, name);
        ModStore.delete(live);
        if (old.exists()) ModStore.copy(old, live);
      }
      if (!new File(backup, "ready").delete()) throw new IOException("Cannot finish save recovery");
    }
    ModStore.delete(backup);
  }

  static void restore(File files, InputStream source) throws IOException {
    recover(files);
    File stage = new File(files, "save-import"), backup = new File(files, "save-previous");
    ModStore.delete(stage);
    stage.mkdirs();
    try {
      ModStore.unpack(
          source, stage, b -> ImportJob.progress("Reading backup · " + b / (1024 * 1024) + " MiB"));
      if (!new File(stage, "openjuice-save-backup-v1").isFile())
        throw new IOException("Not an OpenJuice save backup");
      for (File f : stage.listFiles())
        if (!java.util.Arrays.asList("saves", "ModSaves", "reg.cf1", "openjuice-save-backup-v1")
            .contains(f.getName())) throw new IOException("Unexpected backup entry");
      backup.mkdirs();
      for (String name : ROOTS) {
        File live = new File(files, name);
        if (live.exists()) ModStore.copy(live, new File(backup, name));
      }
      if (!new File(backup, "ready").createNewFile())
        throw new IOException("Cannot prepare save recovery");
      try {
        for (String name : ROOTS) {
          File live = new File(files, name), next = new File(stage, name);
          ModStore.delete(live);
          if (next.exists()) ModStore.copy(next, live);
        }
        if (!new File(backup, "ready").delete()) throw new IOException("Cannot commit saves");
      } catch (IOException e) {
        recover(files);
        throw e;
      }
      ModStore.delete(backup);
    } finally {
      ModStore.delete(stage);
    }
  }
}
