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

import android.app.Notification;
import android.os.RemoteException;
import android.util.Log;

import com.ntrack.audioroute.internal.SharedMemoryUtils;

public abstract class AudioModule {

  static {
    System.loadLibrary("audiomodule");
  }

  private static final String TAG = "audioroute";

  private String name = null;
  private int token = -1;
  protected long handle = 0;

  //private Notification notification;

  private class FdReceiverThread extends Thread {
    @Override
    public void run() {
      Log.d(TAG, "Starting receiving shared desc");
      token = SharedMemoryUtils.receiveSharedMemoryFileDescriptor();
      Log.i(TAG, "fd: " + token);
    }
  }

  /**
   * Constructor.
   * 
   * @param notification
   *            Notification to be passed to the Audioroute service, so that the service
   *            can associate an audio module with an app. May be null.
   */
  protected AudioModule() {

  }

  /*public void setNotification(Notification notification)
  {
    this.notification=notification;
  }*/
  /**
   * This method takes care of the elaborate choreography that it takes to set
   * up an audio module and to connect it to its representation in the
   * Audioroute service.
   * 
   * Specifically, it sets up the shared memory between the local module and
   * the Audioroute service, creates a new module in the service, and connects
   * it to the local module.
   * 
   * A module can only be configured once. If it times out, it cannot be
   * reinstated and should be released.
   * 
   * @param audioroute
   *            Stub for communicating with the Audioroute service.
   * @param name
   *            Name of the new audio module in Audioroute.
   * @return 0 on success, a negative error on failure; use
   *         {@link AudiorouteException} to interpret the return value.
   * @throws RemoteException
   */
  int moduleIndex=-1;
  public static int instanceIndex=-1;
  public int getModuleIndex()
  {
    return moduleIndex;
  }
  public int getCurrentInstanceId()
  {
    return instanceIndex;
  }
  public void setCurrentInstanceId(int instanceIndex)
  {
    this.instanceIndex=instanceIndex;
  }
  boolean configured=false;
  public boolean isConfigured()
  {
    return configured;
  }
  public interface IModuleDisconnectListener
  {
    void onConnectionShutDown();
  }
  IModuleDisconnectListener listener;
  public int configure(String name, int engineData, IModuleDisconnectListener listener)
      //throws RemoteException
      {
        this.listener=listener;
    int version= getProtocolVersion();
    /*if (version != getProtocolVersion()) {
      return AudiorouteException.PROTOCOL_VERSION_MISMATCH;
    }*/
    if (this.handle != 0) {
      throw new IllegalStateException("Module is already configured.");
    }
    /*FdReceiverThread t = new FdReceiverThread();
    t.start();
    final int MaxSleepCount=1000;
    int slept=0;
    boolean timeout=false;
    while (
    //audioroute.sendSharedMemoryFileDescriptor() != 0 &&
    t.isAlive()) {
      try {
        Thread.sleep(10); // Wait for receiver thread to spin up.
      } catch (InterruptedException e) {
        Thread.currentThread().interrupt();
      }
      if(slept++>MaxSleepCount) {
        timeout=true;
        break;
      }
    }
    try {
      if(!timeout) t.join();
      else {
        Log.i(TAG, "Timeout while establishing connection");
        token=-1;
      }
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
    }*/
    token=engineData;
    if (token < 0) {
      return token;
    }
    int index = 0;//audioroute.createModule(name, getInputChannels(), getOutputChannels(), notification);
    moduleIndex=index;
    if (index < 0) {
      SharedMemoryUtils.closeSharedMemoryFileDescriptor(token);
      return index;
    }
    handle = createRunner(version, token, index);
    if (handle == 0) {
      //audioroute.deleteModule(name);
      SharedMemoryUtils.closeSharedMemoryFileDescriptor(token);
      return AudiorouteException.FAILURE;
    }
    if (!configure_module(name, handle)) {
      releaseInternal(handle);
      //audioroute.deleteModule(name);
      SharedMemoryUtils.closeSharedMemoryFileDescriptor(token);
      return AudiorouteException.FAILURE;
    }
    configured=true;
    this.name = name;
    if(instanceIndex==-1) createNewInstanceIndex();
    return AudiorouteException.SUCCESS;
  }
  public void onConnectionShutdown()
  {
    configured=false;
    if(listener!=null)
      listener.onConnectionShutDown();
    //lastInstanceIndex=0;
  }
  public void onActivated(int sampleRate, int framesPerBuffer, int[] connectedInputBuses, int[] connectedOutputBuses)
  {

  }
  public void onDeactivated()
  {

  }
  static int lastInstanceIndex=0;
  public void createNewInstanceIndex()
  {
    instanceIndex=++lastInstanceIndex;
  }


  /**
   * @return The name of the module if configured, null if it isn't.
   */
  public String getName() {
    return name;
  }

  /**
   * @return The notification associated with this module.
   */
  /*protected Notification getNotification() {
    return notification;
  }*/

  /**
   * @return True if the module has timed out. A module that has timed out
   *         cannot be reinstated and must be released.
   */
  public final boolean hasTimedOut() {
    return handle != 0 && hasTimedOut(handle);
  }

  /**
   * @return The number of input channels of this module.
   */
  public int getInputChannels() { return 2;}

  /**
   * @return The number of output channels of this module.
   */
  public int getOutputChannels() { return 2;}

  /**
   * This method is called by the public configure method. It is responsible
   * to setting up the native components of an audio module implementation,
   * such as the audio processing function and any data structures that make
   * up the processing context.
   * 
   * @param name
   * @param handle
   *            Opaque handle to the internal data structure representing an
   *            audio module.
   * @param sampleRate
   * @param bufferSize
   * @return True on success
   */
  protected abstract boolean configure_module(String name, long handle);

  public void stopAndRelease()
  {
    if(handle!=0)
      releaseInternal(handle);
    handle=0;
    release();
  }

  public void audiorouteConfigure(long process_func, long init_func, long data)
  {
    //Call native
    audiorouteModuleConfigure(handle, process_func, init_func, data);
  }

  /**
   * Releases any resources held by the audio module, such as memory allocated
   * for the processing context.
   */
  protected abstract void release();

  /**
   * @return The Audioroute protocol version; mostly for internal use.
   */
  public static native int getProtocolVersion();

  private native long createRunner(int version, int token, int index);

  private native void releaseInternal(long handle);

  private native boolean hasTimedOut(long handle);

  public int getCurrentSampleRate()
  {
    if(moduleIndex<0) return 0;
    return getSampleRate(handle, moduleIndex);
  }
  public int getCurrentFramesPerBuffer()
  {
    if(moduleIndex<0) return 0;
    return getFramesPerBuffer(handle, moduleIndex);
  }
  private native int getFramesPerBuffer(long handle, int moduleIndex);
  private native int getSampleRate(long handle, int moduleIndex);
  private native int audiorouteModuleConfigure(long handle, long process_func, long init_func, long data);

}
