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

import android.app.Activity
import android.content.Intent
import android.support.v7.app.AppCompatActivity
import android.os.Bundle
import android.view.View
import android.graphics.BitmapFactory
import android.os.Handler
import android.widget.Button
import android.widget.Toast
import com.ntrack.audioroute.AudioRouteHostController
import android.view.animation.BounceInterpolator
import android.animation.ObjectAnimator
import android.os.Build
import android.support.annotation.RequiresApi
import android.support.constraint.ConstraintLayout
import android.support.constraint.ConstraintSet
import android.transition.TransitionManager
import android.util.Log
import android.transition.AutoTransition
import android.R.animator
import android.animation.AnimatorInflater
import android.animation.Animator
import android.app.AlertDialog
import android.view.Menu
import android.view.MenuItem
import android.widget.Toolbar
import android.view.View.inflate



val INPUT_CONNECTION = 0
val EFFECT_CONNECTION = 1
val OUTPUT_CONNECTION = 2

class MainActivity : AppCompatActivity() {

    private lateinit var hostController: AudioRouteHostController

    override fun onDestroy() {
        super.onDestroy()
        Log.d("AudiorouteHost", "onDestroy")
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        handler.postDelayed(runnableCode, 1000)

        AudioEngine.instance.activity=this
        AudioEngine.instance.startEngine()
        hostController=AudioEngine.instance.hostController
        hostController.setHostNotificationIcon(BitmapFactory.decodeResource(resources, R.drawable.speakers))
        updateUI()

        val piano: Piano = findViewById(R.id.piano)
        piano.bringToFront()
        piano.setPianoKeyListener(onPianoKeyPress)

        showKeyboard()

    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        val inflater = getMenuInflater()
        inflater.inflate(R.menu.items, menu)
        return super.onCreateOptionsMenu(menu)
    }

    fun ShowCrashDialog() {

        val builder = AlertDialog.Builder(this@MainActivity)

        // Set the alert dialog title
        builder.setTitle("Simulate crash")

        // Display a message on alert dialog
        builder.setMessage("Do you want to simulate host crash?")

        // Set a positive button and its click listener on alert dialog
        builder.setPositiveButton("YES") { dialog, which ->
            //simulate crash
            throw RuntimeException("This is a crash")

        }

        // Display a neutral button on alert dialog
        builder.setNeutralButton("Cancel") { _, _ ->

        }

        // Finally, make the alert dialog using builder
        val dialog: AlertDialog = builder.create()

        // Display the alert dialog on app interface
        dialog.show()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {

        super.onOptionsItemSelected(item)

        when (item.getItemId()) {
            R.id.crash -> ShowCrashDialog()

        }

        return true

    }

    @RequiresApi(Build.VERSION_CODES.KITKAT)
    fun showKeyboard() {
        val constraintLayout = findViewById(R.id.layout) as ConstraintLayout
        val constraint1 = ConstraintSet()
        constraint1.clone(constraintLayout)
        val constraint2 = ConstraintSet()
        constraint2.clone(constraintLayout)

        //constraint2.clone(this, R.layout.activity_main_keyboard)
        constraint2.clear(R.id.piano, ConstraintSet.TOP)
        constraint2.connect(R.id.piano,ConstraintSet.BOTTOM, ConstraintSet.PARENT_ID,ConstraintSet.BOTTOM)

        var show:Boolean=true
        val pianoButton = findViewById<Button>(R.id.pianoBtn)
        pianoButton.setOnClickListener {
            bounceButton(pianoButton)
            val autoTransition = AutoTransition()
            autoTransition.duration = 100

            TransitionManager.beginDelayedTransition(constraintLayout, autoTransition)
            val constraint = if (show) constraint2 else constraint1
            constraint.applyTo(constraintLayout)
            pianoButton.background = if(show) resources.getDrawable(R.drawable.piano_on) else resources.getDrawable(R.drawable.piano)
            show=!show

            updateUI()
        }

    }

    val onPianoKeyPress = object : Piano.PianoKeyListener {

        override fun keyPressed(id: Int, action: Int) {
            if(action==Key.ACTION_KEY_DOWN) {
                Log.i("Piano", "Key DOWN pressed: $id")
                AudioEngine.instance.sendMidiNoteOn(64+id, true)
            }
            else if(action==Key.ACTION_KEY_UP) {
                Log.i("Piano", "Key UP pressed: $id")
                AudioEngine.instance.sendMidiNoteOn(64+id, false)
            }
            else
                Log.i("Piano", "Key pressed: $id")
        }

    }

    val handler: Handler = Handler()
    val runnableCode = object: Runnable {
        override fun run() {
            checkAppAreAlive()
            handler.postDelayed(this, 1000)
        }
    }

    fun onModuleClicked(module: ModuleHosted) : Boolean{
        if(module.connected) {
            if(module.alive) {
                hostController.showUserInterface(this, module.module)
            } else {

                hostController.resuscitateApp(this, AudioRouteHostController.OnModuleCreatedListener {
                    module.module=it

                    initModule(module)

                }, module.module)
            }

            return true
        }

        return false
    }

    fun bounceButton(view: View) {
        val animation = AnimatorInflater.loadAnimator(applicationContext, R.animator.zoomin)
        animation.setTarget(view)
        animation.start()

/*        val animY = ObjectAnimator.ofFloat(view, "translationY", -50f, 0f)
        animY.duration = 10//ms
        animY.interpolator = BounceInterpolator()
        animY.repeatCount = 1
        animY.start()
*/
    }

    fun inputClicked(view: View) {
        bounceButton(view)

        if(!onModuleClicked(AudioEngine.instance.inputModule))
            showAppList(INPUT_CONNECTION)
    }

    fun effectClicked(view: View) {
        bounceButton(view)

        if(!onModuleClicked(AudioEngine.instance.effectModule))
            showAppList(EFFECT_CONNECTION)
    }

    fun outputClicked(view: View) {
        bounceButton(view)

        if(!onModuleClicked(AudioEngine.instance.outputModule))
            showAppList(OUTPUT_CONNECTION)
    }

    fun showAppList(requestCode: Int) {

        val intent = Intent(this, AppListActivity::class.java).apply {
            putExtra("requestCode", requestCode)
        }
        startActivityForResult(intent, requestCode)

    }

    fun disconnectInput(view: View) {
        Toast.makeText(this, "Input disconnected", Toast.LENGTH_LONG).show()
        AudioEngine.instance.disconnectInput()
        updateUI()

    }

    fun disconnectEffect(view: View) {
        Toast.makeText(this, "Effect disconnected", Toast.LENGTH_LONG).show()
        AudioEngine.instance.disconnectEffect()
        updateUI()

    }

    fun disconnectOutput(view: View) {
        Toast.makeText(this, "Output disconnected", Toast.LENGTH_LONG).show()
        AudioEngine.instance.disconnectOutput()
        updateUI()

    }

    fun closeModule(view: View) {
        AudioEngine.instance.disconnectAll()
        updateUI()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)

        if (resultCode == Activity.RESULT_OK) {
            val result=data?.getStringExtra("result")

            if(result=="Mic") AudioEngine.instance.attachMic()
            //else if(result=="Spk") AudioEngine.instance.attachSpeakers()

            hostController.instantiateAudiorouteModule(this, result, AudioRouteHostController.OnModuleCreatedListener {
                lateinit var module: ModuleHosted
                if(requestCode==INPUT_CONNECTION) {
                    module=AudioEngine.instance.inputModule
                    module.isInput=true
                    AudioEngine.instance.micConnected=false
                } else if (requestCode==EFFECT_CONNECTION) {
                    module=AudioEngine.instance.effectModule
                    module.isInput=true
                    module.isOutput=true
                } else if (requestCode==OUTPUT_CONNECTION) {
                    module=AudioEngine.instance.outputModule
                    module.isOutput=true
                    AudioEngine.instance.speakerConnected=false
                }

                module.module = it
                module.connected=true
                module.alive=true
                initModule(module)
            })

            updateUI()
        }
    }

    fun initModule(module: ModuleHosted) {

        Toast.makeText(this, "Init Module", Toast.LENGTH_LONG).show()

        AudioEngine.instance.initModule(module.module.engineref, module.module.moduleindex, module.module.instanceId, module.isInput, module.isOutput)
        updateUI()

    }

    fun updateUI() {

        val openButton = findViewById<Button>(R.id.openInputBtn)
        openButton.visibility = /*if(AudioEngine.instance.isSomethingConnected()) View.VISIBLE else */View.INVISIBLE
        //val pianoButton = findViewById<Button>(R.id.pianoBtn)
        //pianoButton.visibility = if(AudioEngine.instance.inputModule.connected) View.VISIBLE else View.INVISIBLE
        //pianoButton.background = resources.getDrawable(R.drawable.piano)


        val inButton = findViewById<Button>(R.id.inputButton)
        val effButton = findViewById<Button>(R.id.effectButton)
        val outButton = findViewById<Button>(R.id.outputButton)

        val inEjectButton = findViewById<Button>(R.id.ejectInput2)
        val effEjectButton = findViewById<Button>(R.id.ejectEffect2)
        val outEjectButton = findViewById<Button>(R.id.ejectOutput)

        inEjectButton.visibility = if(AudioEngine.instance.isInputConnected()) View.VISIBLE else View.INVISIBLE
        effEjectButton.visibility = if(AudioEngine.instance.isEffectConnected()) View.VISIBLE else View.INVISIBLE
        outEjectButton.visibility = if(AudioEngine.instance.isOutputConnected()) View.VISIBLE else View.INVISIBLE

        if(AudioEngine.instance.inputModule.connected) {
            var icon=hostController.getPackageIcon(AudioEngine.instance.inputModule.module.packagename)
            icon.alpha= if(AudioEngine.instance.inputModule.alive) 255 else 100
            inButton.background = icon
        }
        else if(AudioEngine.instance.micConnected)
            inButton.background = resources.getDrawable(R.drawable.mic)
        else
            inButton.background = resources.getDrawable(R.drawable.plus_circle)

        if(AudioEngine.instance.effectModule.connected) {
            var icon=hostController.getPackageIcon(AudioEngine.instance.effectModule.module.packagename)
            icon.alpha= if(AudioEngine.instance.effectModule.alive) 255 else 100
            effButton.background = icon
        }
        else
            effButton.background = resources.getDrawable(R.drawable.plus_circle)

        if(AudioEngine.instance.outputModule.connected) {
            var icon=hostController.getPackageIcon(AudioEngine.instance.outputModule.module.packagename)
            icon.alpha= if(AudioEngine.instance.outputModule.alive) 255 else 100
            outButton.background = icon
        }
        else /*if(AudioEngine.instance.speakerConnected)*/
            outButton.background = resources.getDrawable(R.drawable.speakers)
        /*else
            outButton.background = resources.getDrawable(R.drawable.plus_circle)*/
    }

    fun checkModuleAlive(module: ModuleHosted) {
        val prevState=module.alive

        module.alive=(module.connected && AudioEngine.instance.isModuleAlive(module.module))

        if(prevState!=module.alive) updateUI()
    }

    fun checkAppAreAlive() {
        checkModuleAlive(AudioEngine.instance.inputModule)
        checkModuleAlive(AudioEngine.instance.effectModule)
        checkModuleAlive(AudioEngine.instance.outputModule)
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        hostController.onNewIntent(intent)
    }
    
    override fun onPause() {
        super.onPause()
        
        val connected:Boolean = AudioEngine.instance.isSomethingConnected()
        if(!AudioEngine.instance.isSomethingConnected())
            AudioEngine.instance.setEngineOn(false)

    }

    override fun onResume() {
        super.onResume()

        AudioEngine.instance.setEngineOn(true)
    }

}

