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


import android.os.AsyncTask;
import android.util.Log;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.UnsupportedEncodingException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLEncoder;
import java.util.Map;

import javax.net.ssl.HttpsURLConnection;

class UrlTask extends AsyncTask<String, String, String> {
    final String TAG = "audioroute-urltask";

    public enum UrlError { MalformedUrl, IOError };
    public interface UrlTaskListener
    {
        void onResponse(String response);
        void onError(UrlError resultCode);
    }
    protected void onPreExecute() {
        super.onPreExecute();
    }
    UrlTaskListener listener;
    Map<String, String> params;

    UrlTask setListener(UrlTaskListener listener)
    {
        this.listener=listener;
        return this;
    }
    UrlTask setParams(Map<String, String> params)
    {
        this.params=params;
        return this;
    }
    String getPostDataString()
    {
        StringBuilder result = new StringBuilder();
        try {
            boolean first = true;
            for (Map.Entry<String, String> entry : params.entrySet()) {
                if (first)
                    first = false;
                else
                    result.append("&");

                result.append(URLEncoder.encode(entry.getKey(), "UTF-8"));
                result.append("=");
                result.append(URLEncoder.encode(entry.getValue(), "UTF-8"));
            }
        }
        catch (UnsupportedEncodingException e)
        {
            Log.d(TAG, "UnsupportedEncodingException");
        }
        return result.toString();
    }
    protected String doInBackground(String... params) {

        HttpsURLConnection connection = null;
        BufferedReader reader = null;

        try {
            URL url = new URL(params[0]);
            connection = (HttpsURLConnection) url.openConnection();
            connection.setAllowUserInteraction(false);
            connection.setInstanceFollowRedirects(true);
            connection.setRequestMethod("POST");
            connection.setDoInput(true);
            connection.setDoOutput(true);
            OutputStream os = connection.getOutputStream();
            BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(os, "UTF-8"));
            writer.write(getPostDataString());

            connection.connect();

            InputStream stream = connection.getInputStream();

            reader = new BufferedReader(new InputStreamReader(stream));

            StringBuffer buffer = new StringBuffer();
            String line = "";

            while ((line = reader.readLine()) != null) {
                buffer.append(line+"\n");
                //Log.d("Response: ", "> " + line);   //here u ll get whole response...... :-)
            }

            return buffer.toString();
        } catch (MalformedURLException e) {
            Log.d(TAG, "Malformed URL: "+e.getStackTrace());
            if(listener!=null) listener.onError(UrlError.MalformedUrl);
        } catch (IOException e) {
            Log.d(TAG, "IO exception: "+e.getMessage());
            if(listener!=null) listener.onError(UrlError.IOError);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
            try {
                if (reader != null) {
                    reader.close();
                }
            } catch (IOException e) {
                if(listener!=null) listener.onError(UrlError.IOError);
            }
        }
        return null;
    }

    @Override
    protected void onPostExecute(String result) {
        super.onPostExecute(result);

        if(listener!=null) listener.onResponse(result);
    }
}