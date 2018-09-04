/*
 * Copyright 2013 Google Inc. All Rights Reserved.
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

#include "simplesynth.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

static int pause_processing=0;

float notesfrequencies[128]={0};
simplesynth_data simplesynthData;

static void init_func(void *context, int sample_rate, int framesPerBuffer, int instance_index, int connectedInputBuses[MaxNumBuses], int connectedOutputBuses[MaxNumBuses])
{
    LOGI("simplesynth initializing processing instance %d: channels: %d framesperbuffer: %d sampling freq: %d", instance_index, connectedOutputBuses[0], framesPerBuffer, sample_rate);
    simplesynth_instance &data = ((simplesynth_data *) context)->instance[instance_index];
    if(data.samplerate!=sample_rate)
    {
        data.samplerate=sample_rate;
        for(int i=0; i<NumVoices; ++i)
            data.voices[i].reset();
    }
}

static void process_func(void *context, int sample_rate, int framesPerBuffer,
    int input_channels, const float *input_buffer,
    int output_channels, float *output_buffer, MusicEvent *events, int eventsNum, int instance_index, AudiorouteTimeInfo *timeInfo) {
    simplesynth_instance &data = ((simplesynth_data *) context)->instance[instance_index];
    while(pause_processing) sleep(1);
    data.process(output_buffer, output_channels>1? (output_buffer+framesPerBuffer) : NULL, framesPerBuffer, 1, sample_rate, events, eventsNum);
}

SIMPLESINTH_NATIVE jlong JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthModule_configureNativeComponents
(JNIEnv *env, jobject obj, jlong handle, jint channels) {
      audioroute_configure_java(env, obj, process_func, init_func, &simplesynthData);
  return (jlong) &simplesynthData;
}

SIMPLESINTH_NATIVE void JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthModule_configureNativeInstance
        (JNIEnv *env, jobject obj, jlong handle, jlong ptr, jint newInstance) {

    simplesynth_data *data = (simplesynth_data *)ptr;
    data->instance[newInstance].initialize();
}



SIMPLESINTH_NATIVE void JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthModule_release
(JNIEnv *env, jobject obj, jlong p) {
    // Release resources here
}

SIMPLESINTH_NATIVE void JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthModule_setParameter
(JNIEnv *env, jobject obj, jlong p, jlong instanceIndex, jint param, jdouble val) {
simplesynth_data *data = (simplesynth_data *) p;
    if(param==0) {
        __sync_bool_compare_and_swap(&data->instance[instanceIndex].waveform,
                                     data->instance[instanceIndex].waveform, (int) (RANGE * val));
    }
    else {
        __sync_bool_compare_and_swap(&pause_processing,
                                     pause_processing, (int)val);
    }
}


SIMPLESINTH_NATIVE jdouble JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthModule_getParameter
        (JNIEnv *env, jobject obj, jlong p, jint param, jlong instanceIndex) {
    simplesynth_data *data = (simplesynth_data *) p;
    return data->instance[instanceIndex].waveform/RANGE;
}