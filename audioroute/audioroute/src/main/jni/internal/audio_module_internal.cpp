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

#include "audio_module_internal.h"
#include "opensl_stream/opensl_stream.h"
#include "shared_memory_internal.h"
#include "simple_barrier.h"
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

void onModuleReleased();

audio_module *audioroute_get_audio_module(void *p, int index) {
  return ((audio_module *) p) + index;
}

float *audioroute_get_audio_buffer(void *p, ptrdiff_t offset) {
  return ((float *) p) + offset;
}

simple_lock_barrier_t *audioroute_get_lock(void *p, ptrdiff_t offset) {
    return (simple_lock_barrier_t*)(((simple_barrier_t *) p) + offset);
}

simple_barrier_t *audioroute_get_barrier(void *p, ptrdiff_t offset) {
  return ((simple_barrier_t *) p) + offset;
}

#include <cassert>
extern "C" void add_nsecs(struct timespec *t, int dt);
#define ONE_BILLION 1000000000
#define REPORT_TIMEOUT 100000
#define INIT_PROC_TIMEOUT_SEC 10
#define RELEASE_PROCESS_TIMEOUT_SECONDS 5
#define DEBUG_PROCESS_TIMEOUT_SECONDS 5 // 500
#define DEBUG_STARTUP_TIMEOUT_SECONDS 100 // 300
#define RELEASE_STARTUP_TIMEOUT_SECONDS 5
extern "C" void *getShmFromEngine(void *engine);

extern "C" void setEngineParameters(void *engine, int sampleRate, int nChannels, int framesPerBuffer);

#define CALLBACK_TYPE_SHUTDOWN 100
#define CALLBACK_TYPE_PROCESS 0
#define CALLBACK_TYPE_INITIALIZE 1
#define CALLBACK_TYPE_KEEPALIVE 2

#define AUDIOROUTE_CALLBACK_RESULT_SUCCESS 1000
#define AUDIOROUTE_CALLBACK_RESULT_UNDEFINED -10

extern "C" int audioroute_kill_module_runner(void *engine, int index, int instance_index)
{
    void *shm_ptr=getShmFromEngine(engine);
    audio_module *module = audioroute_get_audio_module(shm_ptr, index);
    if(NULL==module) return -2;
    module->callbackType=CALLBACK_TYPE_SHUTDOWN; // Kill type
    module->instance_index=instance_index;

    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    add_nsecs(&deadline, REPORT_TIMEOUT);  // 0.1ms deadline for clients to report.
    bool in_use = sb_wait_and_clear(audioroute_get_barrier(shm_ptr, module->report), &deadline) == 0;

    //sb_clobber(audioroute_get_barrier(shm_ptr, module->ready));

    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec+=INIT_PROC_TIMEOUT_SEC;

    sb_wake(audioroute_get_barrier(shm_ptr, module->wake));

    int ret = -1;
    if(in_use) // wait only if module responded to report
        ret=sb_wait_lock(audioroute_get_lock(shm_ptr, module->ready), &deadline);
    return ret;
}

extern "C" int audioroute_initialize(void *engine, int index, int framesPerBuffer, int nChannels, int sampleRate, int instance_index, int connectedInputBuses[MaxNumBuses], int connectedOutputBuses[MaxNumBuses])
{
    void *shm_ptr=getShmFromEngine(engine);
    setEngineParameters(engine, sampleRate, nChannels, framesPerBuffer);
    int indexActual=0;
    audio_module *module = audioroute_get_audio_module(shm_ptr, indexActual);
    if(NULL==module) return -2;
    module->buffer_frames=framesPerBuffer;
    module->sample_rate=sampleRate;
    module->input_channels=module->output_channels=nChannels;
    module->is_hosted=1;
    module->callbackType=CALLBACK_TYPE_INITIALIZE;
    module->instance_index=instance_index;

    module->numInputBuses=0;
    for(int i=0; i<MaxNumBuses; ++i)
        if(connectedInputBuses[i])
            module->numInputBuses++;

    module->numOutputBuses=0;
    for(int i=0; i<MaxNumBuses; ++i)
        if(connectedOutputBuses[i])
            module->numOutputBuses++;

    if(index<0) {
        module->isAlive=0;
        LOGD("Audioroute initialize module index -1 aborting");
        return -1;
    }

   struct timespec deadline;
  /*   clock_gettime(CLOCK_MONOTONIC, &deadline);
    add_nsecs(&deadline, REPORT_TIMEOUT);  // 0.1ms deadline for clients to report.
    bool in_use =
            sb_wait_and_clear(audioroute_get_barrier(shm_ptr, module->report), &deadline) == 0;

    if (in_use) */{
        //sb_clobber(audioroute_get_barrier(shm_ptr, module->ready));

        clock_gettime(AUDIOROUTE_LOCK_TIMEBASE, &deadline);
        deadline.tv_sec+=INIT_PROC_TIMEOUT_SEC;

        sb_wake(audioroute_get_barrier(shm_ptr, module->wake));
        int ret = sb_wait_and_reset_lock(audioroute_get_lock(shm_ptr, module->ready), &deadline);
        module->is_hosted = 0;
        if(0!=ret) {
            LOGD("Audioroute initialize timed out");
        }
        return ret;
    }
    return -1;
}

extern "C" int audioroute_keepalive(void *engine)
{
    void *shm_ptr=getShmFromEngine(engine);
    audio_module *module = audioroute_get_audio_module(shm_ptr, 0);
    if(NULL==module) return -2;
    module->callbackType=CALLBACK_TYPE_KEEPALIVE;

    module->isAlive=1;
    //sb_clobber(audioroute_get_barrier(shm_ptr, module->ready));

    struct timespec deadline;
    clock_gettime(AUDIOROUTE_LOCK_TIMEBASE, &deadline);
    deadline.tv_sec+=1; // 1 second timeout

    sb_wake(audioroute_get_barrier(shm_ptr, module->wake));
    int ret=sb_wait_and_reset_lock(audioroute_get_lock(shm_ptr, module->ready), &deadline);
    if(module->callbackType!=AUDIOROUTE_CALLBACK_RESULT_SUCCESS)
        ret=-1;
    if(0!=ret) module->isAlive=0;
    return ret;
}

extern "C" int audioroute_process(void *engine, int index, float *bufferL, int strideL, float *bufferR, int strideR, int framesPerBuffer, int nChannels, int sampleRate, MusicEvent *events, int eventsNum,  int instance_index, struct AudiorouteTimeInfo *timeInfo)
{
    void *shm_ptr=getShmFromEngine(engine);
    audio_module *module = audioroute_get_audio_module(shm_ptr, index);
    if(NULL==module) return -2;
    module->is_hosted=1;
    module->callbackType=CALLBACK_TYPE_PROCESS;
    float *input_buffer = audioroute_get_audio_buffer(shm_ptr, module->input_buffer);
    float *output_buffer = audioroute_get_audio_buffer(shm_ptr, module->output_buffer);
    assert(framesPerBuffer<=module->buffer_frames);

    if(nChannels==1) {
        for (int i = 0; i < framesPerBuffer; ++i) {
            input_buffer[i] = bufferL[i];
        }
    } else {
        int indl = 0, indr = 0;
        int inddestR = framesPerBuffer;
        for (int i = 0; i < framesPerBuffer; ++i, indl += strideL, indr += strideR) {
            input_buffer[i] = bufferL[indl];
            input_buffer[inddestR++] = bufferR[indr];
        }
    }

  module->buffer_frames=framesPerBuffer;
  module->sample_rate=sampleRate;
    module->instance_index=instance_index;

    for(int i=0; i<eventsNum; ++i)
    {
        module->musicEvents[i]=events[i];
    }
    module->numEvents=eventsNum;
    module->timeInfo=*timeInfo;

  struct timespec deadline;
    /*clock_gettime(CLOCK_MONOTONIC, &deadline);
    add_nsecs(&deadline, REPORT_TIMEOUT);  // 0.1ms deadline for clients to report.
    bool in_use =
            sb_wait_and_clear(audioroute_get_barrier(shm_ptr, module->report), &deadline) == 0;

    if (in_use) */
    {
      module->isAlive=1;
      //sb_clobber(audioroute_get_barrier(shm_ptr, module->ready));

    clock_gettime(AUDIOROUTE_LOCK_TIMEBASE, &deadline);
#ifdef NDEBUG
//        int dt = (ONE_BILLION / sampleRate + 1) * framesPerBuffer;
//    add_nsecs(&deadline, 2 * dt);  // Two-buffer-period processing deadline.
        deadline.tv_sec+=RELEASE_PROCESS_TIMEOUT_SECONDS;
#else
      deadline.tv_sec+=DEBUG_PROCESS_TIMEOUT_SECONDS;
#endif

    sb_wake(audioroute_get_barrier(shm_ptr, module->wake));
    int ret=sb_wait_and_reset_lock(audioroute_get_lock(shm_ptr, module->ready), &deadline);
    module->is_hosted=0;
    if(0!=ret)
    {
      // Wait timed out
      //assert(false);
        LOGD("Audioroute process timeout");
        module->isAlive=0;
        module->host_gave_up=1;
        sb_wake(audioroute_get_barrier(shm_ptr, module->wake));
      return ret;
    }
      if(nChannels==1) {
          for (int i = 0; i < framesPerBuffer; ++i) {
              bufferL[i]=output_buffer[i];
          }
      } else {
          int indl = 0, indr = 0;
          int inddestR = framesPerBuffer;
          for (int i = 0; i < framesPerBuffer; ++i, indl += strideL, indr += strideR) {
              bufferL[indl] = output_buffer[i];
              bufferR[indr] = output_buffer[inddestR++];
          }
      }
  } /*else {
      module->isAlive=0;
      return 1;
  }*/
  return 0;
}

extern "C" int audioroute_process_interleaved(void *engine_ptr, int index, float *buffer_interleaved, int framesPerBuffer, int nChannels, int sampleRate, MusicEvent *events, int eventsNum,  int instance_index, struct AudiorouteTimeInfo *timeInfo)
{
    return audioroute_process(engine_ptr, index, buffer_interleaved, 2, buffer_interleaved+1, 2, framesPerBuffer, nChannels, sampleRate, events, eventsNum, instance_index, timeInfo);
}

extern "C" int audioroute_process_non_interleaved(void *engine_ptr, int index, float *bufferL, float *bufferR, int framesPerBuffer, int nChannels, int sampleRate, MusicEvent *events, int eventsNum,  int instance_index, struct AudiorouteTimeInfo *timeInfo)
{
    return audioroute_process(engine_ptr, index, bufferL, 1, bufferR, 1, framesPerBuffer, nChannels, sampleRate, events, eventsNum, instance_index, timeInfo);
}

#define AM_SIG_ALRM SIGRTMAX

static __thread sigjmp_buf sig_env;

static void signal_handler(int sig, siginfo_t *info, void *context) {
  LOGI("Received signal %d.", sig);
  siglongjmp(sig_env, 1);
}

static void *run_module(void *arg) {
  LOGI("Entering run_module.");
  audio_module_runner *amr = (audio_module_runner *) arg;
  sb_wake(&amr->launched);
  audio_module *module = audioroute_get_audio_module(amr->shm_ptr, amr->index);

  timer_t timer;
  struct sigevent evp;
  evp.sigev_notify = SIGEV_THREAD_ID;
  evp.sigev_signo = AM_SIG_ALRM;
  evp.sigev_value.sival_ptr = module;
  evp.sigev_notify_thread_id = gettid();
  timer_create(CLOCK_MONOTONIC, &evp, &timer);

  struct itimerspec timeout;
  timeout.it_interval.tv_sec = 0;
  timeout.it_interval.tv_nsec = 0;
#ifdef NDEBUG
  timeout.it_value.tv_sec = RELEASE_STARTUP_TIMEOUT_SECONDS;  // 2 seconds timeout.
#else
  timeout.it_value.tv_sec = DEBUG_STARTUP_TIMEOUT_SECONDS;
#endif
  timeout.it_value.tv_nsec = 0;

  struct itimerspec cancel;
  cancel.it_interval.tv_sec = 0;
  cancel.it_interval.tv_nsec = 0;
  cancel.it_value.tv_sec = 0;
  cancel.it_value.tv_nsec = 0;

    AudiorouteTimeInfo timeInfo;

    bool abort=false;

    amr->alive=true;
    amr->exited=false;
    module->host_gave_up=0;

    sb_clobber_lock(audioroute_get_lock(amr->shm_ptr, module->ready)); // Take ownership of lock

  //if (!sigsetjmp(sig_env, 1)) {
    while (!abort) {
      //sb_wake(audioroute_get_barrier(amr->shm_ptr, module->report));
      sb_wait_and_clear(audioroute_get_barrier(amr->shm_ptr, module->wake), NULL);
      if (amr->done) {
          LOGW("Runner is done");
        break;
      }

        //LOGD("RUNNER AWAKE");
      /*if(!module->is_hosted)
          audioroute_collect_input(amr->shm_ptr, amr->index);*/
      //timer_settime(timer, 0, &timeout, NULL);  // Arm timer.

        switch(module->callbackType) {
            case CALLBACK_TYPE_KEEPALIVE:
                // NOP
                module->callbackType=AUDIOROUTE_CALLBACK_RESULT_SUCCESS;
                break;
            case CALLBACK_TYPE_SHUTDOWN:
                abort = true;
                LOGW("Abort runner requested");
                break;
            case CALLBACK_TYPE_INITIALIZE:
                if (amr->initialize_processing) {
                    int connectedInputBuses[MaxNumBuses];
                    int connectedOutputBuses[MaxNumBuses];
                    for(int i=0; i<module->numInputBuses; ++i)
                        connectedInputBuses[i]=2;
                    for(int i=0; i<module->numOutputBuses; ++i)
                        connectedOutputBuses[i]=2;

                    amr->initialize_processing(amr->context, module->sample_rate,
                                               module->buffer_frames, module->instance_index,
                                               connectedInputBuses, connectedOutputBuses);
                }
                break;
            default:
                amr->process(amr->context, module->sample_rate, module->buffer_frames,
                             module->input_channels,
                             audioroute_get_audio_buffer(amr->shm_ptr, module->input_buffer),
                             module->output_channels,
                             audioroute_get_audio_buffer(amr->shm_ptr, module->output_buffer),
                             module->musicEvents, module->numEvents, module->instance_index, &timeInfo);
                break;
        }
        //timer_settime(timer, 0, &cancel, NULL);  // Disarm timer.
        //LOGD("RUNNER SIGNALLING READY");
        sb_wake_lock(audioroute_get_lock(amr->shm_ptr, module->ready));
        //LOGD("RUNNER SIGNALLED READY");
        if(module->host_gave_up) {
            LOGD("Audioroute Host gave up");
            break;
        }
    }
  /*} else {
    __sync_bool_compare_and_swap(&amr->timed_out, 0, 1);
    // We can safely log now because we have already left the processing chain.
    LOGW("Process callback interrupted after timeout; terminating thread.");
  }*/

//  timer_delete(timer);
  LOGI("Leaving run_module.");
    onModuleReleased();
    amr->exited=1;

    //smi_unmap(amr->shm_ptr);
    //free(amr);
    //amr->shm_ptr=NULL;
  return NULL;
}

static void launch_thread(void *context, int sample_rate, int buffer_frames,
    int input_channels, const short *input_buffer,
    int output_channels, short *output_buffer) {
  audio_module_runner *amr = (audio_module_runner *) context;
  if (!--amr->launch_counter) {
    if (!pthread_create(&amr->thread, NULL, run_module, amr)) {
      pthread_setname_np(amr->thread, "AudioModule");
    } else {
      LOGW("Thread creation failed: %s", strerror(errno));
    }
  }
}

static size_t get_protected_size() {
  return BARRIER_OFFSET * MEM_PAGE_SIZE;
}

static audio_module_runner *runner=NULL;
audio_module_runner *audioroute_create(int version, int token, int index) {
  if (version != AUDIOROUTE_PROTOCOL_VERSION) {
    LOGW("Protocol version mismatch.");
    return NULL;
  }
    audio_module_runner *amr;
    if(NULL!=runner) amr=runner;
    else {
        amr = (audio_module_runner *) malloc(sizeof(audio_module_runner));
        amr->shm_ptr=NULL;
    }
  if (amr) {
    amr->shm_fd = token;
      if(amr->shm_ptr!=NULL)
          smi_unmap(amr->shm_ptr);
    amr->shm_ptr = smi_map(token);
    //smi_protect(amr->shm_ptr, get_protected_size()); // Doesn't work when converting to cpp
    amr->index = index;
    amr->done = 0;
    amr->timed_out = 0;
    amr->process = NULL;
    amr->context = NULL;
      amr->alive=0;
    amr->launch_counter = 3;  // Make sure that this number stays current.

    audio_module *module = audioroute_get_audio_module(amr->shm_ptr, amr->index);
    // Clear barriers, just in case.
    sb_clobber(audioroute_get_barrier(amr->shm_ptr, module->report));
    sb_clobber(audioroute_get_barrier(amr->shm_ptr, module->wake));
    //sb_clobber(audioroute_get_barrier(amr->shm_ptr, module->ready));

    OPENSL_STREAM *os = opensl_open(module->sample_rate, 0, 2,
        module->buffer_frames, launch_thread, amr);
    sb_clobber(&amr->launched);
    opensl_start(os);
    sb_wait(&amr->launched, NULL);
    opensl_close(os);

      module->is_hosted=999; // Signal host that plugin has received shared memory


      struct sigaction act;
    act.sa_sigaction = signal_handler;
    act.sa_flags = SA_SIGINFO;
    sigfillset(&act.sa_mask);
    sigaction(AM_SIG_ALRM, &act, NULL);

      const int SleepTime=1000*30;
      const int Microseconds=1000000;
      const int MaxWait=5*Microseconds/SleepTime;

      for(int i=0; i<MaxWait&&!amr->alive; ++i)
      {
          usleep(SleepTime);
      }
      if(!amr->alive)
      {
        LOGI("Failed to spin up module runner");
      }
  }
  return (audio_module_runner *) amr;
}

void audioroute_release(audio_module_runner *amr) {
  audio_module *module = audioroute_get_audio_module(amr->shm_ptr, amr->index);

  amr->done = 1;
  sb_wake(audioroute_get_barrier(amr->shm_ptr, module->wake));
  /*if(!amr->exited)
      pthread_join(amr->thread, NULL);

  smi_unmap(amr->shm_ptr);
  free(amr);*/
}

int audioroute_has_timed_out(audio_module_runner *amr) {
  return __sync_or_and_fetch(&amr->timed_out, 0);
}

static bool launched= false;

static void launch_thread1(void *context, int sample_rate, int buffer_frames,
                          int input_channels, const short *input_buffer,
                          int output_channels, short *output_buffer) {

    void *(*f)(void *)=(void *(*)(void *))context;
    launched=true;
    (*f)(NULL);
}

void LaunchThread(void *(*fun_ptr)(void *))
{
    launched=false;
    OPENSL_STREAM *os = opensl_open(48000, 0, 2,
                                    192, launch_thread, (void *)fun_ptr);
    opensl_start(os);
    while(!launched) usleep(1000000);
    opensl_close(os);
}