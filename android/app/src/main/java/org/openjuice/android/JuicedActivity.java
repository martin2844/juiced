package org.openjuice.android;
import org.libsdl.app.SDLActivity;
import android.os.Bundle;
import android.view.InputDevice;
import android.view.View;
import android.view.ViewGroup;
import android.graphics.Canvas;
import android.graphics.Paint;
public final class JuicedActivity extends SDLActivity {
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        boolean controller = false;
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(id);
            if (device != null && (device.supportsSource(InputDevice.SOURCE_GAMEPAD)
                    || device.supportsSource(InputDevice.SOURCE_JOYSTICK))) controller = true;
        }
        if (!controller && mLayout != null && !"diagnostics".equals(getIntent().getStringExtra("mode"))) {
            View labels = new View(this) {
                final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
                @Override protected void onDraw(Canvas canvas) {
                    float width = getWidth(), height = getHeight();
                    float[][] boxes = {{0,.65f,.18f,1},{.18f,.65f,.36f,1},{.64f,.65f,.82f,1},
                        {.82f,.65f,1,1},{0,0,.18f,.2f},{.82f,0,1,.2f}};
                    String[] names = {"LEFT", "RIGHT", "BRAKE / DOWN", "GAS / UP", "BACK / PAUSE", "SELECT"};
                    paint.setTextSize(12 * getResources().getDisplayMetrics().scaledDensity);
                    paint.setTextAlign(Paint.Align.CENTER);
                    for (int i = 0; i < boxes.length; i++) {
                        float[] b = boxes[i];
                        paint.setColor(0x45FFFFFF); paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(2);
                        canvas.drawRoundRect(b[0]*width+8,b[1]*height+8,b[2]*width-8,b[3]*height-8,16,16,paint);
                        paint.setColor(0xAAFFFFFF); paint.setStyle(Paint.Style.FILL);
                        canvas.drawText(names[i],(b[0]+b[2])*width/2,(b[1]+b[3])*height/2,paint);
                    }
                }
            };
            labels.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            mLayout.addView(labels, new ViewGroup.LayoutParams(-1,-1));
        }
    }
    @Override public void onBackPressed() {
        if (mBrokenLibraries) { super.onBackPressed(); return; }
        onNativeKeyDown(android.view.KeyEvent.KEYCODE_ESCAPE);
        new android.os.Handler(getMainLooper()).postDelayed(
            () -> onNativeKeyUp(android.view.KeyEvent.KEYCODE_ESCAPE), 50);
    }
    @Override protected String[] getLibraries() { return new String[] {"SDL3", "main"}; }
    @Override protected String[] getArguments() { String mode = getIntent().getStringExtra("mode");
        String root = getFilesDir().getAbsolutePath();
        if ("race".equals(mode)) return new String[] {root, "-i"};
        if ("diagnostics".equals(mode)) return new String[] {root, "--diagnostics"};
        return new String[] {root}; }
}
