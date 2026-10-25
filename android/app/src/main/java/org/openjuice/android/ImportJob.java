package org.openjuice.android;

import java.io.IOException;
import java.util.concurrent.*;

final class ImportJob {
  interface Work {
    String run() throws Exception;
  }

  private static final ExecutorService worker = Executors.newSingleThreadExecutor();
  static java.io.File lockFile;
  static volatile boolean busy = false, cancel = false;
  static volatile String status = "";

  static synchronized void start(Work work) {
    if (busy) return;
    busy = true;
    cancel = false;
    status = "Checking files…";
    worker.execute(
        () -> {
          try (java.io.RandomAccessFile file = new java.io.RandomAccessFile(lockFile, "rw");
              java.nio.channels.FileLock lock = file.getChannel().tryLock()) {
            if (lock == null)
              throw new IOException("Exit the game before changing the installation.");
            status = work.run();
          } catch (Exception | LinkageError e) {
            status = "Could not finish: " + e.getMessage();
          } finally {
            busy = false;
          }
        });
  }

  static void progress(String message) throws IOException {
    if (cancel || Thread.currentThread().isInterrupted())
      throw new IOException("Import cancelled; previous installation kept.");
    status = message;
  }
}
