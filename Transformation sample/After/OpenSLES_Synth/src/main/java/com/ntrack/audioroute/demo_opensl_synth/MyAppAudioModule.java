package com.ntrack.audioroute.demo_opensl_synth;
import com.ntrack.audioroute.AudioModule;

public class MyAppAudioModule extends AudioModule {
    private long my_native_resources = 0;

    @Override
    protected boolean configure_module(String name, long handle) {
        my_native_resources = configureNativeComponents(handle, 2);

        return my_native_resources != 0;
    }

    @Override
    protected void release() {
        if (my_native_resources != 0) {
            release(my_native_resources);
            my_native_resources = 0;
        }
    }

    private native long configureNativeComponents(long handle, int channels);

    private native void release(long native_resources_pointer);
}