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

package com.ntrack.audioroute;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.provider.MediaStore;
import android.util.Log;
import android.view.KeyEvent;

public class AudiorouteActivityController {
    static {
        System.loadLibrary("audiomodule");
    }

    public static final String IntentAudiorouteConnection="audiorouteconnection";
    public static final String IntentIntentUniqueId="audiorouteintentid";
    public static final String IntentRequestingNewInstance="requestingNewInstance";
    public static final String IntentGoBackToApp="gobacktoapp";
    public static final String IntentShowInterfaceForInstance="showinterfaceforinstance";
    public static final String IntentIsFirstInstance="request_new_instance";
    public static final String IntentForceInstanceResuscitating="force_instance_resuscitating";
    public static final String IntentEngineData="audiorouteenginedata";
    public static final String IntentProtocolVersion="audiorouteprotver";

    public static final String IntentReturningFromApp="audioroutereturningfromapp";
    public static final String IntentAudiorouteModuleIndex="audioroutemoduleindex";
    public static final String IntentAudiorouteModuleCreated="audioroutemodulecreated";
    public static final String IntentAudiorouteInstanceIndex="instanceindex";

    public interface Listener {
        void onRouteConnected();

        void onRouteDisconnected();

        AudioModule createAudioModule();
    }
    static AudiorouteActivityController instance;
    static public AudiorouteActivityController getInstance()
    {
        if(null!=instance) return instance;
        instance=new AudiorouteActivityController();
        return instance;
    }
    Listener listener;

    public void setListener(Listener l) {
        listener = l;
    }
    Activity activityParent;
    protected IAudiorouteService audioroute = null;

    public AudiorouteActivityController() {
    }
    public void setActivity(Activity activity)
    {
        activityParent=activity;
    }
    final String TAG="AudioRoute";
    AudioModule module;
    private String moduleLabel;

    public AudioModule getModule()
    {
        return module;
    }
    public void setModuleLabel(String label)
    {
        moduleLabel=label;
    }
    Bitmap moduleIcon;
    public void setModuleImage(Bitmap icon)
    {
        moduleIcon=icon;
    }
    protected boolean connectModule=true;
    public void setConnectModule(boolean connectModule)
    {
        this.connectModule=connectModule;
    }
    boolean connected=false;
    public boolean isConnected()
    {
        return connected;
    }
    public boolean isIntentConnecting=false;
    public boolean isConnecting()
    {
        return isIntentConnecting;
    }
    boolean checkIntentShouldConnect(Intent intent)
    {
        return intent.getBooleanExtra(IntentAudiorouteConnection, false);
    }
    int lastIntentId=-1;
    boolean intentIsAlreadyUsed(Intent intent)
    {
        final int intentid=intent.getIntExtra(IntentIntentUniqueId, 0);
        if(intentid<=lastIntentId)
            return true;
        return false;
    }
    public boolean onActivityCreated(Activity _activityParent, boolean forceConnect)
    {
        setActivity(_activityParent);
        Intent intent=_activityParent.getIntent();

        if(!intentIsAlreadyUsed(intent)) // Fix for the case when activity was destroyed while still connected to audioroute. To reproduce:
                            // set 'don't keep activities'
                            // kill all apps
                            // instantiate simplesynth from host
                            // it should play sound
                            // switch to simplesynth with icon in host
                            // the onCreate() method in the simplesynth activity has now the Intent that was originally used to create the activity and if this check is not performed
                            // would still kill the connection becaue 'isFirstInstance' would then be set
            doNewIntent(intent, forceConnect);
        /*boolean shouldConnect=forceConnect||checkIntentShouldConnect(intent);

        processExtraIntentParams(intent, false); // get extra parameters
        boolean doconnect=shouldConnect&&!isConnected();
        if(doconnect)
        {
            connect(activityParent);
        }
        return doconnect;*/
        return true;
    }
    void createMyModule()
    {
        module = listener.createAudioModule();
    }
    public void onConnected()
    {
        connected=true;
        Log.i(TAG, "Service connected.");
        PendingIntent pi = PendingIntent.getActivity(activityParent, 0, new Intent(activityParent, activityParent.getClass()), 0);
        /*Notification.Builder notificationBuilder = new Notification.Builder(activityParent);
        notificationBuilder.setLargeIcon(moduleIcon);
        Notification notification=notificationBuilder.setContentTitle(moduleLabel+"-Module").setContentIntent(pi).build();*/
        if(connectModule) {
                Log.i(TAG, "Creating module.");
                createMyModule();

                exchangeEngineDataAndConnect();
        }
    }
    void exchangeEngineDataAndConnect() {
        if (null != messenger) {
            try {
                Message msg = Message.obtain(null, 1, 101, 1001, null);
                msg.replyTo = new Messenger(new Handler() {

                    @Override
                    public void handleMessage(Message msg) {
                        Bundle bundle = msg.getData();
                        ParcelFileDescriptor fd = bundle.getParcelable("fd");
                        engineData = fd.getFd();
                        Log.d(TAG, "hosted handleMessage: arg1: " + Integer.toString(engineData) + " msg=" + msg);
                        super.handleMessage(msg);
                        doConnectModule(); // Now that we have fd we can launch the connection
                    }

                });
                messenger.send(msg);
            } catch (RemoteException e) {
                Log.d(TAG, "Error with messenger: " + e.getStackTrace());
            }
        }
    }
    void doConnectModule()
    {
        int moduleIndex=module.configure(moduleLabel, engineData, new AudioModule.IModuleDisconnectListener() {
            @Override
            public void onConnectionShutDown() {
                connected=false;
                if(forcedDisconnect) {
                    forcedDisconnect=false;
                    new Handler(Looper.getMainLooper()).post(new Runnable() {
                        @Override
                        public void run() {
                            connect(activityParent);
                        }
                    });
                }
                else {
                    if(showForegroundServiceNotification)
                        AudioModuleForegroundService.stopService(activityParent);
                }
            }
        });
        AcquireHostPackageName();
        if(listener!=null) listener.onRouteConnected();
        CheckGoBackToApp();
    }
    void AcquireHostPackageName()
    {
        if (gobacktoapp != null && gobacktoapp.length() > 0&&module!=null) {
            hostPackageName=gobacktoapp;
        }
    }
    void CheckGoBackToApp()
    {
        if(module==null||gobacktoapp ==null||gobacktoapp .length()==0) return;
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                {
                    //audioroute.switchBackToCallingAppIfAny(activityParent.getPackageName());
                    if(module==null) return;
                    if (gobacktoapp != null && gobacktoapp.length() > 0) {
                        if(forceInstanceIdOnResuscitate!=-1) {
                            module.setCurrentInstanceId(forceInstanceIdOnResuscitate);
                        } else if(requestingNewInstance) {
                            module.createNewInstanceIndex();
                        }
                        requestingNewInstance=false;
                        forceInstanceIdOnResuscitate=-1;
                        AcquireHostPackageName();
                        gobacktoapp = null;
                        doSwitchToHostApp(true);
                    }
                }
            }
        });
    }
    private String hostPackageName;

    private void doSwitchToHostApp(boolean onModuleCreated)
    {
        Intent launchIntent = activityParent.getPackageManager().getLaunchIntentForPackage(hostPackageName);
        if (launchIntent != null) {
            launchIntent.putExtra(IntentReturningFromApp, activityParent.getPackageName());
            launchIntent.putExtra(IntentProtocolVersion, AudioModule.getProtocolVersion());
            launchIntent.putExtra(IntentAudiorouteModuleIndex, module.getModuleIndex());
            launchIntent.putExtra(IntentAudiorouteInstanceIndex, module.getCurrentInstanceId());
            launchIntent.putExtra(IntentAudiorouteModuleCreated, onModuleCreated ? 1 : 0);
            activityParent.startActivity(launchIntent);
        }
    }
    public void switchToHostApp()
    {
        doSwitchToHostApp(false);
    }
    public Drawable getHostIcon()
    {
        if(null==hostPackageName) return null;
        return getPackageIcon(activityParent, hostPackageName);
    }
    static Drawable getPackageIcon(Activity activity, String packageName)
    {
        try {

            if(null==activity) return null;
            Drawable icon = activity.getPackageManager().getApplicationIcon(packageName);
            return icon;
        }
        catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
        return null;
    }
    public boolean onResume()
    {
        boolean ret=false;
        int oldonResumeConnect=onResumeConnect;
        onResumeConnect=0;
        if(oldonResumeConnect==1)
        {
            ret=true;
            connect(activityParent);
        } else {
            if (oldonResumeConnect != 99)
                CheckGoBackToApp();
        }
        return ret;
    }
    String gobacktoapp;
    boolean requestingNewInstance=false;
    boolean isFirstInstance=false;
    int onResumeConnect=0; // 0 == dont connect, 1 = connect, 99=dont even check go back
    boolean forcedDisconnect=false;
    public static void showMismatchProtocol(Context context)
    {
        if(null==context) return;
        new AlertDialog.Builder(context)
                .setTitle("Audioroute")
                .setMessage("The version of Audioroute in the host is not compatible with this app")
                .setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                }).show();
    }
    void doNewIntent(Intent intent, boolean force)
    {
        lastIntentId=intent.getIntExtra(IntentIntentUniqueId, 0);
        isIntentConnecting=intent.hasExtra(AudiorouteActivityController.IntentGoBackToApp);
        requestingNewInstance=intent.getBooleanExtra(IntentRequestingNewInstance, false);
        isFirstInstance=intent.getBooleanExtra(IntentIsFirstInstance, false);
        if(isFirstInstance) {
            int protocolVersion=intent.getIntExtra(IntentProtocolVersion, 0);
            if(protocolVersion!= AudioModule.getProtocolVersion())
            {
                onResumeConnect=0;
                forcedDisconnect=false;
                showMismatchProtocol(activityParent);
                return;
            }
        }
        if(isFirstInstance&&isConnected()) // Host reports that it is establishing the connection while we though we were already connected
        {
            forcedDisconnect=true;
            processExtraIntentParams(intent, false);
            onResumeConnect=99;
            disconnect();
        } else {
            if((force || checkIntentShouldConnect(intent)) && !isConnected())
                onResumeConnect=1;
            processExtraIntentParams(intent, false);
        }

    }
    public void onNewIntent(Intent intent)
    {
        doNewIntent(intent, false);
    }
    int forceInstanceIdOnResuscitate=-1;
    int engineData=-1;
    Messenger messenger;
    void processExtraIntentParams(Intent intent, final boolean doConnection)
    {
        if(gobacktoapp==null||gobacktoapp.isEmpty()) gobacktoapp=intent.getStringExtra(IntentGoBackToApp);
        forceInstanceIdOnResuscitate=intent.getIntExtra(IntentForceInstanceResuscitating, -1);
        engineData=-1;
        /*ParcelFileDescriptor fd = intent.getParcelableExtra(IntentEngineData);
        if(null!=fd)
            engineData = fd.getFd();*/

        messenger = intent.getParcelableExtra(IntentEngineData);

        int showinginterfaceForInstance=forceInstanceIdOnResuscitate;
        if(forceInstanceIdOnResuscitate==-1)
            showinginterfaceForInstance=intent.getIntExtra(IntentShowInterfaceForInstance, -1);

        if(-1!=showinginterfaceForInstance&&null!=getModule())
            getModule().setCurrentInstanceId(showinginterfaceForInstance);
    }
    public void connect(Activity activity) {
        if(!showForegroundServiceNotification||null!=AudioModuleForegroundService.instance)
        {
            if(showForegroundServiceNotification) {
                AudioModuleForegroundService.instance.addListener(new AudioModuleForegroundService.OnServiceStartedListener() {
                    @Override
                    public void serviceStarted() {
                    }

                    @Override
                    public void onDisconnect() {
                        disconnect();
                    }
                });
            }
            onConnected();
        }
        else {
            AudioModuleForegroundService.startService(activity, new AudioModuleForegroundService.OnServiceStartedListener() {
                @Override
                public void serviceStarted() {
                    onConnected();
                }
                @Override public void onDisconnect()
                {
                    disconnect();
                }
            }, moduleIcon);
        }
    }
    public void onForegroundServiceStarted()
    {
    }

    public void disconnect() {
        if(module!=null) module.stopAndRelease();
        if(null!=listener) listener.onRouteDisconnected();
        isIntentConnecting=false;
        connected=false;
    }
    boolean showForegroundServiceNotification=true;

    public void setShowForegroundServiceNotification(boolean showForegroundService) {
        showForegroundServiceNotification=showForegroundService;
    }
    public int getCurrentSampleRate()
    {
        if(null==module) return -1;
        return module.getCurrentSampleRate();
    }
    public int getCurrentFramesPerBuffer()
    {
        return module.getCurrentFramesPerBuffer();
    }
}
