package com.ntrack.audioroute.demo_opensl_synth;

import android.graphics.drawable.Drawable;

import java.util.ArrayList;

public class Key {

    private Drawable drawable;
    private Piano piano;
    private Piano.PianoKeyListener listener;
    private ArrayList<Finger> fingers = new ArrayList<Finger>();;
    private int id;

    public static final int ACTION_KEY_DOWN = 0;
    public static final int ACTION_KEY_UP = 1;

    public Key(int id, Piano piano) {
        this.id = id;
        this.piano = piano;
    }

    public Boolean isPressed() {
        return fingers.size() > 0;
    }

    public void press(Finger finger) {
        this.fingers.add(finger);
        this.piano.invalidate();
        if (listener != null) {
            this.listener.keyPressed(id, Key.ACTION_KEY_DOWN);
        }
    }

    public void depress(Finger finger) {
        this.fingers.remove(finger);
        if (!isPressed()) {
            this.piano.invalidate();
            if (listener != null) {
                this.listener.keyPressed(id, Key.ACTION_KEY_UP);
            }
        }
    }

    public Drawable getDrawable() {
        return drawable;
    }

    public void setDrawable(Drawable drawable) {
        this.drawable = drawable;
    }

    public void setPianoKeyListener(Piano.PianoKeyListener listener) {
        this.listener = listener;
    }

}