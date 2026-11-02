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
      // Updating a package must not need to read/copy the outgoing payload.
      File outgoing = new File(files, "mods/test/main.lua");
      check(outgoing.setReadable(false, false), "make outgoing payload unreadable");
      ModStore.install(files, imported, f -> "OK: updated");
      check(new File(files, "mods/test/disabled").exists(), "preserve disabled");
      check(new File(files, "mods/test/disabled").delete(), "enable existing package");
      write(new File(imported, "test/disabled"), "disabled by package author");
      pkg(new File(files, "mods"), "untouched");
      write(new File(files, "mods/collection.ini"), "collection preferences");
      ModStore.install(files, imported, f -> "OK: keep user choices");
      check(!new File(files, "mods/test/disabled").exists(), "preserve enabled despite imported marker");
      check(new File(files, "mods/untouched/main.lua").isFile(), "unreplaced package preserved");
      check(Files.readString(new File(files, "mods/collection.ini").toPath()).equals("collection preferences"),
          "collection preferences preserved");
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
      File fixture = new File(files, "unreadable-fixture"), locked = new File(fixture, "mods");
      pkg(locked, "untouched");
      File replacement = new File(fixture, "incoming");
      pkg(replacement, "test");
      check(locked.setReadable(false, false), "make existing package list unreadable");
      boolean[] validated = {false};
      try {
        check(locked.listFiles() == null, "unreadable directory fixture effective");
        try {
          ModStore.install(fixture, replacement, f -> { validated[0] = true; return "OK: test"; });
          throw new AssertionError("unreadable existing mods accepted");
        } catch (IOException expected) {
        }
      } finally {
        locked.setReadable(true, false);
        new File(fixture, "mods-previous").setReadable(true, false);
      }
      check(!validated[0], "unreadable existing package list rejected before validation/activation");
      check(new File(locked, "untouched/main.lua").isFile(), "unreadable installation preserved");
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
