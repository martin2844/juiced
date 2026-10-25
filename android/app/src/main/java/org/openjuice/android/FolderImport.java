package org.openjuice.android;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.*;

final class FolderImport {
  private long bytes;
  private int entries;

  void copy(ContentResolver resolver, Uri tree, File target) throws IOException {
    walk(resolver, tree, DocumentsContract.getTreeDocumentId(tree), target, 0);
  }

  private void walk(ContentResolver r, Uri tree, String id, File dir, int depth)
      throws IOException {
    if (depth > 24) throw new IOException("Folder nesting is too deep");
    dir.mkdirs();
    Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, id);
    String[] columns = {
      DocumentsContract.Document.COLUMN_DOCUMENT_ID,
      DocumentsContract.Document.COLUMN_DISPLAY_NAME,
      DocumentsContract.Document.COLUMN_MIME_TYPE
    };
    try (Cursor c = r.query(children, columns, null, null, null)) {
      if (c == null) throw new IOException("Folder access was lost; select it again");
      while (c.moveToNext()) {
        if (++entries > 30000) throw new IOException("Too many mod files");
        String childId = c.getString(0), name = c.getString(1), mime = c.getString(2);
        if (name == null || name.contains("/") || name.contains("\\"))
          throw new IOException("Invalid file name");
        if (name.startsWith(".") || name.equals("tools")) continue;
        File out = ModStore.child(dir, name);
        if (out.exists()) throw new IOException("Duplicate file name");
        if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime))
          walk(r, tree, childId, out, depth + 1);
        else
          try (InputStream in =
                  r.openInputStream(DocumentsContract.buildDocumentUriUsingTree(tree, childId));
              OutputStream dest = new BufferedOutputStream(new FileOutputStream(out))) {
            if (in == null) throw new IOException("Cannot open " + name);
            byte[] buffer = new byte[65536];
            int n;
            while ((n = in.read(buffer)) != -1) {
              bytes += n;
              if (bytes > ModStore.LIMIT) throw new IOException("Mods exceed 2 GiB limit");
              dest.write(buffer, 0, n);
              ImportJob.progress("Reading mods · " + bytes / (1024 * 1024) + " MiB");
            }
          }
      }
    }
  }
}
