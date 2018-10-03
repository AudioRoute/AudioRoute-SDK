/*
 * Copyright 2018 n-Track Software All Rights Reserved.
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
#include <mutex>
#include <assert.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "demo_opensl_synth.h"

simplesynth_instance instanceData;

static int sampleRate=0;
static const int numChannels=2;

#define BUFFER_SIZE_FRAMES 5120
#define BUFFER_SIZE (BUFFER_SIZE_FRAMES * sizeof(short) * numChannels)

static const int NUM_BUFFERS=2;
static short buffer[NUM_BUFFERS][BUFFER_SIZE]={0};
static int curBuffer = 0;

#include <list>
static std::list<int32_t> midi_queue; // We're using locking because this sample is not designed for performance. Not the best strategy for realtime audio.
static std::mutex midi_queue_mutex;

bool OpenSLWrap_Init();
void OpenSLWrap_Shutdown();

class MyCallbackObject {
    float tempBufferFloat[BUFFER_SIZE_FRAMES*numChannels];
public:
    int process(short *audioData, int32_t numFrames){
        const int MaxNotes=100;
        static int32_t notebuffer[MaxNotes];

        //float *buf=static_cast<float *>(audioData);
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
        for(int i=0; i<numFrames*numChannels; ++i)
            tempBufferFloat[i]=0;
        instanceData.process(tempBufferFloat, numChannels>1 ? (tempBufferFloat+1) : NULL, numFrames, numChannels, sampleRate, numEvents>0 ? notebuffer : NULL, numEvents);

        for(int i=0; i<numFrames*numChannels; ++i)
        {
            int temp=(int)(tempBufferFloat[i]*32768.0f);
            if(temp<-32768) temp=-32768;
            if(temp>32767) temp=32767;
            audioData[i]=(short)temp;
        }
        return 0;
    }
};

static MyCallbackObject synthCallback;

SIMPLESINTH_NATIVE void JNICALL
Java_com_ntrack_audioroute_demo_1opensl_1synth_OpenSLSynthActivity_sendMidiNote
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
Java_com_ntrack_audioroute_demo_1opensl_1synth_OpenSLSynthActivity_startAudio
        (JNIEnv *env, jobject obj) {
    OpenSLWrap_Init();
}

SIMPLESINTH_NATIVE void JNICALL
Java_com_ntrack_audioroute_demo_1opensl_1synth_OpenSLSynthActivity_stopAudio
        (JNIEnv *env, jobject obj) {
    OpenSLWrap_Shutdown();
}

SIMPLESINTH_NATIVE void JNICALL
Java_com_ntrack_audioroute_demo_1opensl_1synth_OpenSLSynthActivity_setWaveform
        (JNIEnv *env, jobject obj, jint mode) {
    instanceData.waveform=mode;
}

SIMPLESINTH_NATIVE jint JNICALL
Java_com_ntrack_audioroute_demo_1opensl_1synth_OpenSLSynthActivity_getWaveform
        (JNIEnv *env, jobject obj) {

    return instanceData.waveform;
}

// This is kinda ugly, but for simplicity I've left these as globals just like in the sample,
// as there's not really any use case for this where we have multiple audio devices yet.

// engine interfaces
static SLObjectItf engineObject;
static SLEngineItf engineEngine;
static SLObjectItf outputMixObject;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay;
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
static SLMuteSoloItf bqPlayerMuteSolo;
static SLVolumeItf bqPlayerVolume;

// This callback handler is called every time a buffer finishes playing.
// The documentation available is very unclear about how to best manage buffers.
// I've chosen to this approach: Instantly enqueue a buffer that was rendered to the last time,
// and then render the next. Hopefully it's okay to spend time in this callback after having enqueued.
static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    assert(bq == bqPlayerBufferQueue);
    assert(NULL == context);

    short *nextBuffer = buffer[curBuffer];
    int nextSize = BUFFER_SIZE;

    synthCallback.process(buffer[curBuffer], BUFFER_SIZE_FRAMES);

    SLresult result;
    result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, nextBuffer, nextSize);

    // Comment from sample code:
    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
    // which for this code example would indicate a programming error
    assert(SL_RESULT_SUCCESS == result);

    curBuffer ^= 1;  // Switch buffer
    // Render to the fresh buffer
}

// create the engine and output mix objects
bool OpenSLWrap_Init() {
    SLresult result;
    // create engine
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, 0, 0);
    assert(SL_RESULT_SUCCESS == result);
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);

    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

    sampleRate=44100;

    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,
            numChannels,
            SL_SAMPLINGRATE_44_1,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    // create audio player
    const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk, 2, ids, req);
    assert(SL_RESULT_SUCCESS == result);

    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL);
    assert(SL_RESULT_SUCCESS == result);
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);

    // Render and enqueue a first buffer. (or should we just play the buffer empty?)
    curBuffer = 0;
    //synthCallback.process(buffer[curBuffer], BUFFER_SIZE_FRAMES);

    (*bqPlayerBufferQueue)->Clear(bqPlayerBufferQueue);

    result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[curBuffer], BUFFER_SIZE);
    if (SL_RESULT_SUCCESS != result) {
        return false;
    }
    curBuffer ^= 1;
    result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer[curBuffer], BUFFER_SIZE);
    if (SL_RESULT_SUCCESS != result) {
        return false;
    }
    curBuffer ^= 1;

    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);

    return true;
}

// shut down the native audio system
void OpenSLWrap_Shutdown() {
    if (bqPlayerObject != NULL) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = NULL;
        bqPlayerPlay = NULL;
        bqPlayerBufferQueue = NULL;
        bqPlayerMuteSolo = NULL;
        bqPlayerVolume = NULL;
    }
    if (outputMixObject != NULL) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
    }
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }
}