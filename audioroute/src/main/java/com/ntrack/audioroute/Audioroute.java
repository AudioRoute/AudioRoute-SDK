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

import android.content.Context;
import android.util.Log;

import com.ntrack.audioroute.internal.OpenSlParams;

class Audioroute {

  public static final String ProductName = "AudioRoute";
  private static final String TAG = ProductName;

  public static native boolean isAlive(long context);

  public static native int getEngineData(long streamPtr);

  public static long getAudiorouteInstance(Context context, int inputChannels, int outputChannels)
  {
    OpenSlParams params = OpenSlParams.createInstance(context);
    long streamPtr = createInstance(params.getSampleRate(), params.getBufferSize(), inputChannels, outputChannels);
    Log.i(TAG, "Created stream with ptr " + streamPtr);
    return streamPtr;
  }

  public static synchronized void release(long streamPtr, int instance_index) {
    if (streamPtr != 0) {
      releaseInstance(streamPtr, instance_index);
      streamPtr = 0;
    }
  }

  public static synchronized int sendSharedMemoryFileDescriptor(long streamPtr) {
    if (streamPtr == 0) {
      throw new IllegalStateException("Stream closed.");
    }
    return AudiorouteException.successOrFailure(doSendSharedMemoryFileDescriptor(streamPtr));
  }

  private static native long createInstance(int sampleRate, int bufferSize, int inputChannels, int outputChannels);

  private static native void releaseInstance(long streamPtr, int instanceId);

  private static native int getProtocolVersion(long streamPtr);
  private static native int doSendSharedMemoryFileDescriptor(long streamPtr);
  protected static native boolean isModuleConnected(long streamPtr);

  protected static boolean isModuleConnectedPtr(long streamPtr)
  {
    return isModuleConnected(streamPtr);
  }
}

