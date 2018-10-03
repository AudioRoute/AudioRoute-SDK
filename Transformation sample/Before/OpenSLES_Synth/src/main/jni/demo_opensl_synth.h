#pragma once
#include <math.h>

#define LOGI(...) \
  __android_log_print(ANDROID_LOG_INFO, "audioroute", __VA_ARGS__)
#define LOGW(...) \
  __android_log_print(ANDROID_LOG_WARN, "audioroute", __VA_ARGS__)
#include <jni.h>
#include <unistd.h>

#define SIMPLESINTH_NATIVE extern "C" JNIEXPORT


#define RANGE 1

extern float notesfrequencies[128];

class voice
{
    float frequency;
    float period;
    long long phase;
    int on;
    float amplitude;
    float targetAmplitude;
    int note;
    int waveform;
    int nextNote;
    int samplingFrequency;
#define DECAY 0.995
#define DECAY_REPLACE_VOICE 0.95
#define AMPLITUDE_SILENCE 0.000000005
public:
    void reset()
    {
        on=0;
        phase=0;
        note=-1;
    }
    void turn_off()
    {
        on=3;
        //reset();
    }
    void turn_on(int note, int samplingfrequency, int waveform, float amplitude){
        if(on!=0) {
            if(on==99) return;
            on=99;
            nextNote=note;
            this->waveform=waveform;
            targetAmplitude=amplitude;
            samplingFrequency=samplingfrequency;
            return;
        }
        init(note, samplingfrequency, waveform);
        on=1;
        phase=0;
        this->targetAmplitude=amplitude;
        this->amplitude=0;
    }
    int currentNote()
    {
        return note;
    }
    voice()
    {
        reset();
    }
#define PHI 3.1415926535897932384626433832795
private:
    void init(int note, int samplingfrequency, int waveform)
    {
        reset();
        this->waveform=waveform;
        frequency=notesfrequencies[note];
        float discFreq=frequency/(2*samplingfrequency);
        period=1/discFreq;
        if(waveform==0) period/=2*PHI;
        this->note=note;
    }
public:
    float process()
    {
        if(on==0) return 0;
        else if(on==99) {
            amplitude = amplitude * DECAY_REPLACE_VOICE;
            if (amplitude < AMPLITUDE_SILENCE) {
                on=0;
                turn_on(nextNote, samplingFrequency, waveform, targetAmplitude);
            }
        } else if(on==3) {
            amplitude = amplitude * DECAY;
            if(amplitude<AMPLITUDE_SILENCE) {
                reset();
                return 0;
            }
        } else if(on==1) {
            amplitude += 0.00003;
            if(amplitude-targetAmplitude>-0.00005) {
                on=2;
            }
        }
        if(0==waveform) { // Pure tone
            return amplitude * sin(((float) phase++) / period);
        } else { // Saw tooth
            return amplitude * (((float) ((phase++)%((int)period))) / period);
        }
    }
    int is_on()
    {
        return on==2||on==1;
    }
};
#define NumVoices 10
struct simplesynth_instance{
    int waveform;  // RC lowpass filter coefficient, between 0 and RANGE.
    voice voices[NumVoices];
    int samplerate;
    int lastVoice;
private:
    inline void CookNoteFrequencies()
    {
        if(notesfrequencies[127]!=0.0f) return;

        float rootA = 440;
        notesfrequencies[57] = rootA;
        for (int i = 58; i < 127; i++)
        {
            notesfrequencies[i] = rootA * pow(2, (i - 57) / 12.0);
        }
        for (int i = 56; i >= 0; i--) {
            notesfrequencies[i] = rootA * pow(2, (i - 57) / 12.0);
        }
    }
public:
    simplesynth_instance() {
        initialize();
        CookNoteFrequencies();
    }
    void initialize()
    {
        waveform = 0;
        lastVoice=0;
    }
    float linToGain(float linval)
    {
        return powf(10.f, -1*(1-linval))*0.45f;
    }
    void process(float *bufferL, float *bufferR, int framesPerBuffer, int stride, int sample_rate, int32_t *events, int eventsNum)
    {
        int currentEvent=0;

        for(int i=0; i<framesPerBuffer; ++i) {
            for(; currentEvent<eventsNum; ++currentEvent)
            {
                int status=events[currentEvent]&0xf0;
                int note=(events[currentEvent]&0xff00)>>8;
                int velocity=(events[currentEvent]&0xff0000)>>16;
                if (status==0x90&&velocity>0) {
                    lastVoice++;
                    lastVoice=lastVoice%NumVoices;
                    voices[lastVoice].turn_on(note, sample_rate, waveform, linToGain(velocity/127.0f));
                }
                else if (status==0x80||(status==0x90&&velocity==0)) {
                    for(int v=0; v<NumVoices; ++v) {
                        if(voices[v].is_on()&&voices[v].currentNote()==note)
                            voices[v].turn_off();
                    }
                }
            }
            float sample=0;
            for(int voice=0; voice<NumVoices; ++voice)
                sample+=voices[voice].process();

            *bufferL=sample;
            bufferL+=stride;
            if(bufferR) {
                *bufferR = sample;
                bufferR += stride;
            }
        }
    }
};

const int MaxInstances = 24;
extern simplesynth_instance instanceData;
