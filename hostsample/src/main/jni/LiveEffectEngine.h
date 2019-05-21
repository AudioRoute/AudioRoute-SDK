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

#ifndef OBOE_LIVEEFFECTENGINE_H
#define OBOE_LIVEEFFECTENGINE_H

#include <jni.h>
#include <oboe/Oboe.h>
#include <string>
#include <thread>
#include <stdlib.h>
#include <audio_module.h>

class Module {

public:
    void* engineRef;
    int moduleIndex;
    int instanceIndex;
    bool connected=false;
    bool statusError=false;
    Module() {};
};

class LiveEffectEngine : public oboe::AudioStreamCallback {
   public:
    bool sendNoteMidi=true;
    float *mBufferInterleaved;
    LiveEffectEngine();
    ~LiveEffectEngine();
    void setRecordingDeviceId(int32_t deviceId);
    void setPlaybackDeviceId(int32_t deviceId);
    void setEngineOn(bool isOn);
    void InitModule(void *shm_ptr, int index, int instance_index, bool isInput, bool isOutput);
    void DisconnectModule(void *shm_ptr, int index, int instance_index);
    bool isModuleAlive(void *shm_ptr, int index, int instance_index);
    void SendMidiNoteOn(int note, bool noteOn);
    Module *inputModule, *effectModule, *outputModule;
    /*
     * oboe::AudioStreamCallback interface implementation
     */
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream,
                                          void *audioData, int32_t numFrames);
    void onErrorBeforeClose(oboe::AudioStream *oboeStream, oboe::Result error);
    void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error);

    bool setAudioApi(oboe::AudioApi);
    bool isAAudioSupported(void);

    bool AttachInput(bool enabled);
    bool AttachOutput(bool enabled);

    void FillMidiEvents();
private:

    MusicEvent events[128];
    int numEvents=0;
    bool micEnabled=false;
    bool speakersEnabled=false;
    bool mIsEffectOn = false;
    uint64_t mProcessedFrameCount = 0;
    uint64_t mSystemStartupFrames = 0;
    int32_t mRecordingDeviceId = oboe::kUnspecified;
    int32_t mPlaybackDeviceId = oboe::kUnspecified;
    oboe::AudioFormat mFormat = oboe::AudioFormat::I16;
    int32_t mSampleRate = oboe::kUnspecified;
    int32_t mInputChannelCount = oboe::ChannelCount::Stereo;
    int32_t mOutputChannelCount = oboe::ChannelCount::Stereo;
    oboe::AudioStream *mRecordingStream = nullptr;
    oboe::AudioStream *mPlayStream = nullptr;
    std::mutex mRestartingLock;
    oboe::AudioApi mAudioApi = oboe::AudioApi::AAudio;

    void openRecordingStream();
    void openPlaybackStream();

    void startStream(oboe::AudioStream *stream);
    void stopStream(oboe::AudioStream *stream);
    void closeStream(oboe::AudioStream *stream);

    void openAllStreams();
    void closeAllStreams();
    void restartStreams();

    oboe::AudioStreamBuilder *setupCommonStreamParameters(
        oboe::AudioStreamBuilder *builder);
    oboe::AudioStreamBuilder *setupRecordingStreamParameters(
        oboe::AudioStreamBuilder *builder);
    oboe::AudioStreamBuilder *setupPlaybackStreamParameters(
        oboe::AudioStreamBuilder *builder);
    void warnIfNotLowLatency(oboe::AudioStream *stream);
};

#endif  // OBOE_LIVEEFFECTENGINE_H
