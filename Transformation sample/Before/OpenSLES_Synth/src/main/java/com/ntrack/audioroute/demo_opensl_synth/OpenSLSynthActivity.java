/*
 * Copyright 2019 n-Track S.r.l. All Rights Reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

package com.ntrack.audioroute.demo_opensl_synth;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Menu;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;

import com.ntrack.audioroute.demo_opensl_synth.R;

public class OpenSLSynthActivity extends Activity implements OnSeekBarChangeListener {
  static {
    System.loadLibrary("demo_opensl_synth");
  }

  private static final String TAG = "SimpleSynthSample";
  public static final String MY_PREFS_NAME = "MyPrefsFile";

  private int sampleRate;

 @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    final OpenSLSynthActivity mythis=this;

    setContentView(R.layout.activity_main);
    TextView textView = (TextView) findViewById(R.id.mainText);
    SeekBar waveformBar = (SeekBar) findViewById(R.id.waveformBar);
    waveformBar.setOnSeekBarChangeListener(this);
    waveformBar.setProgress(0);
    Button button = (Button) findViewById(R.id.connectButton);
    Button hostButton = findViewById(R.id.hostButton);

    updateUI(true, false);

    ((com.ntrack.audioroute.demo_opensl_synth.Piano)findViewById(R.id.piano)).setPianoKeyListener(new com.ntrack.audioroute.demo_opensl_synth.Piano.PianoKeyListener() {
      @Override
      public void keyPressed(int id, int action) {
        sendMidiNote(id+64, (action== com.ntrack.audioroute.demo_opensl_synth.Key.ACTION_KEY_DOWN) ? 1 : 0);
      }
    });
  }
  public void checkStartAudio()
  {
    if(isResumed) startAudio();
  }

  void updateUI(boolean setProgress, boolean unused)
  {
    int mode=getWaveform();
    SeekBar waveformBar = (SeekBar) findViewById(R.id.waveformBar);
    if(setProgress&&null!=waveformBar) waveformBar.setProgress(mode==0 ? 0 : 100);

    TextView textView = (TextView) findViewById(R.id.mainText);
    if(null!=textView)
      textView.setText("Synth wave: "+(mode==1 ? "Saw tooth" : "Sine") );
    }

  native void startAudio();
  native void stopAudio();
  native void sendMidiNote(int note, int isNoteOn);
  native void setWaveform(int mode);
  native int getWaveform();
  @Override
  protected void onPause() {
    super.onPause();
    isResumed=false;

    stopAudio();
  }
  boolean isResumed=false;
  @Override
  protected void onResume() {
    super.onResume();

    isResumed=true;

    updateUI(true, false);

    startAudio();

  }
  @Override protected void onNewIntent (Intent intent)
  {
    super.onNewIntent(intent);
  }
  @Override
  protected void onDestroy() {
    super.onDestroy();
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    // Inflate the menu; this adds items to the action bar if it is present.
    getMenuInflater().inflate(R.menu.main, menu);
    return true;
  }

  @Override
  public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
      int mode=0;
      if(progress<50) mode=0;
      else mode=1;
      setWaveform(mode);
      updateUI(false, false);
  }

  @Override
  public void onStartTrackingTouch(SeekBar seekBar) {}

  @Override
  public void onStopTrackingTouch(SeekBar seekBar) {}
}

