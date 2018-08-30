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

public class SimpleSynthActivity extends AudiorouteActivity implements OnSeekBarChangeListener, View.OnClickListener {

  private static final String TAG = "SimpleSynthSample";
  public static final String MY_PREFS_NAME = "MyPrefsFile";

  private int sampleRate;
  boolean isAudiorouteConnected=false;

  @Override protected String getModuleLabel()
  {
    return "simplesynth";
  }
  @Override public Bitmap getModuleImage()
  {
    Drawable d = getResources().getDrawable(com.ntrack.audioroute.simplesynth.R.drawable.app_icon);
    Bitmap bitmap = ((BitmapDrawable)d).getBitmap();
    return bitmap;
  }
  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
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
  }

  void updateUI(boolean setProgress, boolean isAudioRouteHosted)
  {
    TextView textView = (TextView) findViewById(R.id.instance_label);
    int instance=0;
    int mode=0;
    if(controller!=null&&controller.getModule()!=null&&controller.getModule().isConfigured()) {
      instance = controller.getModule().getCurrentInstanceId();
      mode=((com.ntrack.audioroute.simplesynth.SimpleSynthModule)controller.getModule()).getWaveform();
    }
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
  }

  @Override
  public void onClick(View v) {
    if(controller!=null)
      controller.switchToHostApp();
  }

  @Override
  protected void onResume() {
    super.onResume();

    updateUI(true, isAudiorouteConnected);
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
    if (controller.getModule() != null) {
      int mode=0;
      if(progress<50) mode=0;
      else mode=1;
      ((SimpleSynthModule)controller.getModule()).setWaveform(mode);
      updateUI(false, isAudiorouteConnected);
    }
  }

  @Override
  public void onStartTrackingTouch(SeekBar seekBar) {}

  @Override
  public void onStopTrackingTouch(SeekBar seekBar) {}

  @Override
  protected void onAudiorouteConnected() {
    this.isAudiorouteConnected=true;
    new Handler(Looper.getMainLooper()).post(new Runnable() {
      @Override
      public void run() {
        updateUI(true, isAudiorouteConnected);
      }
    });
  }
  @Override protected AudioModule createModule()
  {
    return new SimpleSynthModule(2);
  }
  @Override
  protected void onAudiorouteDisconnected() {
    this.isAudiorouteConnected = false;
    new Handler(Looper.getMainLooper()).post(new Runnable() {
      @Override
      public void run() {
        showHostIcon(isAudiorouteConnected);
      }
    });
  }

}

