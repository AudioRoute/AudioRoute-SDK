/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 * Copyright 2019 n-Track S.r.l.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


package com.ntrack.audioroute;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.view.KeyEvent;

/**
 * This convenience class encapsulates two common features of activities that use Audioroute, the
 * connection to the Audioroute service and routing of activity events to Audioroute.
 */

public abstract class AudiorouteActivity extends Activity {

  protected AudiorouteActivityController controller;

  protected abstract String getModuleLabel();
  protected abstract Bitmap getModuleImage();
  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    controller= AudiorouteActivityController.getInstance();
    controller.setModuleLabel(getModuleLabel());
    controller.setModuleImage(getModuleImage());
    controller.setListener(new AudiorouteActivityController.Listener() {
      @Override
      public void onRouteConnected() {
        onAudiorouteConnected();
      }

      @Override
      public void onRouteDisconnected() {
        onAudiorouteDisconnected();
      }

      @Override
      public AudioModule createAudioModule() {
        return createModule();
      }
    });
    controller.onActivityCreated(this, false);
  }
  @Override
  protected void onResume()
  {
    super.onResume();
    controller.onResume();
  }
  @Override protected void onNewIntent (Intent intent)
  {
    super.onNewIntent(intent);
    controller.onNewIntent(intent);
  }
  protected void onAudiorouteDisconnected()
  {

  }
  protected void onAudiorouteConnected()
  {
  }

  protected abstract AudioModule createModule();
  /**
   * When overriding the onDestroy method, make sure to invoke this parent method _at the end_.
   */
  @Override
  protected void onDestroy() {
    //controller.disconnect(this);
    super.onDestroy();
  }
}
