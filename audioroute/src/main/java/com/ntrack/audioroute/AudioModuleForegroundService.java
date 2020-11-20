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
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.IBinder;
import android.support.v4.app.NotificationCompat;
import android.util.Log;

import java.util.ArrayList;

public class AudioModuleForegroundService extends Service {

    public static AudioModuleForegroundService instance;

    public static String MAIN_ACTION = "com.ntrack.audioroute.foregroundservice.action.main";
    public static String PAUSE_ACTION = "com.ntrack.audioroute.foregroundservice.action.pause";
    public static String STARTFOREGROUND_ACTION = "com.ntrack.foregroundservice.action.startforeground";
    public static String STOPFOREGROUND_ACTION = "com.ntrack.foregroundservice.action.stopforeground";

    static int NOTIFICATION_ID_FOREGROUND_SERVICE = 101;

    private static final String LOG_TAG = "AudioRouteForeService";

    @Override
    public void onCreate() {
        super.onCreate();
    }

    static String productName;
    static Bitmap productBitmap;
    static Class mainActivityClass;
    static OnServiceStartedListener controller;

    public interface OnServiceStartedListener
    {
        void serviceStarted();
        void onDisconnect();
    }
    ArrayList<OnServiceStartedListener> listeners=new ArrayList<>();
    static void setProductName(OnServiceStartedListener _controller, String _productName, Bitmap _productBitmap)
    {
        productName=_productName;
        productBitmap=_productBitmap;
        controller=_controller;
    }
    static void setActivityClass(Class _activityClass)
    {
        mainActivityClass=_activityClass;
    }
    public static void startService(Activity activity, OnServiceStartedListener listener, Bitmap moduleIcon)
    {
        setActivityClass(activity.getClass());
        setProductName(listener, activity.getTitle().toString(), moduleIcon);
        Intent startIntent = new Intent(activity, AudioModuleForegroundService.class);
        startIntent.setAction(STARTFOREGROUND_ACTION);
        activity.startService(startIntent);
    }
    public static void stopService(Activity activity)
    {
        setActivityClass(activity.getClass());
        Intent startIntent = new Intent(activity, AudioModuleForegroundService.class);
        startIntent.setAction(STOPFOREGROUND_ACTION);
        activity.startService(startIntent);
        instance=null;
    }
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if(null==intent)
        {
            Log.e(LOG_TAG, "Foreground service null start command");
        }
        else if (intent.getAction().equals(STARTFOREGROUND_ACTION)) {
            instance=this;
            Log.i(LOG_TAG, "Received Start Foreground Intent ");
            if(null==mainActivityClass) return START_STICKY;
            Intent notificationIntent = new Intent(this, mainActivityClass);
            notificationIntent.setAction(MAIN_ACTION);
            notificationIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                    | Intent.FLAG_ACTIVITY_CLEAR_TASK);
            PendingIntent pendingIntent = PendingIntent.getActivity(this, 0,
                    notificationIntent, 0);

            Intent pauseIntent = new Intent(this, AudioModuleForegroundService.class);
            pauseIntent.setAction(PAUSE_ACTION);
            PendingIntent ppauseIntent = PendingIntent.getService(this, 0, pauseIntent, 0);

            Bitmap icon = productBitmap;

            String channelName="";
            String CHANNEL_ID="ChannelIdAudioroute";
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                NotificationChannel channel = new NotificationChannel(CHANNEL_ID, productName,
                        NotificationManager.IMPORTANCE_DEFAULT);
                channel.setShowBadge(false);
                channel.setSound(null, null);
                NotificationManager mNotificationManager = (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
                mNotificationManager.createNotificationChannel(channel);
                channelName=CHANNEL_ID;
            }

            Notification notification = new NotificationCompat.Builder(this, channelName)
                    .setContentTitle(productName)
                    .setTicker(productName)
                    .setContentText(Audioroute.ProductName)
                    .setSmallIcon(R.drawable.audioroute_logo)
                    .setLargeIcon(Bitmap.createScaledBitmap(icon, 128, 128, false))
                    .setContentIntent(pendingIntent)
                    .setOngoing(true)
                    .addAction(android.R.drawable.ic_media_pause, "Close", ppauseIntent).build();
            startForeground(NOTIFICATION_ID_FOREGROUND_SERVICE,
                    notification);

            if(null!=controller) {
                controller.serviceStarted();
                listeners.add(controller);
            }
            controller=null;
        } else if (intent.getAction().equals(PAUSE_ACTION)) {
            Log.i(LOG_TAG, "Clicked stop");
            stopForeground(true);
            stopSelf();
        } else if (intent.getAction().equals(STOPFOREGROUND_ACTION)) {
            Log.i(LOG_TAG, "Received Stop Foreground Intent");
            stopForeground(true);
            stopSelf();
        }
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        for(int i=0; i<listeners.size(); ++i)
            listeners.get(i).onDisconnect();
        if(controller!=null) {
            controller.onDisconnect();
            controller = null;
        }
        instance=null;
        Log.i(LOG_TAG, "In onDestroy");
    }

    @Override
    public IBinder onBind(Intent intent) {
        // Used only in case of bound services.
        return null;
    }
    public void addListener(OnServiceStartedListener l)
    {
        listeners.add(l);
    }
}