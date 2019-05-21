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

package com.audioroute.hostsample

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.support.v4.app.ActivityCompat
import com.ntrack.audioroute.AudioRouteHostController

class ModuleHosted {

    var module: AudioRouteHostController.ModuleRuntimeInfo = AudioRouteHostController.ModuleRuntimeInfo()
    var connected : Boolean = false
    var alive : Boolean = true
    var isInput : Boolean = false
    var isOutput : Boolean = false
}

class AudioEngine {

    private val AUDIO_EFFECT_REQUEST = 0

    var engineRunning: Boolean = false
    var micConnected: Boolean = false
    var speakerConnected: Boolean = false

    var inputModule: ModuleHosted = ModuleHosted()
    var effectModule: ModuleHosted = ModuleHosted()
    var outputModule: ModuleHosted = ModuleHosted()
    lateinit var hostController: AudioRouteHostController
    lateinit var activity: Activity

    init {
        System.loadLibrary("hostsample")
        hostController = AudioRouteHostController()
    }

    private object GetInstance {
        val INSTANCE = AudioEngine()
    }

    companion object {
        val instance: AudioEngine by lazy { GetInstance.INSTANCE }
    }

    fun isModuleAlive(module: AudioRouteHostController.ModuleRuntimeInfo) : Boolean {
        return isModuleAlive(module.engineref, module.moduleindex, module.instanceId)
    }

    fun disconnectModule(module: AudioRouteHostController.ModuleRuntimeInfo) {
        disconnectModule(module.engineref, module.moduleindex, module.instanceId)
    }

    fun removeModule(module: AudioRouteHostController.ModuleRuntimeInfo) {
        disconnectModule(module)
        hostController.releaseModuleInstance(module)
    }

    fun isInputConnected() : Boolean {
        return micConnected||inputModule.connected
    }

    fun isEffectConnected() : Boolean {
        return effectModule.connected
    }

    fun isOutputConnected() : Boolean {
        return outputModule.connected
    }

    fun isSomethingConnected() : Boolean {
        return (micConnected || inputModule.connected || effectModule.connected || outputModule.connected)
    }

    fun disconnectInput() {
        setEngineOn(false)

        if(inputModule.connected) {
            removeModule(inputModule.module)
            inputModule.connected=false
        }

        micConnected=attachInput(false)

        setEngineOn(true)
    }

    fun disconnectEffect() {
        setEngineOn(false)

        if(effectModule.connected) {
            removeModule(effectModule.module)
            effectModule.connected=false
        }

        setEngineOn(true)
    }

    fun disconnectOutput() {
        setEngineOn(false)

        if(outputModule.connected) {
            removeModule(outputModule.module)
            outputModule.connected=false
        }

        //speakerConnected=attachOutput(false)

        setEngineOn(true)
    }

    fun disconnectAll() {

        setEngineOn(false)

        //REMOVE ALL
        if(inputModule.connected) {
            removeModule(inputModule.module)
            inputModule.connected=false
        }

        if(effectModule.connected) {
            removeModule(effectModule.module)
            effectModule.connected = false
        }

        if(outputModule.connected) {
            removeModule(outputModule.module)
            outputModule.connected = false
        }

        micConnected=attachInput(false)
        //speakerConnected=attachOutput(false)

        setEngineOn(true)

    }

    fun startEngine() {

        if(engineRunning) return

        engineRunning=create()
        setPlaybackDeviceId(0)
        setRecordingDeviceId(0)
        //AudioEngine.instance.setAPI(1) //0:AAUDIO 1:OpenSLES

        if (!isRecordPermissionGranted()) {
            requestRecordPermission()
            return
        }

        setEngineOn(true)
        if(!isOutputConnected()) attachSpeakers()
    }

    private fun isRecordPermissionGranted(): Boolean {
        return ActivityCompat.checkSelfPermission(activity, Manifest.permission.RECORD_AUDIO) === PackageManager.PERMISSION_GRANTED
    }

    private fun requestRecordPermission() {
        ActivityCompat.requestPermissions(
                activity,
                arrayOf(Manifest.permission.RECORD_AUDIO),
                AUDIO_EFFECT_REQUEST)
    }

    fun attachMic() {
        micConnected=attachInput(true)
    }

    fun attachSpeakers() {
        speakerConnected=attachOutput(true)
    }

    // Native methods
    external fun create(): Boolean
    external fun attachInput(enabled: Boolean) : Boolean
    external fun attachOutput(enabled: Boolean) : Boolean
    external fun setAPI(apiType: Int): Boolean
    external fun setEngineOn(isEngineOn: Boolean)
    external fun setRecordingDeviceId(deviceId: Int)
    external fun setPlaybackDeviceId(deviceId: Int)
    external fun delete()

    external fun disconnectModule(engineRef: Long, moduleIndex : Int, instanceIndex: Int)
    external fun initModule(engineRef: Long, moduleIndex : Int, instanceIndex: Int, isInput: Boolean, isOutput: Boolean)
    external fun isModuleAlive(engineRef: Long, moduleIndex : Int, instanceIndex: Int) : Boolean
    external fun sendMidiNoteOn(note : Int, noteOn: Boolean)

}

