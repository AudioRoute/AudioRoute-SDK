/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 * Copyright 2019 n-Track S.r.l. All Rights Reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "audioroute.h"

#include "audio_module.h"
#include "audio_module_internal.h"
#include "opensl_stream/opensl_stream.h"
#include "shared_memory_internal.h"
#include "simple_barrier.h"

#include <android/log.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  OPENSL_STREAM *os;
  int sample_rate;
  int buffer_frames;
  int shm_fd;
  void *shm_ptr;
  ptrdiff_t next_buffer;
} audioroute;

static void perform_cleanup(audioroute *pb) {
  int i, j, k;
  for (i = 0; i < MAX_MODULES; ++i) {
    audio_module *module = audioroute_get_audio_module(pb->shm_ptr, i);
    if (__sync_or_and_fetch(&module->status, 0) == 2) {
      int buffer_frames = (module->input_channels + module->output_channels) *
        pb->buffer_frames;
      pb->next_buffer -= buffer_frames;
      for (j = 0; j < MAX_MODULES; ++j) {
        audio_module *other = audioroute_get_audio_module(pb->shm_ptr, j);
        if (other->input_buffer > module->input_buffer) {
          other->input_buffer -= buffer_frames;
          other->output_buffer -= buffer_frames;
        }
      }
        __sync_bool_compare_and_swap(&module->status, 2, 0);
    } else {
    }
  }
}

static int is_running(audioroute *pb) {
  return 1; //opensl_is_running(pb->os);
}

static int add_module(audioroute *pb, int input_channels, int output_channels) {
  if (!is_running(pb)) {
    perform_cleanup(pb);
  }
  if ((pb->next_buffer + (input_channels + output_channels) *
        pb->buffer_frames) * sizeof(float) > smi_get_size()) {
    return -9;  // AudiorouteException.OUT_OF_BUFFER_SPACE
  }
  int i;
  for (i = 0; i < MAX_MODULES; ++i) {
    audio_module *module = audioroute_get_audio_module(pb->shm_ptr, i);
    if (__sync_or_and_fetch(&module->status, 0) == 0) {
      module->active = 0;
      module->in_use = 0;
      module->sample_rate = pb->sample_rate;
      module->buffer_frames = pb->buffer_frames;
      module->input_channels = input_channels;
      module->input_buffer = pb->next_buffer;
      pb->next_buffer += input_channels * pb->buffer_frames;
      module->output_channels = output_channels;
      module->output_buffer = pb->next_buffer;
      pb->next_buffer += output_channels * pb->buffer_frames;
      module->report =
        BARRIER_OFFSET * MEM_PAGE_SIZE / sizeof(struct simple_lock_barrier_t) + i * 3; // simple_lock_barrier_t is bigger than simple_barrier_t, we're wasting some space
      sb_clobber(audioroute_get_barrier(pb->shm_ptr, module->report));
      module->wake = module->report + 1;
      sb_clobber(audioroute_get_barrier(pb->shm_ptr, module->wake));
      module->ready = module->report + 2;

        module->numInputBuses=1;
        module->numOutputBuses=1;
      __sync_bool_compare_and_swap(&module->status, 0, 1);
      return i;
    }
  }
  return -5;  // AudiorouteException.TOO_MANY_MODULES
}

static int delete_module(audioroute *pb, int index) {
  audio_module *module = audioroute_get_audio_module(pb->shm_ptr, index);
  __sync_bool_compare_and_swap(&module->status, 1, 2);
  return 0;
}

static int activate_module(audioroute *pb, int index) {
  audio_module *module = audioroute_get_audio_module(pb->shm_ptr, index);
  __sync_bool_compare_and_swap(&module->active, 0, 1);
  return 0;
}
static int deactivate_module(audioroute *pb, int index) {
  audio_module *module = audioroute_get_audio_module(pb->shm_ptr, index);
  __sync_bool_compare_and_swap(&module->active, 1, 0);
  return 0;
}

static int is_active(audioroute *pb, int index) {
  audio_module *module = audioroute_get_audio_module(pb->shm_ptr, index);
  return __sync_or_and_fetch(&module->active, 0);
}

static int get_input_channels(audioroute *pb, int index) {
  audio_module *module = audioroute_get_audio_module(pb->shm_ptr, index);
  return module->input_channels;
}

static int is_connected(audioroute *pb, int source_index, int source_port, int sink_index, int sink_port);

static int *get_connected_buses(audioroute *pb, int index, int isInput, int *connectedBuses) {
    audio_module *module = audioroute_get_audio_module(pb->shm_ptr, index);

    for(int i=0; i<MaxNumBuses; ++i)
        connectedBuses[i]=0;

    if(!isInput) { // Check which input is connected to the module output
        for(int mod=0; mod<MAX_MODULES; ++mod) {
            if(mod==index) continue;
            for (int bus = 0; bus < module->numOutputBuses; ++bus) {
                int channelForBusL = bus * ChannelsPerBus;
                int channelForBusR = channelForBusL + 1;

                audio_module *sink = audioroute_get_audio_module(pb->shm_ptr, mod);

                for(int destBus=0; destBus<sink->numInputBuses; destBus++) {
                    int channelForBusInputL = destBus * ChannelsPerBus;
                    if (is_connected(pb, index, channelForBusL, mod, channelForBusInputL))
                        connectedBuses[bus]=1;
                }
            }
        }
    } else {
        for(int mod=0; mod<MAX_MODULES; ++mod) {
            if(mod==index) continue;
            for (int bus = 0; bus < module->numInputBuses; ++bus) {
                int channelForBusL = bus * ChannelsPerBus;
                int channelForBusR = channelForBusL + 1;

                audio_module *source = audioroute_get_audio_module(pb->shm_ptr, mod);

                for(int destBus=0; destBus<source->numOutputBuses; destBus++) {
                    int channelForBusInputL = destBus * ChannelsPerBus;
                    if (is_connected(pb, mod, channelForBusInputL, index, channelForBusL))
                        connectedBuses[bus]=1;
                }
            }
        }
    }
    return 0;
}


static int get_output_channels(audioroute *pb, int index) {
  audio_module *module = audioroute_get_audio_module(pb->shm_ptr, index);
  return module->output_channels;
}

static int is_connected(audioroute *pb, int source_index, int source_port,
    int sink_index, int sink_port) {
  audio_module *sink = audioroute_get_audio_module(pb->shm_ptr, sink_index);
  int i;
  return 0;
}

static int connect_modules(audioroute *pb, int source_index, int source_port,
   int sink_index, int sink_port) {
  if (!is_running(pb)) {
    perform_cleanup(pb);
  }
  return -7;  // AudiorouteException.TOO_MANY_CONNECTIONS
}

static int disconnect_modules(audioroute *pb, int source_index, int source_port,
   int sink_index, int sink_port) {
  audio_module *sink = audioroute_get_audio_module(pb->shm_ptr, sink_index);
  int i;
  return 0;
}

int audioroute_kill_module_runner(void *engine, int index, int instance_index);

static void release(audioroute *pb, int instance_index) {

  audioroute_kill_module_runner(pb, 0, instance_index);
  int i;
  smi_unlock(pb->shm_ptr);
  smi_unmap(pb->shm_ptr);
  close(pb->shm_fd);
  free(pb);
}

#define ONE_BILLION 1000000000

void add_nsecs(struct timespec *t, int dt) {
  t->tv_nsec += dt;
  if (t->tv_nsec >= ONE_BILLION) {
    ++t->tv_sec;
    t->tv_nsec -= ONE_BILLION;
  }
}

static const float float_to_short = SHRT_MAX;
static const float short_to_float = 1 / (1 + (float) SHRT_MAX);

static void stop_modules(audioroute *pb)
{
    audio_module *sink = audioroute_get_audio_module(pb->shm_ptr, 0);

}

static audioroute *create_instance(int sample_rate, int buffer_frames,
    int input_channels, int output_channels) {
  audioroute *pb = (audioroute *)malloc(sizeof(audioroute));
  if (pb) {
    pb->sample_rate = sample_rate;
    pb->buffer_frames = buffer_frames;
    pb->next_buffer = BUFFER_OFFSET * MEM_PAGE_SIZE / sizeof(float);

    pb->shm_fd = smi_create();
    if (pb->shm_fd < 0) {
      LOGW("Unable to create shared memory.");
      free(pb);
      return NULL;
    }
    pb->shm_ptr = smi_map(pb->shm_fd);
    if (!pb->shm_ptr) {
      LOGW("Unable to map shared memory.");
      close(pb->shm_fd);
      free(pb);
      return NULL;
    }
    smi_lock(pb->shm_ptr);

    /*// Create OpenSL stream.
    pb->os = opensl_open(sample_rate,
        input_channels, output_channels, buffer_frames, process, pb);
    if (!pb->os) {
      smi_unlock(pb->shm_ptr);
      smi_unmap(pb->shm_ptr);
      close(pb->shm_fd);
      free(pb);
      return NULL;
    }*/

    int i;
    for (i = 0; i < MAX_MODULES; ++i) {
      audio_module *module = audioroute_get_audio_module(pb->shm_ptr, i);
      memset(module, 0, sizeof(audio_module));
    }
    activate_module(pb, add_module(pb, 0, input_channels));
    //activate_module(pb, add_module(pb, output_channels, 0));
  }
  return pb;
}

JNIEXPORT jlong JNICALL
Java_com_ntrack_audioroute_Audioroute_createInstance
(JNIEnv *env, jobject obj, jint sample_rate, jint buffer_frames,
 int input_channels, int output_channels) {
  return (jlong) create_instance(sample_rate, buffer_frames,
      input_channels, output_channels);
}

JNIEXPORT void JNICALL
Java_com_ntrack_audioroute_Audioroute_releaseInstance
(JNIEnv *env, jobject obj, jlong p, jint instance_index) {
  audioroute *pb = (audioroute *) p;
   stop_modules(pb);
  release(pb, instance_index);
}

JNIEXPORT jboolean JNICALL Java_com_ntrack_audioroute_Audioroute_isModuleConnected(JNIEnv *env, jobject obj, jlong streamPtr)
{
    audioroute *pb = (audioroute *) streamPtr;
    if(NULL==pb) return 0;
    audio_module *module = audioroute_get_audio_module(pb->shm_ptr, 0);
    return module->isAlive!=0;
}

int audioroute_keepalive(void *pb);

JNIEXPORT jboolean JNICALL Java_com_ntrack_audioroute_Audioroute_isAlive (JNIEnv *env, jobject obj, jlong streamPtr)
{
  audioroute *pb = (audioroute *) streamPtr;
  if(NULL==pb) return 0;
  audio_module *module=audioroute_get_audio_module(pb->shm_ptr, 0);
  if(!module->isAlive) { // Timed out, check if is still responsive
      int ret = audioroute_keepalive(pb);
      return 0 == ret;
  }
  return 1;
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_doSendSharedMemoryFileDescriptor
(JNIEnv *env, jobject obj, jlong p) {
  audioroute *pb = (audioroute *) p;
#ifdef NDEBUG
    int maxSleep=250;
#else
  int maxSleep=1000;
#endif
    int count=0;
  audio_module *module=audioroute_get_audio_module(pb->shm_ptr, 0);
  module->is_hosted=-1;

    usleep(40000); // Give time to slave to spin up

    module->isAlive=0;
  while(1) {
      if (smi_send(pb->shm_fd) == 0) {
          break;
      }
      usleep(100000);
      if(++count>maxSleep) {
          LOGW("Shmem descriptor send timeout");
          return -1;
      } else {
          LOGW("Shmem trying send again");
      }
    }
    while(module->is_hosted != 999) {
        usleep(20000);
        if(++count>maxSleep) {
            LOGW("Shmem descriptor send timeout (flag)");
            return -1;
        } else {
            LOGW("Shmem waiting for flag");
        }
    }
    module->isAlive=1;
    LOGW("Shmem file descriptor send success");
  return 0;
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_start
(JNIEnv *env, jobject obj, jlong p) {
  audioroute *pb = (audioroute *) p;
  return opensl_start(pb->os);
}

JNIEXPORT void JNICALL
Java_com_ntrack_audioroute_Audioroute_stop
(JNIEnv *env, jobject obj, jlong p) {
  audioroute *pb = (audioroute *) p;
  opensl_pause(pb->os);
}

JNIEXPORT jboolean JNICALL
Java_com_ntrack_audioroute_Audioroute_isRunning
(JNIEnv *env, jobject obj, jlong p) {
  audioroute *pb = (audioroute *) p;
  return is_running(pb);
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_createModule
(JNIEnv *env, jobject obj, jlong p, jint input_channels, jint output_channels) {
  audioroute *pb = (audioroute *) p;
  return add_module(pb, input_channels, output_channels);
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_connectPorts
(JNIEnv *env, jobject obj, jlong p,
 jint source_index, jint source_port, jint sink_index, jint sink_port) {
  audioroute *pb = (audioroute *) p;
  return connect_modules(pb, source_index, source_port, sink_index, sink_port);
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_disconnectPorts
(JNIEnv *env, jobject obj, jlong p,
 jint source_index, jint source_port, jint sink_index, jint sink_port) {
  audioroute *pb = (audioroute *) p;
  return disconnect_modules(pb, source_index, source_port, sink_index, sink_port);
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_deleteModule
(JNIEnv *env, jobject obj, jlong p, jint index) {
  audioroute *pb = (audioroute *) p;
  return delete_module(pb, index);
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_activateModule
(JNIEnv *env, jobject obj, jlong p, jint index) {
  audioroute *pb = (audioroute *) p;
  return activate_module(pb, index);
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_deactivateModule
(JNIEnv *env, jobject obj, jlong p, jint index) {
  audioroute *pb = (audioroute *) p;
  return deactivate_module(pb, index);
}

JNIEXPORT jboolean JNICALL
Java_com_ntrack_audioroute_Audioroute_isActive
(JNIEnv *env, jobject obj, jlong p, jint index) {
  audioroute *pb = (audioroute *) p;
  return is_active(pb, index);
}

JNIEXPORT jboolean JNICALL
Java_com_ntrack_audioroute_Audioroute_isConnected
(JNIEnv *env, jobject obj, jlong p, jint sourceIndex, jint sourcePort,
 jint sinkIndex, jint sinkPort) {
  audioroute *pb = (audioroute *) p;
  return is_connected(pb, sourceIndex, sourcePort, sinkIndex, sinkPort);
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_getInputChannels
(JNIEnv *env, jobject obj, jlong p, jint index) {
  audioroute *pb = (audioroute *) p;
  return get_input_channels(pb, index);
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_getOutputChannels
(JNIEnv *env, jobject obj, jlong p, jint index) {
  audioroute *pb = (audioroute *) p;
  return get_output_channels(pb, index);
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_getProtocolVersion
(JNIEnv *env, jobject obj, jlong p) {
  return AUDIOROUTE_PROTOCOL_VERSION;
}

JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_Audioroute_getEngineData
        (JNIEnv *env, jobject obj, jlong p) {
    audioroute *pb = (audioroute *) p;
    return pb->shm_fd;
}

JNIEXPORT jintArray JNICALL
Java_com_ntrack_audioroute_Audioroute_getConnectedBuses
        (JNIEnv *env, jobject obj, jlong p, jint index, jboolean isInput) {
  audioroute *pb = (audioroute *) p;
  int arrayconnected[MaxNumBuses];
    get_connected_buses(pb, index, isInput, arrayconnected);

  jintArray result;
  result = (*env)->NewIntArray(env, MaxNumBuses);
  if (result == NULL) {
    return NULL; /* out of memory error thrown */
  }
  int i;
  // fill a temp structure to use to populate the java int array
  jint fill[MaxNumBuses];
  for (i = 0; i < MaxNumBuses; i++) {
    fill[i] = arrayconnected[i]; // put whatever logic you want to populate the values here.
  }
  // move from the temp structure to the java structure
    (*env)->SetIntArrayRegion(env, result, 0, MaxNumBuses, fill);
  return result;
}

void *getShmFromEngine(void *engine)
{
    audioroute *pb = (audioroute *) engine;
    return pb->shm_ptr;
}

void setEngineParameters(void *engine, int sampleRate, int nChannels, int framesPerBuffer)
{
  audioroute *pb = (audioroute *) engine;
  pb->sample_rate=sampleRate;
  pb->buffer_frames=framesPerBuffer;
}

void DoSetJVM(JavaVM *jvm);

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved) {
  DoSetJVM(jvm);
    return JNI_VERSION_1_6;
}