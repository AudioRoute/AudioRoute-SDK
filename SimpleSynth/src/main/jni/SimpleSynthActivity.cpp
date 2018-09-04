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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include <math.h>

#include <oboe/Oboe.h>

#include "simplesynth.h"

static int sampleRate=0;
static int numChannels=0;
static oboe::AudioStream *stream=NULL;

#include <list>
static std::list<int32_t> midi_queue; // We're using locking because this sample is not designed for performance. Not the best strategy for realtime audio.
static std::mutex midi_queue_mutex;

class MyCallback : public oboe::AudioStreamCallback {
public:
    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames){
        const int MaxNotes=100;
        static MusicEvent events[MaxNotes];
        static int32_t notebuffer[MaxNotes];

        float *buf=static_cast<float *>(audioData);
        int numEvents=0;
        {
            std::lock_guard<std::mutex> lock(midi_queue_mutex);
            while(midi_queue.size()&&numEvents<MaxNotes)
            {
                notebuffer[numEvents]=midi_queue.front();
                midi_queue.pop_front();
                numEvents++;
            }
        }
        for(int i=0; i<numEvents; ++i)
        {
            GetMusicEventFromRawMIDI(notebuffer[i], &events[i]);
        }
        simplesynthData.instance[0].process(buf, numChannels>1 ? (buf+1) : NULL, numFrames, numChannels, sampleRate, events, numEvents);
        return oboe::DataCallbackResult::Continue;
    }
};

static MyCallback myCallback;

SIMPLESINTH_NATIVE void JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthActivity_sendMidiNote
        (JNIEnv *env, jobject obj, jint note, jint isNoteOn) {

    int32_t data=isNoteOn ? 0x90 : 0x80;
    data|=((int32_t)note)<<8;
    int velocity=127;
    if(!isNoteOn) velocity=0;
    data|=((int32_t)velocity)<<16;
    std::lock_guard<std::mutex> lock(midi_queue_mutex);
    midi_queue.push_back(data);
}

SIMPLESINTH_NATIVE void JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthActivity_startAudio
        (JNIEnv *env, jobject obj) {

    if(NULL!=stream) return;
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setCallback(&myCallback);
    oboe::Result result = builder.openStream(&stream);
    if (result != oboe::Result::OK){
        LOGI("Failed to create stream. Error: %s", oboe::convertToText(result));
        return;
    }
    oboe::AudioFormat format = stream->getFormat();
    LOGI("AudioStream format is %s", oboe::convertToText(format));

    sampleRate=stream->getSampleRate();
    numChannels=stream->getChannelCount();
    stream->requestStart();
}

SIMPLESINTH_NATIVE void JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthActivity_stopAudio
        (JNIEnv *env, jobject obj) {
    if(NULL!=stream) stream->close();
    stream=NULL;
}

SIMPLESINTH_NATIVE void JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthActivity_setWaveform
        (JNIEnv *env, jobject obj, jint instanceId, jint mode) {
    simplesynthData.instance[instanceId].waveform=mode;
}

SIMPLESINTH_NATIVE jint JNICALL
Java_com_ntrack_audioroute_simplesynth_SimpleSynthActivity_getWaveform
        (JNIEnv *env, jobject obj, jint instanceId) {

    return simplesynthData.instance[instanceId].waveform;
}
