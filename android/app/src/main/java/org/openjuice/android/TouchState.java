package org.openjuice.android;

import java.util.*;

/** Per-pointer actions with aggregate key ownership, including overlapping remaps. */
final class TouchState {
  interface Sink {
    void key(int code, boolean down);
  }

  private final Map<Integer, Integer> fingers = new HashMap<>();
  private final Set<Integer> pressed = new HashSet<>();
  private final Sink sink;

  TouchState(Sink sink) {
    this.sink = sink;
  }

  void update(int pointer, int action) {
    if (action < 0) fingers.remove(pointer);
    else fingers.put(pointer, action);
    sync();
  }

  void release(int pointer) {
    fingers.remove(pointer);
    sync();
  }

  void clear() {
    fingers.clear();
    sync();
  }

  boolean held(int action) {
    return fingers.containsValue(action);
  }

  private void sync() {
    Set<Integer> next = new HashSet<>();
    for (int a : fingers.values()) for (int k : TouchConfig.KEYS[a]) next.add(k);
    for (int k : pressed) if (!next.contains(k)) sink.key(k, false);
    for (int k : next) if (!pressed.contains(k)) sink.key(k, true);
    pressed.clear();
    pressed.addAll(next);
  }
}
