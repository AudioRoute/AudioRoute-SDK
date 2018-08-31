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
#include "audio_module.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#include <math.h>

#define LOGI(...) \
  __android_log_print(ANDROID_LOG_INFO, "audioroute", __VA_ARGS__)
#define LOGW(...) \
  __android_log_print(ANDROID_LOG_WARN, "audioroute", __VA_ARGS__)
#include <jni.h>
#include <unistd.h>

#define SIMPLESINTH_NATIVE extern "C" JNIEXPORT


#define RANGE 1

float notesfrequencies[128];

void CookNoteFrequencies()
{
    float rootA = 440;
    notesfrequencies[57] = rootA;
    for (int i = 58; i < 127; i++)
    {
        notesfrequencies[i] = rootA * pow(2, (i - 57) / 12.0);
    }
    for (int i = 56; i >= 0; i--) {
        notesfrequencies[i] = rootA * pow(2, (i - 57) / 12.0);
    }

}
class voice
{
    float frequency;
    float period;
    long long phase;
    int on;
    float amplitude;
    float targetAmplitude;
    int note;
    int waveform;
    int nextNote;
    int samplingFrequency;
#define DECAY 0.995
#define DECAY_REPLACE_VOICE 0.95
#define AMPLITUDE_SILENCE 0.000000005
public:
    void reset()
    {
        on=0;
        phase=0;
        note=-1;
    }
    void turn_off()
    {
        on=3;
        //reset();
    }
    void turn_on(int note, int samplingfrequency, int waveform, float amplitude){
        if(on!=0) {
            if(on==99) return;
            on=99;
            nextNote=note;
            this->waveform=waveform;
            targetAmplitude=amplitude;
            samplingFrequency=samplingfrequency;
            return;
        }
        init(note, samplingfrequency, waveform);
        on=1;
        phase=0;
        this->targetAmplitude=amplitude;
        this->amplitude=0;
    }
    int currentNote()
    {
        return note;
    }
    voice()
    {
        reset();
    }
#define PHI 3.1415926535897932384626433832795
private:
    void init(int note, int samplingfrequency, int waveform)
    {
        reset();
        this->waveform=waveform;
        frequency=notesfrequencies[note];
        float discFreq=frequency/(2*samplingfrequency);
        period=1/discFreq;
        if(waveform==0) period/=2*PHI;
        this->note=note;
    }
public:
    float process()
    {
        if(on==0) return 0;
        else if(on==99) {
            amplitude = amplitude * DECAY_REPLACE_VOICE;
            if (amplitude < AMPLITUDE_SILENCE) {
                on=0;
                turn_on(nextNote, samplingFrequency, waveform, targetAmplitude);
            }
        } else if(on==3) {
            amplitude = amplitude * DECAY;
            if(amplitude<AMPLITUDE_SILENCE) {
                reset();
                return 0;
            }
        } else if(on==1) {
            amplitude += 0.00001;
            if((targetAmplitude-amplitude)<0.00005) {
                on=2;
            }
        }
        if(0==waveform) { // Pure tone
            return amplitude * sin(((float) phase++) / period);
        } else { // Saw tooth
            return amplitude * (((float) ((phase++)%((int)period))) / period);
        }
    }
    int is_on()
    {
        return on==2||on==1;
    }
};
#define NumVoices 10
struct simplesynth_instance{
  int waveform;  // RC lowpass filter coefficient, between 0 and RANGE.
  voice voices[NumVoices];
    int samplerate;
    int lastVoice;

    simplesynth_instance() {
        initialize();
    }
    void initialize()
    {
        waveform = 0;
        lastVoice=0;
    }
};

static int pause_processing=0;

const int MaxInstances = 24;
typedef struct {
    simplesynth_instance instance[MaxInstances];
} simplesynth_data;

static void init_func(void *context, int sample_rate, int framesPerBuffer, int instance_index, int connectedInputBuses[MaxNumBuses], int connectedOutputBuses[MaxNumBuses])
{
    CookNoteFrequencies();
    LOGI("simplesynth initializing processing instance %d: channels: %d framesperbuffer: %d sampling freq: %d", instance_index, connectedOutputBuses[0], framesPerBuffer, sample_rate);
    simplesynth_instance &data = ((simplesynth_data *) context)->instance[instance_index];
    if(data.samplerate!=sample_rate)
    {
        data.samplerate=sample_rate;
        for(int i=0; i<NumVoices; ++i)
            data.voices[i].reset();
    }
}

float linToGain(float linval)
{
    return powf(10.f, -1*(1-linval))*0.4f;
}

static void process_func(void *context, int sample_rate, int framesPerBuffer,
    int input_channels, const float *input_buffer,
    int output_channels, float *output_buffer, MusicEvent *events, int eventsNum, int instance_index, AudiorouteTimeInfo *timeInfo) {
    simplesynth_instance &data = ((simplesynth_data *) context)->instance[instance_index];
    int currentEvent=0;
    while(pause_processing) sleep(1);
    for(int i=0; i<framesPerBuffer; ++i) {
        for(; currentEvent<eventsNum; ++currentEvent)
        {
            if(events[currentEvent].deltaFrames>=i) {
                if (EventTypeNoteOn==events[currentEvent].eventType) {
                    data.lastVoice++;
                    data.lastVoice=data.lastVoice%NumVoices;
                    data.voices[data.lastVoice].turn_on(events[currentEvent].index, sample_rate, data.waveform, linToGain(events[currentEvent].value));
                }
                else if (EventTypeNoteOff==events[currentEvent].eventType) {
                    for(int v=0; v<NumVoices; ++v) {
                        if(data.voices[v].is_on()&&data.voices[v].currentNote()==events[currentEvent].index)
                            data.voices[v].turn_off();
                    }
                }
            } else break;
        }
        float sample=0;
        for(int voice=0; voice<NumVoices; ++voice)
            sample+=data.voices[voice].process();

        for(int ch=0; ch<output_channels; ++ch)
        {
            output_buffer[i+ch*framesPerBuffer]=sample;
        }
    }
}

SIMPLESINTH_NATIVE jlong JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthModule_configureNativeComponents
(JNIEnv *env, jobject obj, jlong handle, jint channels) {
  simplesynth_data *data = (simplesynth_data *)malloc(sizeof(simplesynth_data));
    memset(data, 0, sizeof(*data));
  if (data) {
      audioroute_configure_java(env, obj, process_func, init_func, data);
  } else {
      free(data);
      data = NULL;
    }
  return (jlong) data;
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
simplesynth_data *data = (simplesynth_data *) p;
  free(data);
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