package org.openjuice.android;

import java.io.*;
import java.nio.channels.FileLock;
import java.nio.file.Files;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.atomic.AtomicBoolean;

/** Exercise cleanup through the same job/installation-lock boundary as the launcher. */
public final class ImportJobLockTest {
  static void check(boolean value, String why) {
    if (!value) throw new AssertionError(why);
  }

  static void waitForJob() throws Exception {
    long deadline = System.nanoTime() + 5_000_000_000L;
    while (ImportJob.busy && System.nanoTime() < deadline) Thread.sleep(5);
    check(!ImportJob.busy, "job completed before timeout");
  }

  public static void main(String[] args) throws Exception {
    File files = Files.createTempDirectory("oj-import-lock-").toFile();
    try {
      File cache = new File(files, "mods/.cache/stage-crashed/derived.dat");
      File mod = new File(files, "mods/test/main.lua");
      File save = new File(files, "ModSaves/profile.dat");
      File other = new File(files, "mods/.preferences");
      for (File path : new File[] {cache, mod, save, other}) {
        path.getParentFile().mkdirs();
        Files.writeString(path.toPath(), path.getName());
      }
      ImportJob.lockFile = new File(files, "installation.lock");
      AtomicBoolean ran = new AtomicBoolean();
      try (RandomAccessFile lockFile = new RandomAccessFile(ImportJob.lockFile, "rw");
          FileLock lock = lockFile.getChannel().lock()) {
        ImportJob.start(() -> { ran.set(true); return "must not run"; });
        waitForJob();
        check(!ran.get() && cache.isFile(), "active installation lock protects overlays");
      }
      ImportJob.start(() -> {
        check(!cache.exists(), "stale overlays removed before work begins");
        ran.set(true);
        return "OK: cache cleared under lock";
      });
      waitForJob();
      check(ran.get() && ImportJob.status.startsWith("OK:"), "job runs after game releases lock");
      for (File path : new File[] {mod, save, other})
        check(Files.readString(path.toPath()).equals(path.getName()), "only derived overlays removed");
      System.out.println("PASS: live installation lock prevents cleanup; stale overlays cleared under lock; mods/saves/preferences preserved");
    } finally {
      var field = ImportJob.class.getDeclaredField("worker");
      field.setAccessible(true);
      ((ExecutorService) field.get(null)).shutdownNow();
      ModStore.delete(files);
    }
  }
}
