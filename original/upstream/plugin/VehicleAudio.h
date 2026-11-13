#pragma once
#include <mmsystem.h>
#include <fstream>
#include "../src/VehicleSound.h"
namespace cars {
// Prepared recordings are plugin-owned. WinMM is polled on the UI thread;
// no file access or game pointers are used by the sample renderer.
class VehicleAudio {
    enum {BufferCount=4,BufferSamples=2048};
    HWAVEOUT output;WAVEHDR headers[BufferCount];short samples[BufferCount][BufferSamples];
    VehicleSoundMixer mixer;std::string vehicle;
    double phase,filtered,gain;unsigned noise;bool failed,bankReady;
    void closeDevice(){if(!output)return;waveOutReset(output);for(unsigned n=0;n<BufferCount;++n)waveOutUnprepareHeader(output,&headers[n],sizeof(WAVEHDR));waveOutClose(output);output=0;ZeroMemory(headers,sizeof(headers));gain=0;}
    void fallback(short* out,const VehicleSoundInput& input,double volume) {
        for(unsigned k=0;k<BufferSamples;++k) {
            double target=input.engine?.15+.08*input.throttle:0;gain+=(target-gain)*.0005;
            phase+=6.283185307179586*(28+input.speed*.35+input.throttle*18)/44100;if(phase>6.283185307179586)phase-=6.283185307179586;
            noise=1664525*noise+1013904223;double random=(int(noise>>16)-32768)/32768.0;filtered+=(random-filtered)*.06;
            double value=gain*(sin(phase)+.35*sin(2*phase)+.18*sin(4*phase)+filtered*.7)+filtered*input.speed*.00025;
            out[k]=short(soundClamp(value*soundClamp(volume,0,1),-.85,.85)*32767);
        }
    }
public:
    VehicleAudio():output(0),phase(0),filtered(0),gain(0),noise(5489),failed(false),bankReady(false){ZeroMemory(headers,sizeof(headers));}
    bool initialize() {
        static int moduleAnchor;HMODULE module=0;wchar_t path[32768];
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&moduleAnchor),&module))return false;
        DWORD length=GetModuleFileNameW(module,path,32768);if(!length||length>=32768)return false;
        std::wstring folder(path,length);size_t slash=folder.find_last_of(L"\\/");if(slash==std::wstring::npos)return false;
        folder=folder.substr(0,slash+1)+L"audio\\v8\\";
        bankReady=true;
        for(unsigned n=0;n<V8Count;++n){std::string name(v8File(n));std::wstring file=folder+std::wstring(name.begin(),name.end());std::ifstream input(file.c_str(),std::ios::binary);if(!input||!mixer.bank[n].load(input))bankReady=false;}
        return bankReady;
    }
    void stop(){closeDevice();vehicle.clear();mixer.reset();}
    void update(const std::string& key,const VehicleSoundInput& input,double volume) {
        if(failed)return;
        if(key!=vehicle){stop();vehicle=key;mixer.prime(input);}
        mixer.setInput(input);
        if(bankReady?!mixer.hasSound():!input.engine&&input.speed<1){
            // Let the last queued shutdown/impact samples reach the device.
            for(unsigned n=0;n<BufferCount;++n)if(headers[n].dwFlags&WHDR_INQUEUE)return;
            closeDevice();return;
        }
        if(!output) {
            WAVEFORMATEX format;ZeroMemory(&format,sizeof(format));format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;format.nSamplesPerSec=44100;format.wBitsPerSample=16;format.nBlockAlign=2;format.nAvgBytesPerSec=88200;
            if(waveOutOpen(&output,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){output=0;failed=true;return;}
            for(unsigned n=0;n<BufferCount;++n){headers[n].lpData=reinterpret_cast<LPSTR>(samples[n]);headers[n].dwBufferLength=sizeof(samples[n]);if(waveOutPrepareHeader(output,&headers[n],sizeof(WAVEHDR))!=MMSYSERR_NOERROR){stop();failed=true;return;}}
        }
        for(unsigned n=0;n<BufferCount;++n)if(!(headers[n].dwFlags&WHDR_INQUEUE)) {
            if(bankReady)mixer.render(samples[n],BufferSamples,volume);else fallback(samples[n],input,volume);
            if(waveOutWrite(output,&headers[n],sizeof(WAVEHDR))!=MMSYSERR_NOERROR){stop();failed=true;return;}
        }
    }
};
}
