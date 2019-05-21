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
#include <oboe/Oboe.h>
#include "LiveEffectEngine.h"
#include "../../oboe/src/common/OboeDebug.h"

static const int kOboeApiAAudio = 0;
static const int kOboeApiOpenSLES = 1;

static LiveEffectEngine *engine = nullptr;

extern "C" {

JNIEXPORT bool JNICALL
Java_com_audioroute_hostsample_AudioEngine_create(JNIEnv *env,
                                                               jclass) {
    if (engine == nullptr) {
        engine = new LiveEffectEngine();
    }

    return (engine != nullptr);
}

JNIEXPORT void JNICALL
Java_com_audioroute_hostsample_AudioEngine_delete(JNIEnv *env,
                                                               jclass) {
    delete engine;
    engine = nullptr;
}

JNIEXPORT void JNICALL
Java_com_audioroute_hostsample_AudioEngine_setEngineOn(
    JNIEnv *env, jclass, jboolean isEngineOn) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine before calling this "
            "method");
        return;
    }

    engine->setEngineOn(isEngineOn);
}

JNIEXPORT bool JNICALL
Java_com_audioroute_hostsample_AudioEngine_isModuleAlive(
        JNIEnv *env, jclass classz, jlong engineRef, jint moduleIndex, jint instanceIndex) {
    if (engine == nullptr) {
        LOGE("Engine is null, you must call createEngine before calling this "  "method");
        return false;
    }

    return engine->isModuleAlive((void*)engineRef, moduleIndex, instanceIndex);
}

JNIEXPORT void JNICALL
Java_com_audioroute_hostsample_AudioEngine_initModule(
        JNIEnv *env, jclass classz, jlong engineRef, jint moduleIndex, jint instanceIndex, jboolean isInput, jboolean isOutput) {
    if (engine == nullptr) {
        LOGE(
                "Engine is null, you must call createEngine before calling this "
                        "method");
        return;
    }

    engine->InitModule((void*)engineRef, moduleIndex, instanceIndex, isInput, isOutput);
}

JNIEXPORT bool JNICALL
Java_com_audioroute_hostsample_AudioEngine_attachInput(
        JNIEnv *env, jclass, jboolean enabled) {
    if (engine == nullptr) {
        LOGE(
                "Engine is null, you must call createEngine before calling this "
                        "method");
        return false;
    }

    return engine->AttachInput(enabled);
}

JNIEXPORT bool JNICALL
Java_com_audioroute_hostsample_AudioEngine_attachOutput(
        JNIEnv *env, jclass, jboolean enabled) {
    if (engine == nullptr) {
        LOGE(
                "Engine is null, you must call createEngine before calling this "
                        "method");
        return false;
    }

    return engine->AttachOutput(enabled);
}

JNIEXPORT void JNICALL
Java_com_audioroute_hostsample_AudioEngine_sendMidiNoteOn(
        JNIEnv *env, jclass, jint note, jboolean noteOn) {
    if (engine == nullptr) {
        LOGE("Engine is null, you must call createEngine before calling this " "method");
        return;
    }

    engine->SendMidiNoteOn(note, noteOn);
}

JNIEXPORT void JNICALL
Java_com_audioroute_hostsample_AudioEngine_disconnectModule(
        JNIEnv *env, jclass, jlong engineRef, jint moduleIndex, jint instanceIndex) {
    if (engine == nullptr) {
        LOGE(
                "Engine is null, you must call createEngine before calling this "
                        "method");
        return;
    }

    engine->DisconnectModule((void*)engineRef, moduleIndex, instanceIndex);
}

JNIEXPORT void JNICALL
Java_com_audioroute_hostsample_AudioEngine_setRecordingDeviceId(
    JNIEnv *env, jclass, jint deviceId) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine before calling this "
            "method");
        return;
    }

    engine->setRecordingDeviceId(deviceId);
}

JNIEXPORT void JNICALL
Java_com_audioroute_hostsample_AudioEngine_setPlaybackDeviceId(
    JNIEnv *env, jclass, jint deviceId) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine before calling this "
            "method");
        return;
    }

    engine->setPlaybackDeviceId(deviceId);
}

JNIEXPORT jboolean JNICALL
Java_com_audioroute_hostsample_AudioEngine_setAPI(JNIEnv *env,
                                                               jclass type,
                                                               jint apiType) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine "
            "before calling this method");
        return JNI_FALSE;
    }

    oboe::AudioApi audioApi;
    switch (apiType) {
        case kOboeApiAAudio:
            audioApi = oboe::AudioApi::AAudio;
            break;
        case kOboeApiOpenSLES:
            audioApi = oboe::AudioApi::OpenSLES;
            break;
        default:
            LOGE("Unknown API selection to setAPI() %d", apiType);
            return JNI_FALSE;
    }

    return static_cast<jboolean>(engine->setAudioApi(audioApi) ? JNI_TRUE
                                                               : JNI_FALSE);
}

JNIEXPORT jboolean JNICALL
Java_com_audioroute_hostsample_AudioEngine_isAAudioSupported(
    JNIEnv *env, jclass type) {
    if (engine == nullptr) {
        LOGE(
            "Engine is null, you must call createEngine "
            "before calling this method");
        return JNI_FALSE;
    }
    return static_cast<jboolean>(engine->isAAudioSupported() ? JNI_TRUE
                                                             : JNI_FALSE);
}
}



