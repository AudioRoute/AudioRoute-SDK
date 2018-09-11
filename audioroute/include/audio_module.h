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

/*
 * Native library for use with the native components of subclasses of
 * AudioModule.java. Audio modules must implement a process callback of type
 * audio_module_process_t and hook up the audioroute_configure function in this library
 * to the corresponding configure method in the Java class. See
 * LowpassModule.java and lowpass.{h,c} for an example of this interaction.
 */

#ifndef __AUDIO_MODULE_H__
#define __AUDIO_MODULE_H__

#include <stdint.h>

#define MaxNumBuses 8

#ifdef __cplusplus

extern "C" {
#endif

enum EventType { EventTypeNoteOn, EventTypeNoteOff, EventTypeController, EventTypePitchBend, EventTypePolyPressure};

// Structure used to pass music events (i.e. MIDI) from host to hosted app
typedef struct {
    enum EventType eventType;
    int index; // Note or controller num
    float value; // Velocity or controller value, normalized between 0 and 1
    int channel; // Midi channel
    int deltaFrames; // Frames from start of current buffer at which the event starts
    int noteLengthFrames; // Length of event, in frames. 0 if not available (e.g. real time event played via an instrument)
    float endingValue; // may be used for note off velocity
    float detuning; // in cents
} MusicEvent;

#define AROUTE_MIN(a,b) (((a)<(b))?(a):(b))
#define AROUTE_MAX(a,b) (((a)>(b))?(a):(b))

// Convert MusicEvent to Raw 4 bytes MIDI event
// Note that the conversion may loose precision for note velocities, controller values etc.
inline int32_t ConvertMusicEventToRawMIDI(MusicEvent *event)
{
    int32_t ev=0;
    float val=AROUTE_MAX(0.0f, AROUTE_MIN(event->value, 1.0f));
    switch(event->eventType) {
        case EventTypeNoteOn:
            return ((int32_t)0x90)|(AROUTE_MIN(event->channel, 16))|(((int32_t) (val*127))<<16)|(((int32_t)event->index)<<8);
        case EventTypeNoteOff:
            return 0x80|(AROUTE_MIN(event->channel, 16))|/*(((int32_t) (val*127))<<16)|*/(((int32_t)event->index)<<8);
        case EventTypeController:
            return 0xB0|(AROUTE_MIN(event->channel, 16))|(((int32_t) (val*127))<<16)|(((int32_t)event->index)<<8);
        case EventTypePitchBend:
        {
            int32_t valn=val * 16384;
            int32_t lsb=valn&0x7F;
            int32_t msb=valn>7;
            return 0xE0 | (AROUTE_MIN(event->channel, 16)) | (msb << 16) | (lsb << 8);
        }
        case EventTypePolyPressure:
            return 0xA0|(AROUTE_MIN(event->channel, 16))|(((int32_t) (val*127))<<16)|(((int32_t)event->index)<<8);
    }
    return ev;
}

inline int GetMusicEventFromRawMIDI(int32_t midi, MusicEvent *m)
{
    m->index=0;
    m->value=0;
    m->deltaFrames=0;
    m->noteLengthFrames=0;
    m->endingValue=0;
    m->detuning=0;
    m->channel=midi&0xf;

    switch(midi&0xf0) {
        case 0x90:
            m->eventType=EventTypeNoteOn;
            m->index=(midi&0xff00)>>8;
            m->value=((float)((midi&0xff0000)>>16))/127.0f;
            if(m->value==0)
                m->eventType=EventTypeNoteOff; // Note off disguised as note on
            return 1;
        case 0x80:
            m->eventType=EventTypeNoteOff;
            m->index=(midi&0xff00)>>8;
            return 1;
        case 0xB0:
            m->eventType=EventTypeController;
            m->index=(midi&0xff00)>>8;
            m->value=((float)((midi&0xff0000)>>16))/127.0f;
            return 1;
        case 0xE0:
        {
            m->eventType=EventTypePitchBend;
            int32_t lsb=midi&0xff00;
            lsb>>8;
            int32_t msb=midi&0xff0000;
            msb=msb>>16;
            msb=msb<<7;
            int32_t val=msb|lsb;
            m->value=val/16384.0;
            return 1;
        }
        case 0xA0:
            m->eventType=EventTypePolyPressure;
            m->index=(midi&0xff00)>>8;
            m->value=((float)((midi&0xff0000)>>16))/127.0f;
            return 1;
    }
    return 0;
}

// Structure used to pass streaming position and state
struct AudiorouteTimeInfo
{
    uint64_t sampleOffset;
    int32_t bpm; // 1000th of beats, i.e. 120 bpm = 120000
    int32_t timeSignatureNumerator;
    int32_t timeSignatureDenominator;
    enum TimeInfoFlags { Play=1, Rec=2};
    uint64_t flags; // Bitwise combination of TimeInfoFlags
};

/*
 * Processing callback; takes a processing context (which is just a pointer to
 * whatever data you want to pass to the callback), the sample rate, the buffer
 * size in frames, the number of input and output channels, as well as input
 * and output buffers whose size is the number of channels times the number of
 * frames per buffer. Buffers are non-interleaved (each channel buffer is
 * concatenated to the next channel).
 *
 * This function will be invoked on a dedicated audio thread, and so any data
 * in the context that may be modified concurrently must be protected (e.g., by
 * gcc atomics) to prevent race conditions.
 */
typedef void (*audio_module_process_t)
    (void *context, int sample_rate, int framesPerBuffer,
     int input_channels, const float *input_buffer,
     int output_channels, float *output_buffer, MusicEvent *events, int eventsNum, int instance_index, struct AudiorouteTimeInfo *timeInfo);

/*
 * Initialize processing callback; takes a processing context (which is just a pointer to
 * whatever data you want to pass to the callback), the sample rate, the buffer
 * size in frames, the number of input and output channels.
 *
 * This function may be used to perform lengthy operations such as allocate buffers, read
 * resources from disk etc. It will not typically be called in the audio thread so you must
 * take care of synchronizing with it, altough callbacks won't normally be invoked
 * during the initialization phase
 */

typedef void (*initialize_processing_t)(void *context, int sample_rate, int framesPerBuffer, int instance_index, int connectedInputBuses[MaxNumBuses], int connectedOutputBuses[MaxNumBuses]);

/*
 * Configures the audio module with a native audio processing method and
 * its context. The handle parameter is the handle that the AudioModule class
 * passes to the protected configure method. On the Java side, this handle is
 * of type long, and so in C it must be cast from jlong to void*.
 */
struct audioroute_engine;
void audioroute_configure_engine(void *handle, audio_module_process_t process, initialize_processing_t initialize_processing, void *context, struct audioroute_engine **p);
void audioroute_configure(void *handle, audio_module_process_t process, initialize_processing_t initialize_processing, void *context);

#ifdef __cplusplus
}

#include <jni.h>
__inline void audioroute_configure_java(JNIEnv *env, jobject obj, audio_module_process_t process, initialize_processing_t initialize_processing, void *context)
{
    jmethodID audiorouteConfigure = env->GetMethodID(env->GetObjectClass(obj), "audiorouteConfigure", "(JJJ)V");
    env->CallVoidMethod(obj, audiorouteConfigure, (jlong)process, (jlong)initialize_processing, (jlong)context);
}

#endif



#endif
