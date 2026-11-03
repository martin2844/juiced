package org.openjuice.android;

/** Debug APK only: launch deterministic native smoke tests through the app's own UID. */
public final class TestLaunchActivity extends android.app.Activity {
  @Override
  public void onCreate(android.os.Bundle state) {
    super.onCreate(state);
    android.content.Intent game = new android.content.Intent(this, JuicedActivity.class);
    game.putExtra("mode", getIntent().getStringExtra("mode"));
    game.putExtra("isolate", getIntent().getBooleanExtra("isolate", false));
    game.putExtra("track", getIntent().getStringExtra("track"));
    game.putExtra("car", getIntent().getStringExtra("car"));
    game.putExtra("frames", getIntent().getStringExtra("frames"));
    startActivity(game);
    finish();
  }
}
