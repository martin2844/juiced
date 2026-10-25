package org.openjuice.android;

import java.nio.file.*;
import java.util.*;

public final class TouchStateTest {
  static void check(boolean b, String m) {
    if (!b) throw new AssertionError(m);
  }

  public static void main(String[] args) throws Exception {
    Set<Integer> down = new HashSet<>();
    TouchState s =
        new TouchState(
            (k, d) -> {
              if (d) down.add(k);
              else down.remove(k);
            });
    s.update(1, 0);
    s.update(2, 2);
    check(down.contains(1000) && down.contains(1002), "steer and throttle together");
    s.update(3, 2);
    s.release(2);
    check(down.contains(1002), "second finger retains throttle");
    s.update(1, 1);
    check(!down.contains(1000) && down.contains(1001), "drag changes direction");
    s.update(4, 5);
    s.update(5, 5);
    s.release(4);
    check(down.contains(111), "shared remapped action stays held");
    s.update(3, -1);
    check(!down.contains(1002), "outside releases pedal");
    s.clear();
    s.update(1, 14);
    check(down.contains(19) && !down.contains(1002), "menu up must not type W");
    s.clear();
    check(down.isEmpty(), "cancel/focus loss releases all keys");
    Path dir = Files.createTempDirectory("oj-touch-");
    TouchConfig c = new TouchConfig();
    c.visibility = 1;
    c.positions[0][0] = .3f;
    c.actions[0] = 6;
    c.opacity = .4f;
    c.save(dir.toFile());
    TouchConfig loaded = TouchConfig.load(dir.toFile());
    check(
        loaded.visibility == 1 && loaded.actions[0] == 6 && loaded.positions[0][0] == .3f,
        "layout and remap round trip");
    Files.writeString(
        dir.resolve("touch.properties"), "size=NaN\nopacity=Infinity\n0.x=-500\n0.action=999\n");
    loaded = TouchConfig.load(dir.toFile());
    check(
        loaded.size == 1
            && loaded.opacity == .65f
            && loaded.positions[0][0] == .04f
            && loaded.actions[0] == TouchConfig.ACTIONS.length - 1,
        "invalid settings bounded");
    Files.delete(dir.resolve("touch.properties"));
    Files.delete(dir);
    System.out.println(
        "Touch input ownership, multitouch, remapping, drag, cancellation and configuration"
            + " passed");
  }
}
