package org.openjuice.android;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.*;
import android.view.*;
import android.widget.*;

final class Ui {
  static final int INK = 0xff22272a,
      PAPER = 0xfff1eee7,
      MUTED = 0xff657075,
      ORANGE = 0xffd84617,
      LINE = 0xffd5d6d0,
      WHITE = 0xffffffff;

  static int dp(Context c, float v) {
    return Math.round(v * c.getResources().getDisplayMetrics().density);
  }

  static LinearLayout column(Context c) {
    LinearLayout l = new LinearLayout(c);
    l.setOrientation(1);
    return l;
  }

  static TextView label(Context c, String text, int size, int color) {
    TextView t = new TextView(c);
    t.setText(text);
    t.setTextSize(size);
    t.setTextColor(color);
    t.setPadding(0, dp(c, 4), 0, dp(c, 8));
    return t;
  }

  static TextView title(Context c, String text) {
    TextView t = label(c, text, 28, INK);
    t.setTypeface(Typeface.create("sans-serif-condensed", Typeface.BOLD));
    return t;
  }

  static GradientDrawable background(Context c, int color, int stroke) {
    GradientDrawable d = new GradientDrawable();
    d.setColor(color);
    d.setCornerRadius(dp(c, 8));
    if (stroke != 0) d.setStroke(dp(c, 1), stroke);
    return d;
  }

  static void style(Button b, boolean primary) {
    Context c = b.getContext();
    int[][] states = {
      {android.R.attr.state_focused},
      {android.R.attr.state_pressed},
      {-android.R.attr.state_enabled},
      {}
    };
    StateListDrawable bg = new StateListDrawable();
    int[] colors = {0xffffd6bd, 0xffedb090, 0xffdfdfd8, primary ? ORANGE : WHITE};
    for (int i = 0; i < states.length; i++) {
      GradientDrawable d = background(c, colors[i], i == 0 ? ORANGE : primary ? 0 : LINE);
      if (i == 0) d.setStroke(dp(c, 3), ORANGE);
      bg.addState(states[i], d);
    }
    b.setStateListAnimator(null);
    b.setElevation(0);
    b.setBackground(bg);
    b.setTextColor(new ColorStateList(states, new int[] {INK, INK, MUTED, primary ? WHITE : INK}));
    b.setAllCaps(false);
    b.setTextSize(15);
    b.setMinHeight(dp(c, 48));
    b.setMinimumHeight(dp(c, 48));
    b.setPadding(dp(c, 16), dp(c, 8), dp(c, 16), dp(c, 8));
  }

  static Button button(Context c, String text, boolean primary, Runnable action) {
    Button b = new Button(c);
    b.setText(text);
    style(b, primary);
    b.setOnClickListener(v -> action.run());
    return b;
  }

  static void add(LinearLayout l, View v) {
    LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-1, -2);
    p.bottomMargin = dp(l.getContext(), 10);
    l.addView(v, p);
  }

  static void divider(LinearLayout l) {
    View v = new View(l.getContext());
    v.setBackgroundColor(LINE);
    LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-1, dp(l.getContext(), 1));
    p.setMargins(0, dp(l.getContext(), 12), 0, dp(l.getContext(), 12));
    l.addView(v, p);
  }

  static LinearLayout section(Context c, String title, String detail) {
    LinearLayout l = column(c);
    l.addView(Ui.title(c, title));
    if (!detail.isEmpty()) l.addView(label(c, detail, 15, MUTED));
    return l;
  }

  static void insets(View v, int padding) {
    v.setOnApplyWindowInsetsListener(
        (view, i) -> {
          view.setPadding(
              dp(view.getContext(), padding) + i.getSystemWindowInsetLeft(),
              dp(view.getContext(), padding) + i.getSystemWindowInsetTop(),
              dp(view.getContext(), padding) + i.getSystemWindowInsetRight(),
              dp(view.getContext(), padding) + i.getSystemWindowInsetBottom());
          return i;
        });
  }

  static LinearLayout editor(Activity a, String title, Runnable save) {
    a.getWindow().setStatusBarColor(PAPER);
    a.getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR);
    LinearLayout root = column(a);
    root.setBackgroundColor(PAPER);
    root.setPadding(dp(a, 18), dp(a, 12), dp(a, 18), dp(a, 12));
    insets(root, 16);
    LinearLayout head = new LinearLayout(a);
    Button back = button(a, "Back", false, a::finish);
    back.setPadding(dp(a, 4), 0, dp(a, 4), 0);
    back.setSingleLine(true);
    head.addView(back, new LinearLayout.LayoutParams(dp(a, 72), dp(a, 48)));
    TextView name = Ui.title(a, title);
    if (a.getResources().getConfiguration().screenWidthDp < 600) name.setTextSize(21);
    name.setPadding(dp(a, 10), 0, 0, 0);
    head.addView(name, new LinearLayout.LayoutParams(0, -2, 1));
    if (save != null)
      head.addView(
          button(a, "Save", true, save), new LinearLayout.LayoutParams(dp(a, 72), dp(a, 48)));
    root.addView(head);
    a.setContentView(root);
    return root;
  }

  static Spinner spinner(Context c, String[] values) {
    Spinner s = new Spinner(c);
    s.setAdapter(
        new ArrayAdapter<String>(c, android.R.layout.simple_spinner_dropdown_item, values) {
          public View getView(int p, View v, android.view.ViewGroup g) {
            TextView t = (TextView) super.getView(p, v, g);
            t.setTextColor(INK);
            t.setTextSize(15);
            t.setPadding(dp(c, 12), dp(c, 12), dp(c, 12), dp(c, 12));
            return t;
          }

          public View getDropDownView(int p, View v, android.view.ViewGroup g) {
            TextView t = (TextView) super.getDropDownView(p, v, g);
            t.setTextColor(INK);
            t.setBackgroundColor(WHITE);
            t.setMinHeight(dp(c, 48));
            return t;
          }
        });
    s.setBackground(background(c, WHITE, LINE));
    return s;
  }
}
