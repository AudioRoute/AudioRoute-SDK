/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 * Copyright 2019 n-Track S.r.l. All Rights Reserved.
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

#include "audio_module_jni.h"

#include "audio_module_internal.h"
#include <string>

extern "C" JNIEXPORT jint JNICALL
Java_com_ntrack_audioroute_AudioModule_getProtocolVersion
(JNIEnv *env, jclass cls) {
  return AUDIOROUTE_PROTOCOL_VERSION;
}

#define AudiorouteAndroidJavaClassNAME AudiorouteAndroidJavaClass
struct AudiorouteAndroidJavaClassNAME
{
    jclass theJavaClass;
    jobject theJavaObject;

    AudiorouteAndroidJavaClassNAME()
    {
        theJavaClass=NULL;
        theJavaObject=NULL;
    }
    JNIEnv* SetupJavaClassAndObjectReferences(JNIEnv *env, jobject inObject)
    {
        // set the reference to the object as global (to avoid problems between different jni calls):
        if (NULL!=theJavaObject) env->DeleteGlobalRef(theJavaObject);
        theJavaObject = env->NewGlobalRef(inObject);
        // Get reference to the class from the object:
        if (NULL!=theJavaClass) env->DeleteGlobalRef(theJavaClass);
        theJavaClass = (jclass) env->NewGlobalRef(env->GetObjectClass(theJavaObject));
        return env;
    }
    bool InitMethod(jmethodID &id, std::string methodName, std::string signature, JNIEnv* env)
    {
        id = env->GetMethodID(theJavaClass, methodName.c_str(), signature.c_str());
        if (JNI_TRUE==env->ExceptionCheck()) {
            //LOGE(" JNI exception in get method id for: %s",methodName.c_str());
            env->ExceptionDescribe();
            env->ExceptionClear();
            //_ASSERT(false);
            return false;
        }
        return true;
    }
    static JavaVM* jvm;
    static pthread_key_t threadDetachKey;
    static void SetJVM(JavaVM* inJvm) { jvm = inJvm; }
    static JNIEnv* GetEnv()
    {
        if (NULL==jvm) return NULL;

        // Try getting the Env for the current thread, and contextually
        // check whether the thread is attached, and attach if not.
        // Since we are setting a "specific" for each thread we attach,
        // we may be tempted to check for thread detach state using:
        // if ((env = pthread_getspecific (threadDetachKey)) == NULL)
        // but with this check also the main java thread would show as
        // detached (we never manually attached called setspecific for it).

        JNIEnv* env;

        if (jvm->GetEnv((void **)&env,JNI_VERSION_1_6)==JNI_EDETACHED)
        {
            jvm->AttachCurrentThread(&env,NULL);
            pthread_setspecific(threadDetachKey, env); // this env will be passed to the destroy callback
            // NB: we are not using the env value associated with the key
            // in the destroy callback, but we need to set the specific
            // anyway, so that conditions are met for the call of the callback
            // (the key must have a non-null value associated with it).
        }
        return env;
    }
    static bool CheckForException(std::string methodInfo, JNIEnv* env) // returns true if there was an exception
    {
        if (JNI_TRUE==env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            return true;
        }
        return false;
    }
};

extern "C" void DoSetJVM(JavaVM *jvm)
{
    AudiorouteAndroidJavaClassNAME::SetJVM(jvm);
}

void DetachCurrentThread(void* venv)
{
    //LOGD(" --------- DETACH CURRENT THREAD! ---------------");
    AudiorouteAndroidJavaClassNAME::jvm->DetachCurrentThread();
}
pthread_key_t CreateThreadDetachKey()
{
    pthread_key_t key;
    pthread_key_create(&key,DetachCurrentThread);
    return key;
}

JavaVM* AudiorouteAndroidJavaClassNAME::jvm = NULL;
pthread_key_t AudiorouteAndroidJavaClassNAME::threadDetachKey = CreateThreadDetachKey();

struct AudioModuleNative : public AudiorouteAndroidJavaClassNAME
{
    jmethodID onConnectionShutdownID=0;

    AudioModuleNative()
    {

    }
    void InitMethods(JNIEnv *env, jobject inObject)
    {
        SetupJavaClassAndObjectReferences(env, inObject);
        InitMethod(onConnectionShutdownID, "onConnectionShutdown", "()V", env);
    }
    void onShutdown() // Called by runner thread that's exiting, calls DetachCurrentThread at end
    {
        if (theJavaObject==NULL || onConnectionShutdownID == NULL) return;
        JNIEnv* env = GetEnv();
        env->CallVoidMethod(theJavaObject, onConnectionShutdownID);
        CheckForException(__PRETTY_FUNCTION__,env);

        DetachCurrentThread(env);
    }
    bool IsInitialized()
    {
        return 0!=onConnectionShutdownID;
    }
};

static AudioModuleNative audioModuleNative;

extern "C" JNIEXPORT jlong JNICALL
Java_com_ntrack_audioroute_AudioModule_createRunner
(JNIEnv *env, jobject obj, jint version, jint token, jint index) {
    if(!audioModuleNative.IsInitialized())
        audioModuleNative.InitMethods(env, obj);

    audioModuleNative.SetupJavaClassAndObjectReferences(env, obj);

  return (jlong) audioroute_create(version, token, index);
}

extern "C" JNIEXPORT void JNICALL
Java_com_ntrack_audioroute_AudioModule_releaseInternal
(JNIEnv *env, jobject obj, jlong p) {
  audio_module_runner *amr = (audio_module_runner *) p;
  audioroute_release(amr);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_ntrack_audioroute_AudioModule_hasTimedOut
(JNIEnv *env, jobject obj, jlong p) {
  audio_module_runner *amr = (audio_module_runner *) p;
  return audioroute_has_timed_out(amr);
}

extern "C" JNIEXPORT jint JNICALL Java_com_ntrack_audioroute_AudioModule_getFramesPerBuffer(JNIEnv *env, jobject obj, jlong p, jlong moduleIndex)
{
  audio_module_runner *amr = (audio_module_runner *) p;
  audio_module *module = audioroute_get_audio_module(amr->shm_ptr, moduleIndex);
  if(NULL== module) return -1;
  return module->buffer_frames;
}

extern "C" JNIEXPORT jint JNICALL Java_com_ntrack_audioroute_AudioModule_getSampleRate(JNIEnv *env, jobject obj, jlong p, jint moduleIndex)
{
  audio_module_runner *amr = (audio_module_runner *) p;
    if(NULL==amr) return 0;
  audio_module *module = audioroute_get_audio_module(amr->shm_ptr, moduleIndex);
  if(NULL== module) return -1;
  return module->sample_rate;
}

void onModuleReleased()
{
    audioModuleNative.onShutdown();
}

extern "C" JNIEXPORT void JNICALL Java_com_ntrack_audioroute_AudioModule_audiorouteModuleConfigure(JNIEnv *env, jobject obj, jlong p, jlong process_func, jlong init_func, jlong data)
{
    audio_module_runner *amr = (audio_module_runner *) p;
    if(NULL==amr) return;
    audioroute_configure(amr, (audio_module_process_t)process_func, (initialize_processing_t)init_func, (void*)data);
}
