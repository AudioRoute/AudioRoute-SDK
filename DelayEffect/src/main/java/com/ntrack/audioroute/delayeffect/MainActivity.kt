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

package com.audioroute.delay

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.drawable.BitmapDrawable
import android.opengl.Visibility
import android.support.v7.app.AppCompatActivity
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.Button
import android.widget.SeekBar
import com.ntrack.audioroute.AudioModule
import com.ntrack.audioroute.AudiorouteActivityController

class MainActivity : AppCompatActivity(), SeekBar.OnSeekBarChangeListener {

    val MY_PREFS_NAME = "MyPrefsFile"

    private lateinit var controller: AudiorouteActivityController
    var isAudioRouteConnected: Boolean = false
    var feedback: Float = 0.5f
    var time: Float = 0.5f

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        controller= AudiorouteActivityController.getInstance()
        controller.setModuleLabel("Delay")
        controller.setModuleImage(BitmapFactory.decodeResource(resources, R.drawable.ic_launcher))

        val listener = object : AudiorouteActivityController.Listener {

            override fun onRouteConnected() {
                this@MainActivity.isAudioRouteConnected = true
                Handler(Looper.getMainLooper()).post(Runnable {
                    updateUI()
                })
            }

            override fun onRouteDisconnected() {
                this@MainActivity.isAudioRouteConnected=false
                Handler(Looper.getMainLooper()).post(Runnable {
                    updateUI()
                })
            }

            override fun createAudioModule(): AudioModule {
                return EffectModule()
            }
        }
        controller.setListener(listener)
        controller.onActivityCreated(this, false)

        val delayTime = findViewById(R.id.delayBar) as SeekBar
        delayTime.setOnSeekBarChangeListener(this)
        delayTime.progress = 50

        val delayFeedback = findViewById(R.id.feedbackBar) as SeekBar
        delayFeedback.setOnSeekBarChangeListener(this)
        delayFeedback.progress = 50

        isAudioRouteConnected=controller.isConnected()
    }

    override fun onResume() {
        super.onResume()
        controller.onResume()
        updateUI()
    }

    override fun onNewIntent (intent: Intent)
    {
        super.onNewIntent(intent)
        controller.onNewIntent(intent)
    }

    fun updateUI()
    {
        var hostButton = findViewById<Button>(R.id.hostButton)
        if(controller!=null) hostButton.background=controller.hostIcon
        hostButton.visibility = if(isAudioRouteConnected) View.VISIBLE else View.INVISIBLE
    }

    fun onHostClicked(view: View)
    {
        if (controller != null)
            controller.switchToHostApp()
    }

    override fun onProgressChanged(p0: SeekBar?, p1: Int, p2: Boolean) {
        val delayTime = findViewById(R.id.delayBar) as SeekBar
        val delayFeedback = findViewById(R.id.feedbackBar) as SeekBar

        if(p0==delayTime)
            time=p1*0.01f
        else
            feedback=p1*0.01f

        if(controller==null || controller.module==null) return

        val mod:EffectModule = controller.module as EffectModule
        mod.setParameter(feedback, time)

    }

    override fun onStartTrackingTouch(p0: SeekBar?) {
    }

    override fun onStopTrackingTouch(p0: SeekBar?) {
    }

}
