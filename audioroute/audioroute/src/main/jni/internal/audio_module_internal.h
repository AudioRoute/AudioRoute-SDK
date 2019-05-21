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

/*
 * Internal data structures representing audio modules and their connections,
 * as well as some functions that operate on them.
 */

#ifndef __AUDIO_MODULE_INTERNAL_H__
#define __AUDIO_MODULE_INTERNAL_H__

#include "audio_module.h"

#include "simple_barrier.h"

#include <pthread.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AUDIOROUTE_PROTOCOL_VERSION 11

#define MAX_MODULES 1
#define MAX_CONNECTIONS 16

#define ChannelsPerBus 2

#define MEM_PAGE_SIZE sysconf(_SC_PAGESIZE)
#define BARRIER_OFFSET (MAX_MODULES * sizeof(audio_module) / MEM_PAGE_SIZE + 1)
#define BUFFER_OFFSET \
  (BARRIER_OFFSET + MAX_MODULES * 3 * sizeof(int) / MEM_PAGE_SIZE + 1)

typedef struct {
    int status;  // 0: none; 1: current; 2: slated for deletion
    int in_use;

    int source_index;
    int source_port;
    int sink_port;
} connection;

#define MaxEventsPerBuffer 100


typedef struct {
    int status;  // 0: none; 1: current; 2: slated for deletion
    int active;
    int in_use;

    int sample_rate;
    int buffer_frames;

    int input_channels;
    union {
        ptrdiff_t input_buffer;   // Storing buffers as offsets of type ptrdiff_t
        int64_t dummy;
    };
    int output_channels;      // rather than pointers of type float* to render
    union {
        ptrdiff_t output_buffer;  // them independent of the shared memory location.
        int64_t dummy2;
    };

    int numInputBuses;
    int numOutputBuses;

    union {
        struct timespec deadline;
        struct {
            int64_t dummy1;
            int64_t dummy2;
        } dummy_deadline;
    };
    union {
        ptrdiff_t report;
        int64_t dummy3;
    };
    union {
        ptrdiff_t wake;
        int64_t dummy4;
    };
    union {
        ptrdiff_t ready;
        int64_t dummy5;
    };

    // Runtime state
    int is_hosted;
    int callbackType; // 0 for processing, 1 for initialize
    int numEvents;
    MusicEvent musicEvents[MaxEventsPerBuffer];
    int instance_index;
    int isAlive;
    int host_gave_up;

    struct AudiorouteTimeInfo timeInfo;
} audio_module;

typedef struct {
    int shm_fd;
    void *shm_ptr;
    int index;
    pthread_t thread;
    simple_barrier_t launched;
    int launch_counter;
    int done;
    int timed_out;
    audio_module_process_t process;
    initialize_processing_t initialize_processing;
    void *context;
    volatile int alive;
    volatile int exited;
} audio_module_runner;

struct audioroute_engine
{
    void *shm_ptr;
};

audio_module *audioroute_get_audio_module(void *p, int index);

float *audioroute_get_audio_buffer(void *p, ptrdiff_t offset);

simple_barrier_t *audioroute_get_barrier(void *p, ptrdiff_t offset);

struct simple_lock_barrier_t *audioroute_get_lock(void *p, ptrdiff_t offset);

void audioroute_collect_input(void *p, int index);

audio_module_runner *audioroute_create(int version, int token, int index);

void audioroute_release(audio_module_runner *p);

int audioroute_has_timed_out(audio_module_runner *p);

// Return 0 if successfull
int audioroute_process(void *engine_ptr, int index, float *bufferL, int strideL, float *bufferR, int strideR, int framesPerBuffer, int nChannels, int sampleRate, MusicEvent *events, int eventsNum,  int instanceId, struct AudiorouteTimeInfo *timeInfo);

int audioroute_process_interleaved(void *engine_ptr, int index, float *buffer_interleaved, int framesPerBuffer, int nChannels, int sampleRate, MusicEvent *events, int eventsNum,  int instanceId, struct AudiorouteTimeInfo *timeInfo);

int audioroute_process_non_interleaved(void *engine_ptr, int index, float *bufferL, float *bufferR, int framesPerBuffer, int nChannels, int sampleRate, MusicEvent *events, int eventsNum,  int instanceId, struct AudiorouteTimeInfo *timeInfo);

// Return 0 if successfull. Calls the initialize_processing callback to set sampling frequency and buffer frame size
// Make sure you call initialize before calling audioroute_process(). If you need to re-initialize (after changing sample rate, frame sizes etc.)
// call audioroute_initialize() again. Thread synchronization between audioroute_initialize() and audioroute_process must be handled by the host app
// Ideally the initialize() call should be performed before your audio engine starts or before adding the module to your engine
int audioroute_initialize(void *engine_ptr, int index, int framesPerBuffer, int nChannels, int sampleRate, int instance_index, int connectedInputBuses[MaxNumBuses], int connectedOutputBuses[MaxNumBuses]);

void audioroute_release_instance(void *engine);

#ifdef __cplusplus
}
#endif
#endif
