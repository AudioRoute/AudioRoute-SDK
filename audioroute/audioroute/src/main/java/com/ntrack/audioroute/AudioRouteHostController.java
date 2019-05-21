/*
 * Copyright 2013 Google Inc. All Rights Reserved.
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

package com.ntrack.audioroute;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.Messenger;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;
import android.util.Pair;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.nio.BufferUnderflowException;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.time.LocalDate;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.Dictionary;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Scanner;

public class AudioRouteHostController {

    static {
        System.loadLibrary("audiomodule");
    }

    final String TAG="AudioRoute";
    String moduleName;

    boolean isExistingApp=false;
    long audioRouteNativeRef=0;

    class AudiorouteEngineInfo
    {
        public Long instanceRef;
        public int engineData;
        public AudiorouteEngineInfo(Long l, int e)
        {
            instanceRef=l;
            engineData=e;
        }
    }
    Map<String, Pair<AudiorouteEngineInfo, Integer>> audiorouteHostInstances = new HashMap<String, Pair<AudiorouteEngineInfo, Integer> >();

    boolean showForegroundServiceNotification=true;

    public void setShowForegroundServiceNotification(boolean showForegroundService) {
        showForegroundServiceNotification=showForegroundService;
    }
    public void releaseModuleInstance(ModuleRuntimeInfo minfo)
    {
        Pair<AudiorouteEngineInfo, Integer> pair=audiorouteHostInstances.get(minfo.packagename);
        if(null==pair) return;
        if(pair.second<=1) {
            Audioroute.release(minfo.engineref, minfo.instanceId);
            audiorouteHostInstances.remove(minfo.packagename);
        }
        else audiorouteHostInstances.put(minfo.packagename, new Pair<AudiorouteEngineInfo, Integer>(pair.first, pair.second-1));
        if(audiorouteHostInstances.isEmpty())
        {
            if(showForegroundServiceNotification) AudioModuleForegroundService.stopService(activity);
        }
    }
    public static class ModuleRuntimeInfo
    {
        public int moduleindex;
        public int instanceId;
        public String packagename;
        public long engineref;
    }
    public interface OnModuleCreatedListener
    {
        void onModuleCreated(ModuleRuntimeInfo m);
    }
    OnModuleCreatedListener onModuleCreatedListener;

    public void onNewIntent(Intent intent)
    {
        if(intent.hasExtra(AudiorouteActivityController.IntentAudiorouteModuleIndex)) {
            int moduleindex=intent.getIntExtra(AudiorouteActivityController.IntentAudiorouteModuleIndex, -1);
            int instanceId=intent.getIntExtra(AudiorouteActivityController.IntentAudiorouteInstanceIndex, -1);
            String packagename=intent.getStringExtra(AudiorouteActivityController.IntentReturningFromApp);
            int protocolVersion=intent.getIntExtra(AudiorouteActivityController.IntentProtocolVersion, 0);
            if(protocolVersion!=AudioModule.getProtocolVersion())
            {
                AudiorouteActivityController.showMismatchProtocol(activity);
                return;
            }
            int moduleCreated=intent.getIntExtra(AudiorouteActivityController.IntentAudiorouteModuleCreated, 0);
            if(moduleCreated!=0&&onModuleCreatedListener!=null) {
                ModuleRuntimeInfo m=new ModuleRuntimeInfo();
                m.moduleindex=moduleindex;
                m.instanceId=instanceId;
                m.packagename=packagename;
                m.engineref=getNativeEngineReference();
                onModuleCreatedListener.onModuleCreated(m);
            }
        }
    }
    Bitmap myModuleIcon;
    public void setHostNotificationIcon(Bitmap bitmap)
    {
        myModuleIcon=bitmap;
    }
    public void resuscitateApp(Activity myActivity, OnModuleCreatedListener listener, ModuleRuntimeInfo minfo)
    {
        //audiorouteHostInstances.remove(moduleName);
        instantiateAudiorouteModuleInternal(myActivity, minfo.packagename, myModuleIcon, listener, minfo.instanceId, minfo.engineref);
    }
    public void instantiateAudiorouteModule(Activity myActivity, String packageName, OnModuleCreatedListener listener) {
        instantiateAudiorouteModuleInternal(myActivity, packageName, myModuleIcon, listener, -1, 0);
    }
    boolean isModuleConnected(long engineRef)
    {
        return Audioroute.isModuleConnected(engineRef);
    }
    void instantiateAudiorouteModuleInternal(Activity myActivity, String packageName, Bitmap myModuleIcon, OnModuleCreatedListener listener, final int instanceIdWhenResuscitating, long engineRefResuscitating) {
        activity = myActivity;
        this.onModuleCreatedListener=listener;
        this.moduleName = packageName;

        if(instanceIdWhenResuscitating!=-1) {
            audioRouteNativeRef=engineRefResuscitating;
            isExistingApp=isModuleConnected(audioRouteNativeRef);
            if(!isExistingApp) {
                isExistingApp=Audioroute.isAlive(audioRouteNativeRef); // Also tries to send the keepalive
            }
        } else if(audiorouteHostInstances.containsKey(moduleName))
        {
            Pair<AudiorouteEngineInfo, Integer> pair=audiorouteHostInstances.get(moduleName);
            audioRouteNativeRef=pair.first.instanceRef;
            audiorouteHostInstances.put(moduleName, new Pair<AudiorouteEngineInfo, Integer>(pair.first, pair.second+1));
            isExistingApp=Audioroute.isAlive(audioRouteNativeRef);
        }
        else {
            audioRouteNativeRef = Audioroute.getAudiorouteInstance(myActivity, 2, 2);
            audiorouteHostInstances.put(moduleName, new Pair<AudiorouteEngineInfo, Integer>(new AudiorouteEngineInfo(audioRouteNativeRef, Audioroute.getEngineData(audioRouteNativeRef)), 1));
            isExistingApp=false;
        }

        if (null != AudioModuleForegroundService.instance&&showForegroundServiceNotification) {
            AudioModuleForegroundService.instance.addListener(new AudioModuleForegroundService.OnServiceStartedListener() {
                @Override
                public void serviceStarted() {

                }

                @Override
                public void onDisconnect() {
                    disconnect();
                }
            });
            launchApp(!isExistingApp, instanceIdWhenResuscitating);
        } else {
            AudioModuleForegroundService.startService(activity, new AudioModuleForegroundService.OnServiceStartedListener() {
                @Override
                public void serviceStarted() {
                    launchApp(!isExistingApp, instanceIdWhenResuscitating);
                }

                @Override
                public void onDisconnect() {
                    disconnect();
                }
            }, myModuleIcon);
        }

    }
    Activity activity;

    void launchApp(boolean isFirstInstance, int instanceIdWhenResuscitating)
    {
        Intent launchIntent = activity.getPackageManager().getLaunchIntentForPackage(moduleName);
        if (launchIntent != null) {
            launchIntent.putExtra(AudiorouteActivityController.IntentGoBackToApp, activity.getPackageName());
            launchIntent.putExtra(AudiorouteActivityController.IntentRequestingNewInstance, true);
            launchIntent.putExtra(AudiorouteActivityController.IntentAudiorouteConnection, true);
            launchIntent.putExtra(AudiorouteActivityController.IntentIsFirstInstance, isFirstInstance);
            launchIntent.putExtra(AudiorouteActivityController.IntentProtocolVersion, AudioModule.getProtocolVersion());

            final Pair<AudiorouteEngineInfo, Integer> pair=audiorouteHostInstances.get(moduleName);

            if(null!=pair) {
                //try {
                    /*ParcelFileDescriptor fd = ParcelFileDescriptor.fromFd(pair.first.engineData);
                    Bundle bundle = launchIntent.getExtras();
                    //bundle.putParcelable(AudiorouteActivityController.IntentEngineData, fd);
                    Binder theBinder=new Binder();
                    if(Build.VERSION.SDK_INT>=18) {
                        bundle.putBinder(AudiorouteActivityController.IntentEngineData, theBinder);
                        launchIntent.putExtras(bundle);
                    }*/

                    launchIntent.putExtra(AudiorouteActivityController.IntentEngineData, new Messenger(new Handler() {

                        @Override
                        public void handleMessage(Message msg) {
                            Log.e(TAG, "handleMessage:: msg=" + msg);
                            switch (msg.what) {
                                case 1:
                                    try  {
                                        ParcelFileDescriptor fd = ParcelFileDescriptor.fromFd(pair.first.engineData);
                                        Message reply = Message.obtain(this, msg.what, 0, 0);
                                        Bundle b = new Bundle(1);
                                        b.putParcelable("fd", fd);
                                        reply.setData(b);
                                        try {
                                            msg.replyTo.send(reply);
                                        }
                                        catch(RemoteException e)
                                        {
                                            Log.d(TAG,"Remote exception while sending engine data");
                                        }
                                    }
                                    catch (IOException e1) {
                                        Log.d(TAG,"IOexception sending engine data "+e1.toString());
                                    }

                                    break;
                                default:
                                    break;
                            }
                            super.handleMessage(msg);
                        }

                    }));
                /*}
                catch(IOException ex)
                {
                    Log.d(TAG, "Error sending engine data: "+ex.toString());
                }*/
            }
            if(instanceIdWhenResuscitating!=-1)
                launchIntent.putExtra(AudiorouteActivityController.IntentForceInstanceResuscitating, instanceIdWhenResuscitating);
            //getAudioroute().setOnModuleConnectedOpenApp(myActivity.getPackageName(), moduleName);
            addIntentUniqueId(activity, launchIntent);
            activity.startActivity(launchIntent);//null pointer check in case package name was not found

            /*try {
                if (isFirstInstance) Audioroute.sendSharedMemoryFileDescriptor(audioRouteNativeRef);
            }
            catch(IllegalStateException e)
            {
                Log.d(TAG, "Error establishing connection to module, invalid state");
            }*/
        }
    }
    void disconnect()
    {

    }

    public long getNativeEngineReference()
    {
        return audioRouteNativeRef;
    }
    public interface ScanModulesListener
    {
        void onScanFinished(List<ModuleInfo> modules);
    }
    public class ModuleInfo
    {
        public String friendlyName;
        public String packagename;
        public int numericId; // unique AudioRoute identifier
        public int wantsMidi; // can receive Midi events (applies to effects and synths)
        public int isSynth; // is synth
        public int minVersion;
        public boolean supportsMultiInstance;
        public int effect;
        public int generator;
        public int sink;
    }
    private List <PackageInfo> getInstalledPackages(Context context) {
        return context.getPackageManager().getInstalledPackages(0);
    }
    String getAudiorouteScanFile(final Context context)
    {
        return context.getFilesDir()+"/audioroutescan.dat";
    }
    boolean isScanJsonCached(final Context context)
    {
        File theFile=new File(getAudiorouteScanFile(context));
        if(!theFile.exists()) return false;
        Date lastModified=new Date(theFile.lastModified());

        Calendar calendar = Calendar.getInstance();
        calendar.add( Calendar.HOUR,  -1 );
        return lastModified.after(calendar.getTime());
    }
    private String readFile(String pathname) throws IOException {

        File file = new File(pathname);
        StringBuilder fileContents = new StringBuilder((int)file.length());
        Scanner scanner = new Scanner(file);
        String lineSeparator = System.getProperty("line.separator");

        try {
            while(scanner.hasNextLine()) {
                fileContents.append(scanner.nextLine() + lineSeparator);
            }
            return fileContents.toString();
        } finally {
            scanner.close();
        }
    }
    public static String readFile(String file, Charset cs)
            throws IOException {
        // No real need to close the BufferedReader/InputStreamReader
        // as they're only wrapping the stream
        FileInputStream stream = new FileInputStream(file);
        try {
            Reader reader = new BufferedReader(new InputStreamReader(stream, cs));
            StringBuilder builder = new StringBuilder();
            char[] buffer = new char[8192];
            int read;
            while ((read = reader.read(buffer, 0, buffer.length)) > 0) {
                builder.append(buffer, 0, read);
            }
            return builder.toString();
        } finally {
            // Potential issue here: if this throws an IOException,
            // it will mask any others. Normally I'd use a utility
            // method which would log exceptions and swallow them
            stream.close();
        }
    }

    String getCachedScanJson(final Context context)
    {
        try {
            if(Build.VERSION.SDK_INT>=19)
                return readFile(getAudiorouteScanFile(context), StandardCharsets.UTF_8);
            return null;
        }
        catch(IOException e)
        {
            return null;
        }
    }
    void saveJson(final Context context, String content)
    {
        File theFile=new File(getAudiorouteScanFile(context));

        try {
            FileOutputStream outputStream = new FileOutputStream(theFile, false);
            outputStream.write(content.getBytes());
            outputStream.close();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    public boolean scanInstalledModules(final Activity context, final ScanModulesListener listener, boolean forceScan)
    {
        if(Build.VERSION.SDK_INT<23)
        {
            Log.i(TAG, "Audioroute requires API 24 or higher");
            return false;
        }
        /*ArrayList<ModuleInfo> modules=new ArrayList<>();
        ModuleInfo m=new ModuleInfo();
        m.friendlyName="Lowpass";
        m.packagename="com.ntrack.audioroute.lowpass";
        m.numericId=9000+1;
        m.wantsMidi=0;
        m.isSynth=0;
        modules.add(m);

        m=new ModuleInfo();
        m.friendlyName="Simple Synth";
        m.packagename="com.ntrack.audioroute.simplesynth";
        m.numericId=9000+2;
        m.wantsMidi=1;
        m.isSynth=1;
        modules.add(m);

        listener.onScanFinished(modules);*/
        activity=context;
        if(!forceScan&&isScanJsonCached(context))
        {
            String cached=getCachedScanJson(context);
            if(null!=cached) scanJson(cached, context, listener);
            return true;
        }

        new UrlTask().setListener(new UrlTask.UrlTaskListener() {
            @Override
            public void onResponse(String response) {
                if (null == response) {
                    Log.d(TAG, "Got a null response");
                    return;
                }
                if(scanJson(response, context, listener))
                {
                    saveJson(context, response);
                }
            }

            @Override
            public void onError(UrlTask.UrlError resultCode) {

            }
        }).setParams(getScanParams(context)).execute(getScanUrl(context));
        return true;
    }
    boolean scanJson(String response, final Context context, final ScanModulesListener listener)
    {
        List<PackageInfo> installedApps=getInstalledPackages(context);
        HashMap<String, ModuleInfo> modules=new HashMap<>();
        try {
            JSONObject json = new JSONObject(response);
            JSONArray apps=json.getJSONArray("apps");
            for(int i=0; i<apps.length(); ++i)
            {
                ModuleInfo m=new ModuleInfo();
                JSONObject mj=apps.getJSONObject(i);
                m.friendlyName=mj.getString("name");
                m.isSynth=mj.getInt("generator");
                m.numericId=i;
                m.packagename=mj.getString("package_name");
                m.wantsMidi=mj.getInt("accepts_midi");
                m.effect=mj.getInt("effect");
                m.generator=mj.getInt("generator");
                if(0==m.effect&&0==m.generator) continue;
                m.minVersion=mj.getInt("app_version");
                m.supportsMultiInstance=mj.optInt("supports_multiple_instances")!=0;
                m.sink=mj.optInt("sink");

                if(m.packagename.equals(context.getPackageName()))
                    continue;

                for(int f=0; f<installedApps.size(); ++f)
                {
                    PackageInfo pInfo=installedApps.get(f);
                    if(pInfo.packageName.equals(m.packagename)) {
                        if(pInfo.versionCode>=m.minVersion)
                        {
                            ModuleInfo existing=modules.get(pInfo.packageName);
                            if(null!=existing) {
                                if(m.minVersion<existing.minVersion)
                                    continue;
                            }
                            modules.put(pInfo.packageName, m);
                        }
                        break;
                    }
                }
            }
        }
        catch(JSONException e)
        {
            Log.d(TAG, "Error parsing json: " +response);
            return false;
        }
        listener.onScanFinished(new ArrayList<ModuleInfo>(modules.values()));
        return true;
    }
    public String getScanUrl(Context context)
    {
        if(BuildConfig.DEBUG)
        {
            return "https://ntrack.com/audioroute/ws/apps/scan.php";
        } else
            return "https://audioroute.ntrack.com/ws/apps/scan.php";
    }
    public Map<String, String> getScanParams(Context context)
    {
        Map<String, String> parameters = new HashMap<String, String>();
        parameters.put("android_sdkint", Integer.toString(Build.VERSION.SDK_INT));
        parameters.put("cpu_abi", Build.CPU_ABI);
        parameters.put("additional_abi", Build.CPU_ABI2);
        parameters.put("device_brand", Build.BRAND);
        parameters.put("device",Build.DEVICE);
        parameters.put("hardware",Build.HARDWARE);
        parameters.put("manufacturer",Build.MANUFACTURER);
        parameters.put("model",Build.MODEL);
        parameters.put("product",Build.PRODUCT);
        parameters.put("hostapp", context.getPackageName());
        return parameters;
    }
    int getLastIntentId(Activity activity)
    {
        SharedPreferences settings = activity.getSharedPreferences("audioroute", 0);
        int i = settings.getInt("lastintentid", 0);
        return i;
    }
    void setLastIntentId(Activity activity, int lastId)
    {
        SharedPreferences settings = activity.getSharedPreferences("audioroute", 0);
        SharedPreferences.Editor editor = settings.edit();
        editor.putInt("lastintentid", lastId);
        editor.commit();
    }
    void addIntentUniqueId(Activity activity, Intent intent)
    {
        int last=getLastIntentId(activity);
        last++;
        setLastIntentId(activity, last);
        intent.putExtra(AudiorouteActivityController.IntentIntentUniqueId, last);
    }
    public void showUserInterface(Activity myActivity, ModuleRuntimeInfo minfo)
    {
        Intent launchIntent = myActivity.getPackageManager().getLaunchIntentForPackage(minfo.packagename);
        if (launchIntent != null) {
            launchIntent.putExtra(AudiorouteActivityController.IntentRequestingNewInstance, false);
            launchIntent.putExtra(AudiorouteActivityController.IntentShowInterfaceForInstance, minfo.instanceId);
            launchIntent.putExtra(AudiorouteActivityController.IntentIsFirstInstance, false);
            addIntentUniqueId(myActivity, launchIntent);
            myActivity.startActivity(launchIntent);//null pointer check in case package name was not found
        }
    }
    public Drawable getPackageIcon(String packageName)
    {
        return AudiorouteActivityController.getPackageIcon(activity, packageName);
    }
}
