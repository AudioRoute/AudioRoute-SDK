/*
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

#include <jni.h>

#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <string>
#include "audio_module.h"
#include <android/log.h>

#define LOGI(...) \
  __android_log_print(ANDROID_LOG_INFO, "audioroute", __VA_ARGS__)
#define LOGW(...) \
  __android_log_print(ANDROID_LOG_WARN, "audioroute", __VA_ARGS__)

class DelayProcessor
{

public:

    float *delayBuffer;
    int delayBufferLength=10;
    int delayReadPosition;
    int delayWritePosition;

    float dryMix=0.5;
    float wetMix=0.5;
    float feedback=0.5;
    float delayLength=0.5;
    double mSampleRate=44100;

    void init (double sampleRate, int samplesPerBlock, int input_channels)
    {
        mSampleRate=sampleRate;
        delayReadPosition = 0;
        delayWritePosition = 0;

        // Use this method as the place to do any pre-playback
        // initialisation that you need..
        delayBufferLength  = (int) 2.0 * sampleRate;
        if (delayBufferLength < 1)
            delayBufferLength = 1;

        delayBuffer = new float[delayBufferLength];
        for (int i=0;i<delayBufferLength;i++)
            delayBuffer[i]=0;

        delayReadPosition = (int) (delayWritePosition - (delayLength * sampleRate) + delayBufferLength) % delayBufferLength;
    }

    void setDelayLenght(float lenght) {
        delayReadPosition = (int) (delayWritePosition - (lenght * mSampleRate) + delayBufferLength) % delayBufferLength;
    }

    void process(const float *input_buffer, float *output_buffer, int framesPerBuffer, int input_channels) {
        int dpr, dpw;

        for (int j = 0; j < input_channels; ++j) {

            const float *channelData = input_buffer;
            float *delayData = delayBuffer;

            dpr = delayReadPosition;
            dpw = delayWritePosition;

            for (int i = 0; i < framesPerBuffer; ++i) {

                const float in = input_buffer[i];
                float out = 0.0;
                float delayed=delayData[dpr];
                out = (dryMix * in + wetMix * delayData[dpr]);

                delayData[dpw] = in + (delayData[dpr] * feedback);

                if (++dpr >= delayBufferLength)
                    dpr = 0;
                if (++dpw >= delayBufferLength)
                    dpw = 0;
                output_buffer[i] = out;
            }

            input_buffer+=framesPerBuffer;
            output_buffer+=framesPerBuffer;
        }

        delayReadPosition = dpr;
        delayWritePosition = dpw;
    }
};

// My audio callback
static void process_func(void *context, int sample_rate, int framesPerBuffer,
                         int input_channels, const float *input_buffer,
                         int output_channels, float *output_buffer, MusicEvent *events, int eventsNum, int instance_index, struct AudiorouteTimeInfo *timeInfo) {
    DelayProcessor *delay = (DelayProcessor *) context;
    delay->process(input_buffer, output_buffer, framesPerBuffer, input_channels);
}

// Audio processing initialization (called before the actual processing starts)
static void init_func(void *context, int sample_rate, int framesPerBuffer, int instance_index, int *inputBuses, int *outputBuses)
{
    // do your initialization stuff here, allocate resources based on sample_rate and framesPerBuffers etc.
    DelayProcessor *delay = (DelayProcessor *) context;
    delay->init(sample_rate, framesPerBuffer, inputBuses[0]);
}


extern "C" JNIEXPORT void JNICALL
Java_com_audioroute_delay_EffectModule_setParameter
        (JNIEnv *env, jobject obj, jlong p, jfloat feedback, jfloat time) {
    DelayProcessor *data = (DelayProcessor *) p;

    data->feedback=feedback;
    data->setDelayLenght(time);

}

extern "C" JNIEXPORT void JNICALL
Java_com_audioroute_delay_SimpleEffectModule_release__J
        (JNIEnv *env, jobject obj, jlong p) {
    DelayProcessor *data = (DelayProcessor *) p;
    free(data);
}

// Tell Audioroute what your initialization and process callbacks are
extern "C" JNIEXPORT jlong JNICALL
Java_com_audioroute_delay_EffectModule_configureNativeComponents
        (JNIEnv *env, jobject obj, jlong handle, jint channels) {
    DelayProcessor *data = new DelayProcessor();

    if (data) {
        audioroute_configure_java(env, obj, process_func, init_func, data);
     }
    return (jlong) data;
}