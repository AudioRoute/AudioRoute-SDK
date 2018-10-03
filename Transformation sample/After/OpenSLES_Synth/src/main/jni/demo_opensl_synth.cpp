/*
 * Copyright 2018 n-Track Software. All Rights Reserved.
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

#include "demo_opensl_synth.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

static int pause_processing=0;

float notesfrequencies[128]={0};

#include "audio_module.h"


struct my_native_data
{
    int data_you_may_need_for_audioroute;
};

// My audio callback
void process_func(void *context, int sample_rate, int framesPerBuffer,
                         int input_channels, const float *input_buffer,
                         int output_channels, float *output_buffer, MusicEvent *events, int eventsNum, int instance_index, struct AudiorouteTimeInfo *timeInfo);

// Audio processing initialization (called before the actual processing starts)
// NOTE: the init_func may be called by an arbitrary thread. If you need to update UI inside this function make sure
// you invoke the code responsible for updating the UI in the UI thread
static void init_func(void *context, int sample_rate, int framesPerBuffer, int instance_index, int connectedInputBuses[MaxNumBuses], int connectedOutputBuses[MaxNumBuses])
{
    // do your initialization stuff here, allocate resources based on sample_rate and framesPerBuffers etc.
}

// Tell Audioroute what your initialization and process callbacks are
extern "C" JNIEXPORT jlong JNICALL
        Java_com_ntrack_audioroute_demo_1opensl_1synth_MyAppAudioModule_configureNativeComponents
        (JNIEnv *env, jobject obj, jlong handle, jint channels) {
    my_native_data *data = new my_native_data();
    audioroute_configure_java(env, obj, process_func, init_func, data);
    return (jlong) data;
}


// Release data allocated when connected to Audioroute
extern "C" JNIEXPORT void JNICALL
Java_com_ntrack_audioroute_demo_1opensl_1synth_MyAppAudioModule_release__J
        (JNIEnv *env, jobject obj, jlong p) {
    my_native_data *data = (my_native_data *) p;
    if(data) delete data;
}
