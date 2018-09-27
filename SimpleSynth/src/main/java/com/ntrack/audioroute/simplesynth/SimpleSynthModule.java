/*
 * Copyright 2018 n-Track Software All Rights Reserved.
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

import android.app.Notification;
import android.util.Log;

import com.ntrack.audioroute.AudioModule;

/**
 * A sample audio module that implements a simple discretization of an RC lowpass filter. The native
 * components are in Audioroute/jni/samples/lowpass.c. They illustrate a number of crucial points.
 * In particular, they show how to use the rendering context of the processing callback, and how to
 * update parameters in a lock-free yet thread-safe manner.
 */
public class SimpleSynthModule extends AudioModule {
  private long ptr = 0;
  private final int channels;

  SimpleSynthActivity theActivity;
  public SimpleSynthModule(int channels, SimpleSynthActivity theActivity) {
    this.theActivity=theActivity;
    if (channels < 1) {
      throw new IllegalArgumentException("Channel count must be at least one.");
    }
    this.channels = channels;
  }

  @Override
  protected boolean configure_module(String name, long handle) {
    if (ptr != 0) {
      throw new IllegalStateException("Module has already been configured.");
    }
    ptr = configureNativeComponents(handle, channels);
    return ptr != 0;
  }
  @Override public void createNewInstanceIndex()
  {
    super.createNewInstanceIndex();
    if (ptr == 0) {
      Log.d("AudiorouteSimpleSynth", "Module has not been configured.");
      return;
    }
    configureNativeInstance(handle, ptr, instanceIndex);
  }

  @Override
  protected void release() {
    theActivity.checkStartAudio();
    if (ptr != 0) {
      release(ptr);
      ptr = 0;
    }
  }

  @Override
  public int getInputChannels() {
    return channels;
  }

  @Override
  public int getOutputChannels() {
    return channels;
  }

  /**
   * Sets the cutoff frequency of the lowpass filter.
   * 
   * @param q cutoff frequency as a fraction of the sample rate.
   */
  public void setSimulateHang()
  {
    if (ptr == 0) return;
    setParameter(ptr, getCurrentInstanceId(), 1, 1);
    try {
      Thread.sleep(2000);
    }
    catch(InterruptedException e)
    {

    }
    setParameter(ptr, getCurrentInstanceId(), 1, 0);
  }
  public void setWaveform(int mode) {
    if (ptr == 0) return;
    setParameter(ptr, getCurrentInstanceId(), 0, mode);
  }
  public int getWaveform()
  {
      if (ptr == 0) {
        return 0;
        //throw new IllegalStateException("Module is not configured.");
      }
      return (int)getParameter(ptr, 0, getCurrentInstanceId());
  }

  private native long configureNativeComponents(long handle, int channels);

  private native void configureNativeInstance(long handle, long ptr, int instanceIndex);

  private native void release(long ptr);

  private native void setParameter(long ptr, long instance_index, int parameter, double alpha);
    private native double getParameter(long ptr, int parameter, long instance_index);
}
