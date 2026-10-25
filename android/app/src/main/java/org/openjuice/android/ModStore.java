package org.openjuice.android;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

/** Transactional package storage, independent of Android so failure paths run on the host. */
public final class ModStore {
  public interface Validator {
    String check(File folder) throws Exception;
  }

  public interface Progress {
    void update(long bytes) throws IOException;
  }

  public static final long LIMIT = 2L * 1024 * 1024 * 1024;

  public static File child(File root, String path) throws IOException {
    if (path.startsWith("/") || path.contains("\\") || path.contains(":"))
      throw new IOException("Invalid path: " + path);
    File f = new File(root, path);
    if (!f.getCanonicalPath().startsWith(root.getCanonicalPath() + File.separator))
      throw new IOException("Unsafe path: " + path);
    return f;
  }

  public static void delete(File f) throws IOException {
    if (!f.exists()) return;
    if (Files.isSymbolicLink(f.toPath())) throw new IOException("Symbolic link not allowed");
    File[] children = f.listFiles();
    if (children != null) for (File c : children) delete(c);
    if (!f.delete()) throw new IOException("Cannot remove " + f.getName());
  }

  public static void unpack(InputStream stream, File target, Progress progress) throws IOException {
    long total = 0;
    int entries = 0;
    try (ZipInputStream in = new ZipInputStream(new BufferedInputStream(stream))) {
      ZipEntry entry;
      byte[] buffer = new byte[65536];
      Set<String> names = new HashSet<>();
      while ((entry = in.getNextEntry()) != null) {
        if (++entries > 30000) throw new IOException("Too many package files");
        File out = child(target, entry.getName());
        if (!names.add(entry.getName().toLowerCase(Locale.ROOT)))
          throw new IOException("Duplicate package path");
        if (entry.isDirectory()) {
          out.mkdirs();
          continue;
        }
        out.getParentFile().mkdirs();
        try (OutputStream dest = new BufferedOutputStream(new FileOutputStream(out))) {
          int n;
          while ((n = in.read(buffer)) != -1) {
            total += n;
            if (total > LIMIT) throw new IOException("Mods exceed 2 GiB limit");
            dest.write(buffer, 0, n);
            progress.update(total);
          }
        }
      }
    }
  }

  public static List<File> packages(File root) {
    List<File> result = new ArrayList<>();
    File[] files = root.listFiles();
    if (files != null)
      for (File f : files)
        if (f.isDirectory()
            && f.getName().matches("[a-z0-9_-]+")
            && new File(f, "mod.ini").isFile()) result.add(f);
    result.sort(Comparator.comparing(File::getName));
    return result;
  }

  public static Properties manifest(File folder) throws IOException {
    Properties p = new Properties();
    try (Reader r =
        new InputStreamReader(
            new FileInputStream(new File(folder, "mod.ini")),
            java.nio.charset.StandardCharsets.UTF_8)) {
      p.load(r);
    }
    if (!"1".equals(p.getProperty("version"))
        || !("assets".equals(p.getProperty("kind")) || "script".equals(p.getProperty("kind"))))
      throw new IOException("Unsupported package: " + folder.getName());
    if ("script".equals(p.getProperty("kind")) && !new File(folder, "main.lua").isFile())
      throw new IOException("Missing script: " + folder.getName());
    return p;
  }

  public static void copy(File from, File to) throws IOException {
    if (Files.isSymbolicLink(from.toPath()))
      throw new IOException("Symbolic links are not supported");
    if (from.isDirectory()) {
      to.mkdirs();
      File[] entries = from.listFiles();
      if (entries == null) throw new IOException("Cannot read folder");
      for (File f : entries)
        if (!f.getName().startsWith(".") && !f.getName().equals("tools"))
          copy(f, child(to, f.getName()));
    } else {
      to.getParentFile().mkdirs();
      Files.copy(from.toPath(), to.toPath(), StandardCopyOption.REPLACE_EXISTING);
    }
  }

  public static void recover(File files) throws IOException {
    File live = new File(files, "mods"), previous = new File(files, "mods-previous");
    if (!live.exists() && previous.exists() && !previous.renameTo(live))
      throw new IOException("Cannot recover mods");
  }

  public static String install(File files, File imported, Validator validator) throws Exception {
    recover(files);
    File root = imported;
    if (packages(root).isEmpty() && !new File(root, "mod.ini").exists()) {
      File[] children = root.listFiles();
      if (children != null)
        for (File c : children)
          if (c.isDirectory() && (!packages(c).isEmpty() || new File(c, "mod.ini").isFile())) {
            root = c;
            break;
          }
    }
    boolean single = new File(root, "mod.ini").isFile();
    List<File> incoming = single ? Collections.singletonList(root) : packages(root);
    if (incoming.isEmpty())
      throw new IOException(
          "No mod.ini packages found. Select the mods folder or a packaged mod ZIP.");
    File next = new File(files, "mods-next"),
        live = new File(files, "mods"),
        previous = new File(files, "mods-previous");
    delete(next);
    next.mkdirs();
    try {
      if (live.exists()) copy(live, next);
      for (File pkg : incoming) {
        manifest(pkg);
        String id = pkg.getName();
        if (!id.matches("[a-z0-9_-]+") || id.equals("mods-import"))
          throw new IOException("A single mod ZIP must contain its named package folder");
        File dest = child(next, id);
        boolean disabled = new File(dest, "disabled").exists();
        delete(dest);
        copy(pkg, dest);
        if (disabled) new File(dest, "disabled").createNewFile();
      }
      File collection = new File(root, "collection.ini");
      if (collection.isFile()) copy(collection, new File(next, "collection.ini"));
      String result = validator.check(next);
      if (!result.startsWith("OK:")) throw new IOException(result);
      delete(previous);
      if (live.exists() && !live.renameTo(previous))
        throw new IOException("Cannot preserve existing mods");
      if (!next.renameTo(live)) {
        previous.renameTo(live);
        throw new IOException("Cannot activate mods");
      }
      delete(previous);
      return result;
    } finally {
      delete(next);
    }
  }
}
