package org.openjuice.android;

import java.io.*;
import java.nio.file.*;
import java.util.*;

final class TouchConfig {
  static final String[] ACTIONS = {
    "Steer left",
    "Steer right",
    "Accelerate",
    "Brake",
    "Select",
    "Back / pause",
    "Nitro",
    "Handbrake",
    "Gear up",
    "Gear down",
    "Camera",
    "Reverse",
    "Menu left",
    "Menu right",
    "Menu up",
    "Menu down",
    "Menu extra (Tab)",
    "Previous page",
    "Next page",
    "Paint purchase (0)"
  };
  static final String[] SHORT = {
    "◀", "▶", "Gas", "Brake", "Select", "Back", "NOS", "H.Brake", "Gear +", "Gear −", "View",
    "Reverse", "◀", "▶", "▲", "▼", "Tab", "Pg Up", "Pg Dn", "0"
  };
  // 1000..1011 are logical driving actions, resolved by the native active map.
  static final int[][] KEYS = {
    {1000}, {1001}, {1002}, {1003}, {66}, {111}, {1006}, {1007}, {1008}, {1009}, {1010}, {1011},
    {21}, {22}, {19}, {20}, {61}, {92}, {93}, {7}
  };
  static final float[][] DRIVE = {
    {.08f, .82f},
    {.21f, .82f},
    {.92f, .72f},
    {.80f, .88f},
    {.51f, .12f},
    {.92f, .12f},
    {.80f, .58f},
    {.67f, .88f},
    {.56f, .88f},
    {.44f, .88f},
    {.79f, .12f},
    {.92f, .90f}
  };
  static final float[][] MENU = {
    {.08f, .72f},
    {.30f, .72f},
    {.19f, .55f},
    {.19f, .89f},
    {.87f, .79f},
    {.73f, .79f},
    {.60f, .79f},
    {.60f, .57f},
    {.73f, .57f},
    {.87f, .57f}
  };
  static final int[] MENU_ACTIONS = {12, 13, 14, 15, 4, 5, 16, 17, 18, 19};
  static final float MIN_SIZE = .5f, MAX_SIZE = 3f;
  static final float[] DEFAULT_SIZES = {
    1.45f, 1.45f, 1.45f, 1.45f, .8f, .8f, .8f, .8f, 1.4f, 1.4f, .8f, .9f
  };
  final float[] sizes = DEFAULT_SIZES.clone();
  final float[] menuSizes = {1.4f, 1.4f, 1.4f, 1.4f, .85f, .85f, .8f, .8f, .8f, .8f};
  int visibility = 0;
  float size = 1, opacity = .65f;
  final float[][] positions = new float[12][2];
  final int[] actions = new int[12];

  TouchConfig() {
    for (int i = 0; i < 12; i++) {
      positions[i] = DRIVE[i].clone();
      actions[i] = i;
    }
  }

  static TouchConfig load(File folder) {
    TouchConfig c = new TouchConfig();
    Properties p = new Properties();
    try (FileReader r = new FileReader(new File(folder, "touch.properties"))) {
      p.load(r);
    } catch (IOException ignored) {
    }
    c.visibility = (int) number(p, "visibility", 0, 0, 2);
    c.size = number(p, "size", 1, .75f, 1.35f);
    c.opacity = number(p, "opacity", .65f, .25f, .9f);
    for (int i = 0; i < 12; i++) {
      c.positions[i][0] = number(p, i + ".x", DRIVE[i][0], .04f, .96f);
      c.positions[i][1] = number(p, i + ".y", DRIVE[i][1], .08f, .92f);
      c.actions[i] = (int) number(p, i + ".action", i, 0, ACTIONS.length - 1);
    }
    for (int i = 0; i < c.sizes.length; i++)
      c.sizes[i] = number(p, i + ".size", c.sizes[i] * c.size, MIN_SIZE, MAX_SIZE);
    for (int i = 0; i < c.menuSizes.length; i++)
      c.menuSizes[i] =
          number(p, "menu." + i + ".size", c.menuSizes[i] * c.size, MIN_SIZE, MAX_SIZE);
    return c;
  }

  static float number(Properties p, String key, float fallback, float min, float max) {
    try {
      float n = Float.parseFloat(p.getProperty(key, ""));
      return Float.isFinite(n) ? Math.max(min, Math.min(max, n)) : fallback;
    } catch (NumberFormatException e) {
      return fallback;
    }
  }

  void save(File folder) throws IOException {
    Properties p = new Properties();
    p.setProperty("visibility", "" + visibility);
    p.setProperty("size", "1.0"); // Per-button sizes already include the legacy global scale.
    p.setProperty("opacity", "" + opacity);
    for (int i = 0; i < 12; i++) {
      p.setProperty(i + ".size", "" + sizes[i]);
      p.setProperty(i + ".x", "" + positions[i][0]);
      p.setProperty(i + ".y", "" + positions[i][1]);
      p.setProperty(i + ".action", "" + actions[i]);
    }
    for (int i = 0; i < menuSizes.length; i++)
      p.setProperty("menu." + i + ".size", "" + menuSizes[i]);
    File temp = new File(folder, "touch.properties.tmp");
    try (FileWriter w = new FileWriter(temp)) {
      p.store(w, "OpenJuice touch layout");
    }
    Files.move(
        temp.toPath(),
        new File(folder, "touch.properties").toPath(),
        StandardCopyOption.REPLACE_EXISTING);
  }
}
