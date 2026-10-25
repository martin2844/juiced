package org.openjuice.android;

import android.content.Context;
import android.graphics.*;
import android.view.*;

final class TouchView extends View {
  TouchConfig config;
  boolean driving, editing;
  int selected = -1;
  java.util.function.IntConsumer selection;
  private final Paint paint = new Paint(3);
  private final TouchState state;
  private int drag = -1;

  TouchView(Context c, TouchConfig config, boolean editing, TouchState.Sink sink) {
    super(c);
    this.config = config;
    this.editing = editing;
    this.driving = editing;
    state = new TouchState(sink);
    setContentDescription(
        editing ? "Touch layout preview. Drag buttons to reposition." : "On-screen game controls");
    setFocusable(false);
  }

  void clear() {
    state.clear();
    invalidate();
  }

  void driving(boolean value) {
    if (driving != value) {
      clear();
      driving = value;
      invalidate();
    }
  }

  private float radius() {
    return Math.min(getWidth(), getHeight()) * .067f * config.size;
  }

  private float x(int i) {
    return Math.max(
        radius(),
        Math.min(
            getWidth() - radius(),
            (driving ? config.positions[i][0] : TouchConfig.MENU[i][0]) * getWidth()));
  }

  private float y(int i) {
    return Math.max(
        radius(),
        Math.min(
            getHeight() - radius(),
            (driving ? config.positions[i][1] : TouchConfig.MENU[i][1]) * getHeight()));
  }

  private int action(int i) {
    return driving ? config.actions[i] : TouchConfig.MENU_ACTIONS[i];
  }

  private int hit(float x, float y) {
    int nearest = -1;
    float distance = Float.MAX_VALUE;
    for (int i = (driving ? 12 : TouchConfig.MENU.length) - 1; i >= 0; i--) {
      float dx = x - x(i), dy = y - y(i);
      float hitRadius = editing ? Math.max(radius(), Ui.dp(getContext(), 22)) : radius() * 1.14f;
      float d = dx * dx + dy * dy;
      if (d <= hitRadius * hitRadius && d < distance) {
        nearest = i;
        distance = d;
      }
    }
    return nearest;
  }

  @Override
  protected void onDraw(Canvas c) {
    super.onDraw(c);
    if (editing) {
      c.drawColor(Ui.INK);
      paint.setColor(0xff465054);
      paint.setStrokeWidth(2);
      c.drawLine(getWidth() * .4f, 0, getWidth() * .22f, getHeight(), paint);
      c.drawLine(getWidth() * .6f, 0, getWidth() * .78f, getHeight(), paint);
      paint.setColor(0xffbec5bf);
      paint.setTextAlign(Paint.Align.CENTER);
      paint.setTextSize(Ui.dp(getContext(), 13));
      c.drawText(
          driving ? "Drag a button • Tap to remap" : "Menu preview • Up / down / left / right",
          getWidth() / 2f,
          getHeight() / 2f,
          paint);
    }
    for (int i = 0; i < (driving ? 12 : TouchConfig.MENU.length); i++) {
      int a = action(i);
      boolean down = state.held(a);
      float r = radius();
      paint.setStyle(Paint.Style.FILL);
      paint.setColor(down || editing && selected == i ? Ui.ORANGE : 0xff202a30);
      paint.setAlpha(down ? 225 : (int) (255 * config.opacity));
      c.drawCircle(x(i), y(i), r, paint);
      paint.setStyle(Paint.Style.STROKE);
      paint.setStrokeWidth(Ui.dp(getContext(), down ? 2 : 1));
      paint.setColor(down ? 0xffffcbb0 : 0xffe8eee9);
      paint.setAlpha((int) (255 * config.opacity));
      c.drawCircle(x(i), y(i), r, paint);
      paint.setStyle(Paint.Style.FILL);
      paint.setColor(Ui.WHITE);
      paint.setAlpha(230);
      paint.setTextAlign(Paint.Align.CENTER);
      paint.setTypeface(Typeface.create("sans-serif-medium", 0));
      String label =
          !driving && i == 2
              ? "▲"
              : !driving && i == 3 ? "▼" : driving && a == 5 ? "Pause" : TouchConfig.SHORT[a];
      paint.setTextSize(Math.min(Ui.dp(getContext(), 14), r * .45f));
      c.drawText(label, x(i), y(i) - (paint.ascent() + paint.descent()) / 2, paint);
    }
  }

  @Override
  public boolean onTouchEvent(MotionEvent e) {
    int index = e.getActionIndex(), kind = e.getActionMasked();
    if (kind == MotionEvent.ACTION_CANCEL) {
      clear();
      drag = -1;
      return true;
    }
    if (editing) {
      if (!driving) return true;
      if (kind == MotionEvent.ACTION_DOWN) {
        drag = hit(e.getX(), e.getY());
        if (drag >= 0) getParent().requestDisallowInterceptTouchEvent(true);
        selected = drag;
        if (selection != null) selection.accept(selected);
      } else if (kind == MotionEvent.ACTION_MOVE && drag >= 0) {
        config.positions[drag][0] = Math.max(.04f, Math.min(.96f, e.getX() / getWidth()));
        config.positions[drag][1] = Math.max(.08f, Math.min(.92f, e.getY() / getHeight()));
      } else if (kind == MotionEvent.ACTION_UP) {
        drag = -1;
        getParent().requestDisallowInterceptTouchEvent(false);
      }
      invalidate();
      return true;
    }
    if (kind == MotionEvent.ACTION_UP || kind == MotionEvent.ACTION_POINTER_UP)
      state.release(e.getPointerId(index));
    else
      for (int i = 0; i < e.getPointerCount(); i++) {
        int slot = hit(e.getX(i), e.getY(i));
        state.update(e.getPointerId(i), slot < 0 ? -1 : action(slot));
      }
    invalidate();
    return true;
  }

  @Override
  protected void onWindowVisibilityChanged(int visibility) {
    super.onWindowVisibilityChanged(visibility);
    if (visibility != VISIBLE && state != null) clear();
  }

  @Override
  protected void onDetachedFromWindow() {
    clear();
    super.onDetachedFromWindow();
  }
}
