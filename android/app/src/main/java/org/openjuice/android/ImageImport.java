package org.openjuice.android;

import android.content.ContentResolver;
import android.graphics.*;
import android.media.ExifInterface;
import android.net.Uri;
import java.io.*;
import java.nio.*;

final class ImageImport {
  static String decode(ContentResolver resolver, Uri uri, File directory, boolean portrait)
      throws IOException {
    File input = File.createTempFile("photo-", ".image", directory.getParentFile());
    try {
      try (InputStream in = resolver.openInputStream(uri);
          OutputStream out = new FileOutputStream(input)) {
        if (in == null) throw new IOException("Cannot open image");
        byte[] buf = new byte[65536];
        int n;
        long total = 0;
        while ((n = in.read(buf)) != -1) {
          total += n;
          if (total > 50L * 1024 * 1024)
            throw new IOException("Choose an image smaller than 50 MB");
          out.write(buf, 0, n);
        }
      }
      BitmapFactory.Options opt = new BitmapFactory.Options();
      opt.inJustDecodeBounds = true;
      BitmapFactory.decodeFile(input.getPath(), opt);
      if (opt.outWidth <= 0 || opt.outHeight <= 0)
        throw new IOException("Choose a PNG or JPEG image");
      opt.inSampleSize = 1;
      while (Math.max(opt.outWidth, opt.outHeight) / opt.inSampleSize > 1024) opt.inSampleSize *= 2;
      opt.inJustDecodeBounds = false;
      Bitmap image = BitmapFactory.decodeFile(input.getPath(), opt);
      if (image == null) throw new IOException("Cannot decode image");
      Matrix orientation = new Matrix();
      try {
        int v =
            new ExifInterface(input.getPath()).getAttributeInt(ExifInterface.TAG_ORIENTATION, 1);
        switch (v) {
          case 2:
            orientation.setScale(-1, 1);
            break;
          case 3:
            orientation.setRotate(180);
            break;
          case 4:
            orientation.setScale(1, -1);
            break;
          case 5:
            orientation.setRotate(90);
            orientation.postScale(-1, 1);
            break;
          case 6:
            orientation.setRotate(90);
            break;
          case 7:
            orientation.setRotate(270);
            orientation.postScale(-1, 1);
            break;
          case 8:
            orientation.setRotate(270);
            break;
        }
      } catch (IOException ignored) {
      }
      Bitmap oriented =
          Bitmap.createBitmap(image, 0, 0, image.getWidth(), image.getHeight(), orientation, true);
      if (oriented != image) image.recycle();
      image = oriented;
      Bitmap canvas = Bitmap.createBitmap(512, 512, Bitmap.Config.ARGB_8888);
      float scale =
          portrait
              ? Math.max(512f / image.getWidth(), 512f / image.getHeight())
              : Math.min(512f / image.getWidth(), 512f / image.getHeight());
      float w = image.getWidth() * scale, h = image.getHeight() * scale;
      new Canvas(canvas)
          .drawBitmap(
              image,
              null,
              new RectF((512 - w) / 2, (512 - h) / 2, (512 + w) / 2, (512 + h) / 2),
              new Paint(Paint.FILTER_BITMAP_FLAG));
      image.recycle();
      int[] pixels = new int[512 * 512];
      canvas.getPixels(pixels, 0, 512, 0, 0, 512, 512);
      canvas.recycle();
      ByteBuffer bytes = ByteBuffer.allocate(44 + pixels.length * 4).order(ByteOrder.LITTLE_ENDIAN);
      for (int v : new int[] {21, 512, 512, 1, 2, 2, 0, 3, 3, 0, 1}) bytes.putInt(v);
      for (int v : pixels) bytes.putInt(v);
      directory.mkdirs();
      String name = "mod_image_" + java.util.UUID.randomUUID().toString().replace("-", "");
      try (OutputStream out = new FileOutputStream(new File(directory, name + ".ddt"))) {
        out.write(bytes.array());
      }
      return name;
    } finally {
      input.delete();
    }
  }
}
