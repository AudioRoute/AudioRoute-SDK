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
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import com.ntrack.audioroute.AudioRouteHostController


class AppListActivity: AppCompatActivity() {

    private lateinit var recyclerView: RecyclerView
    private lateinit var viewAdapter: RecyclerView.Adapter<*>
    private lateinit var viewManager: RecyclerView.LayoutManager
    private lateinit var hostController: AudioRouteHostController

    private fun partItemClicked(data: MyData) {

        val returnIntent = Intent()
        returnIntent.putExtra("result", data.packageName)
        setResult(Activity.RESULT_OK, returnIntent)

        finish()
    }

    fun FilterMultiInstance(item: AudioRouteHostController.ModuleInfo): Boolean {
        if(item.supportsMultiInstance) return true

        return  !((AudioEngine.instance.isInputConnected() && AudioEngine.instance.inputModule.module.packagename==item.packagename) ||
                (AudioEngine.instance.isEffectConnected() && AudioEngine.instance.effectModule.module.packagename==item.packagename) ||
                (AudioEngine.instance.isOutputConnected() && AudioEngine.instance.outputModule.module.packagename==item.packagename))
    }

    fun FilterApp(requestCode: Int, item: AudioRouteHostController.ModuleInfo) : Boolean {
        return  FilterMultiInstance(item) &&
                ((requestCode==INPUT_CONNECTION && item.generator==1) ||
                (requestCode==EFFECT_CONNECTION && item.effect==1) ||
                (requestCode== OUTPUT_CONNECTION && item.sink==1))

    }

    private fun getBitmapFromDrawable(drawable: Drawable): Bitmap? {
        val bmp = Bitmap.createBitmap(drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bmp)
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight())
        drawable.draw(canvas)
        return bmp
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_app_list)

        val requestCode=intent.getIntExtra("requestCode", 0)
        if(requestCode==INPUT_CONNECTION) {
            setTitle("Input App List")
        } else if (requestCode==EFFECT_CONNECTION) {
            setTitle("Effect App List")
        } else if (requestCode==OUTPUT_CONNECTION) {
            setTitle("Output App List")
        }

        hostController = AudioRouteHostController()
        hostController.scanInstalledModules(this, AudioRouteHostController.ScanModulesListener {
            val items = ArrayList<MyData>()

            if (requestCode == INPUT_CONNECTION) //Mic
                items.add(MyData("Microphone", "Mic", BitmapFactory.decodeResource(resources, R.drawable.mic)))

            if (requestCode == OUTPUT_CONNECTION)//Speakers
                items.add(MyData("Speakers", "Spk", BitmapFactory.decodeResource(resources, R.drawable.speakers)))

            for (item: AudioRouteHostController.ModuleInfo in it.orEmpty()) {

                if (FilterApp(requestCode, item)) {
                    var bitmap = getBitmapFromDrawable(hostController.getPackageIcon(item.packagename));
                    if(bitmap!=null) {
                        var bmp : Bitmap = bitmap;
                        items.add(MyData(item.friendlyName, item.packagename, bmp))
                    }
                }
            }

            viewAdapter = RecyclerViewAdapter(items, { data: MyData -> partItemClicked(data) })
            viewManager = LinearLayoutManager(this)

            recyclerView = findViewById<RecyclerView>(R.id.recyclerView).apply {
                setHasFixedSize(true)
                layoutManager = viewManager
                adapter = viewAdapter
            }
        }, false)

    }
}
