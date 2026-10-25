package org.openjuice.android;

import java.io.*;
import java.nio.file.*;
import java.util.zip.*;

public class ModStoreTest {
  static void check(boolean b, String why) {
    if (!b) throw new AssertionError(why);
  }

  static void write(File f, String s) throws IOException {
    f.getParentFile().mkdirs();
    Files.writeString(f.toPath(), s);
  }

  static File pkg(File root, String id) throws IOException {
    File p = new File(root, id);
    write(new File(p, "mod.ini"), "name=Test\nversion=1\nkind=script\n");
    write(new File(p, "main.lua"), "return {api=1}");
    return p;
  }

  public static void main(String[] args) throws Exception {
    File files = Files.createTempDirectory("oj-mods-").toFile();
    try {
      File imported = new File(files, "mods-import");
      pkg(imported, "test");
      ModStore.install(files, imported, f -> "OK: test");
      check(new File(files, "mods/test/main.lua").exists(), "installed");
      write(new File(files, "mods/test/disabled"), "");
      write(new File(imported, "test/main.lua"), "updated");
      ModStore.install(files, imported, f -> "OK: updated");
      check(new File(files, "mods/test/disabled").exists(), "preserve disabled");
      try {
        ModStore.install(files, imported, f -> "ERROR: invalid script");
        throw new AssertionError("bad accepted");
      } catch (IOException expected) {
      }
      check(
          Files.readString(new File(files, "mods/test/main.lua").toPath()).equals("updated"),
          "previous installation intact");
      check(!new File(files, "mods-next").exists(), "failed stage cleaned");
      check(
          new File(files, "mods").renameTo(new File(files, "mods-previous")),
          "simulate interruption");
      ModStore.recover(files);
      check(new File(files, "mods/test/main.lua").exists(), "recover interrupted activation");
      ByteArrayOutputStream bytes = new ByteArrayOutputStream();
      try (ZipOutputStream zip = new ZipOutputStream(bytes)) {
        zip.putNextEntry(new ZipEntry("../escape"));
        zip.write(1);
        zip.closeEntry();
      }
      try {
        ModStore.unpack(new ByteArrayInputStream(bytes.toByteArray()), imported, b -> {});
        throw new AssertionError("traversal accepted");
      } catch (IOException expected) {
      }
      write(new File(files, "saves/SAVE"), "career");
      write(new File(files, "ModSaves/SAVE"), "mod career");
      bytes.reset();
      SaveBackup.export(files, bytes);
      write(new File(files, "saves/SAVE"), "changed");
      SaveBackup.restore(files, new ByteArrayInputStream(bytes.toByteArray()));
      check(
          Files.readString(new File(files, "saves/SAVE").toPath()).equals("career"),
          "stock restore");
      check(
          Files.readString(new File(files, "ModSaves/SAVE").toPath()).equals("mod career"),
          "mod restore");
      try {
        SaveBackup.restore(files, new ByteArrayInputStream(new byte[] {1, 2}));
        throw new AssertionError("bad backup accepted");
      } catch (IOException expected) {
      }
      check(
          Files.readString(new File(files, "saves/SAVE").toPath()).equals("career"),
          "bad backup preserves saves");
      System.out.println(
          "PASS: mod validation rollback, toggle preservation, interrupted install recovery,"
              + " traversal, save roundtrip and invalid backup preservation");
    } finally {
      ModStore.delete(files);
    }
  }
}
