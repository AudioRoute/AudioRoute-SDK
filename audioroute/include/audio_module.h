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
#ifdef __cplusplus
extern "C" {
#endif

enum EventType { EventTypeNoteOn, EventTypeNoteOff, EventTypeController, EventTypePitchBend, EventTypePolyPressure};

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
     int output_channels, float *output_buffer, MusicEvent *events, int eventsNum, int instance_index);

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

typedef void (*initialize_processing_t)(void *context, int sample_rate, int framesPerBuffer, int input_channels, int output_channels, int instance_index);

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
#endif
#endif
