/*
 * Copyright 2013 Google Inc. All Rights Reserved.
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

package com.ntrack.audioroute.simplesynth;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Menu;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;

import com.ntrack.audioroute.AudioModule;
import com.ntrack.audioroute.AudiorouteActivity;
import com.ntrack.audioroute.AudiorouteActivityController;

public class SimpleSynthActivity extends Activity implements OnSeekBarChangeListener, View.OnClickListener {
  static {
    System.loadLibrary("simplesynth");
  }

  AudiorouteActivityController controller;
  private static final String TAG = "SimpleSynthSample";
  public static final String MY_PREFS_NAME = "MyPrefsFile";

  private int sampleRate;
  boolean isAudiorouteConnected=false;

  protected String getModuleLabel()
  {
    return "simplesynth";
  }
  public Bitmap getModuleImage()
  {
    Drawable d = getResources().getDrawable(com.ntrack.audioroute.simplesynth.R.drawable.app_icon);
    Bitmap bitmap = ((BitmapDrawable)d).getBitmap();
    return bitmap;
  }
  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    controller= AudiorouteActivityController.getInstance();
    controller.setModuleLabel(getModuleLabel());
    controller.setModuleImage(getModuleImage());
    final SimpleSynthActivity mythis=this;
    controller.setListener(new AudiorouteActivityController.Listener() {
      @Override
      public void onRouteConnected() {
        isAudiorouteConnected=true;
        new Handler(Looper.getMainLooper()).post(new Runnable() {
          @Override
          public void run() {
            stopAudio();
            updateUI(true, isAudiorouteConnected);
          }
        });
      }

      @Override
      public void onRouteDisconnected() {
        isAudiorouteConnected = false;
        new Handler(Looper.getMainLooper()).post(new Runnable() {
          @Override
          public void run() {
            showHostIcon(isAudiorouteConnected);
            //checkStartAudio();
          }
        });
      }

      @Override
      public AudioModule createAudioModule() {
        return new SimpleSynthModule(2, mythis);
      }
    });
   controller.onActivityCreated(this, false);

    setContentView(R.layout.activity_main);
    TextView textView = (TextView) findViewById(R.id.mainText);
    SeekBar waveformBar = (SeekBar) findViewById(R.id.waveformBar);
    waveformBar.setOnSeekBarChangeListener(this);
    waveformBar.setProgress(0);
    Button button = (Button) findViewById(R.id.connectButton);
    Button hostButton = findViewById(R.id.hostButton);
    hostButton.setOnClickListener(this);
    Button cb = (Button) findViewById(R.id.simulate_hang);
    cb.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        ((SimpleSynthModule)controller.getModule()).setSimulateHang();
      }
    });

    isAudiorouteConnected=controller.isConnected();
    updateUI(true, isAudiorouteConnected);

    ((Piano)findViewById(R.id.piano)).setPianoKeyListener(new Piano.PianoKeyListener() {
      @Override
      public void keyPressed(int id, int action) {
        sendMidiNote(id+64, (action==Key.ACTION_KEY_DOWN) ? 1 : 0);
      }
    });
  }
  public void checkStartAudio()
  {
    if(isResumed) startAudio();
  }

  int getCurrentInstanceId()
  {
    if(controller!=null&&controller.getModule()!=null&&controller.getModule().isConfigured()) {
      return controller.getModule().getCurrentInstanceId();
    }
    return 0;
  }
  void updateUI(boolean setProgress, boolean isAudioRouteHosted)
  {
    TextView textView = (TextView) findViewById(R.id.instance_label);
    int instance=getCurrentInstanceId();
    int mode=0;
    mode=getWaveform(instance);
    if(textView!=null) {
        textView.setText("Instance: "+Integer.toString(instance));
    }
    SeekBar waveformBar = (SeekBar) findViewById(R.id.waveformBar);
    if(setProgress&&null!=waveformBar) waveformBar.setProgress(mode==0 ? 0 : 100);

    textView = (TextView) findViewById(R.id.mainText);
    if(null!=textView)
      textView.setText("Synth wave: "+(mode==1 ? "Saw tooth" : "Sine") );

    showHostIcon(isAudioRouteHosted);
  }

  void showHostIcon(boolean isAudioRouteHosted) {
    Button hostButton = findViewById(R.id.hostButton);
    if(null==hostButton) return;
    hostButton.setVisibility(isAudioRouteHosted ? View.VISIBLE : View.INVISIBLE);

    if(isAudioRouteHosted && controller!=null)
      hostButton.setBackground(controller.getHostIcon());

    // Hide the keyboard when connected to Audioroute
    // If you want to keep showing the keyboard you'll need to route the events to your
    // callback. This sample is designed to routes MIDI events only its own audio callback
    // Not to the Audioroute process callback
    findViewById(R.id.piano).setVisibility(isAudioRouteHosted ? View.INVISIBLE : View.VISIBLE);
  }

  @Override
  public void onClick(View v) {
    if(controller!=null)
      controller.switchToHostApp();
  }

  native void startAudio();
  native void stopAudio();
  native void sendMidiNote(int note, int isNoteOn);
  native void setWaveform(int instanceId, int mode);
  native int getWaveform(int instanceId);
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
    controller.onResume();

    updateUI(true, isAudiorouteConnected);

    if(!controller.isConnected()&&!controller.isConnecting()) startAudio();
  }
  @Override protected void onNewIntent (Intent intent)
  {
    super.onNewIntent(intent);
    controller.onNewIntent(intent);
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
    int instanceId=getCurrentInstanceId();
      int mode=0;
      if(progress<50) mode=0;
      else mode=1;
      setWaveform(instanceId, mode);
      updateUI(false, isAudiorouteConnected);
  }

  @Override
  public void onStartTrackingTouch(SeekBar seekBar) {}

  @Override
  public void onStopTrackingTouch(SeekBar seekBar) {}
}

