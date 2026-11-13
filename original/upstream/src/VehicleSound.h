#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <istream>
#include <vector>
#include <float.h>

namespace cars {
enum V8Sound {
    V8Start,V8Stop,V8Idle,V8Rev,V8Cruise,V8CoastSlow,V8CoastFast,
    V8SkidLow,V8SkidNormal,V8SkidHigh,V8ImpactMedium1,V8ImpactMedium2,
    V8ImpactMedium3,V8ImpactMedium4,V8ImpactHeavy1,V8ImpactHeavy2,
    V8ImpactHeavy3,V8ImpactHeavy4,V8Rollover1,V8Rollover2,
    V8First,V8Second,V8Third,V8RevShort,V8Turbo,V8Count
};
inline const char* v8File(unsigned clip) {
    static const char* names[]={"v8_start_loop1.wav","v8_stop1.wav","v8_idle_loop1.wav",
        "v8_firstgear_rev_loop1.wav","v8_fourth_cruise_loop2.wav","v8_throttle_off_slow_loop2.wav",
        "v8_throttle_off_fast_loop1.wav","v8_skid_lowfriction.wav","v8_skid_normalfriction.wav",
        "v8_skid_highfriction.wav","v8_impact_medium1.wav","v8_impact_medium2.wav",
        "v8_impact_medium3.wav","v8_impact_medium4.wav","v8_impact_heavy1.wav",
        "v8_impact_heavy2.wav","v8_impact_heavy3.wav","v8_impact_heavy4.wav",
        "v8_rollover1.wav","v8_rollover2.wav","v8_first.wav","v8_second.wav",
        "v8_third.wav","v8_rev_short_loop1.wav","v8_turbo_on_loop1.wav"};
    return clip<V8Count?names[clip]:"";
}
inline double soundClamp(double v,double lo,double hi){return !_finite(v)?lo:std::max(lo,std::min(hi,v));}
inline unsigned waveU32(const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
inline unsigned waveU16(const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8);}
struct VehicleWave {
    std::vector<float> samples;
    bool load(std::istream& input) {
        // Prepared format: mono, 44.1 kHz, PCM16. Bound every chunk before allocating.
        unsigned char head[12];if(!input.read(reinterpret_cast<char*>(head),12)||memcmp(head,"RIFF",4)||memcmp(head+8,"WAVE",4))return false;
        unsigned remaining=waveU32(head+4);if(remaining<4||remaining>4*1024*1024)return false;remaining-=4;
        bool format=false,data=false;std::vector<float> loaded;
        while(remaining>=8) {
            unsigned char chunk[8];if(!input.read(reinterpret_cast<char*>(chunk),8))return false;remaining-=8;
            unsigned bytes=waveU32(chunk+4),padded=bytes+(bytes&1);if(bytes>remaining||padded>remaining)return false;
            if(!memcmp(chunk,"fmt ",4)) {
                if(format||bytes<16||bytes>64)return false;unsigned char fmt[64];
                if(!input.read(reinterpret_cast<char*>(fmt),bytes)||waveU16(fmt)!=1||waveU16(fmt+2)!=1||waveU32(fmt+4)!=44100||waveU32(fmt+8)!=88200||waveU16(fmt+12)!=2||waveU16(fmt+14)!=16)return false;format=true;
            }else if(!memcmp(chunk,"data",4)) {
                if(!format||data||bytes<4||(bytes&1))return false;
                std::vector<unsigned char> pcm(bytes);if(!input.read(reinterpret_cast<char*>(&pcm[0]),bytes))return false;
                loaded.resize(bytes/2);for(unsigned n=0;n<bytes/2;++n){int sample=waveU16(&pcm[n*2]);if(sample>=32768)sample-=65536;loaded[n]=float(sample/32768.0);}data=true;
            }else input.ignore(bytes);
            if(bytes&1)input.ignore(1);if(!input)return false;remaining-=padded;
        }
        if(remaining||!format||!data)return false;samples.swap(loaded);return true;
    }
    double at(double& cursor,double rate,bool loop)const {
        if(samples.empty()||!_finite(cursor)||cursor<0||!_finite(rate)||rate<=0)return 0;
        if(cursor>=samples.size()){if(!loop)return 0;cursor=fmod(cursor,double(samples.size()));}
        const size_t a=static_cast<size_t>(cursor),b=a+1<samples.size()?a+1:loop?0:a;
        const double result=samples[a]+(samples[b]-samples[a])*(cursor-a);cursor+=rate;return result;
    }
};
struct VehicleSoundInput {
    bool engine,overturned;double speed,throttle,brake,slip,impact;
    VehicleSoundInput():engine(false),overturned(false),speed(0),throttle(0),brake(0),slip(0),impact(0){}
};
class VehicleSoundMixer {
    struct Voice {int clip;double cursor,gain;Voice():clip(-1),cursor(0),gain(0){}};
    Voice voices[8];double cursor[7],gain[7],pitch,volume,time,lastImpact,lastRollover;
    unsigned variant,events[V8Count];bool primed,previousEngine,previousOverturned;
    VehicleSoundInput state;
    void trigger(unsigned clip,double level) {
        unsigned slot=0;for(unsigned n=0;n<8;++n)if(voices[n].clip<0){slot=n;break;}
        voices[slot].clip=clip;voices[slot].cursor=0;voices[slot].gain=level;++events[clip];
    }
public:
    VehicleWave bank[V8Count];
    VehicleSoundMixer(){reset();}
    void reset(){for(unsigned n=0;n<8;++n)voices[n]=Voice();for(unsigned n=0;n<7;++n)cursor[n]=gain[n]=0;for(unsigned n=0;n<V8Count;++n)events[n]=0;pitch=1;volume=0;time=0;lastImpact=lastRollover=-10;variant=0;primed=false;previousEngine=previousOverturned=false;state=VehicleSoundInput();}
    void prime(const VehicleSoundInput& input){reset();previousEngine=input.engine;previousOverturned=input.overturned;primed=true;state=input;}
    unsigned triggers(unsigned clip)const{return clip<V8Count?events[clip]:0;}
    bool hasSound()const{if(state.engine||state.speed>1)return true;for(unsigned n=0;n<8;++n)if(voices[n].clip>=0)return true;for(unsigned n=0;n<7;++n)if(gain[n]>.001)return true;return false;}
    void setInput(const VehicleSoundInput& input) {
        if(!primed){prime(input);return;}
        if(input.engine&&!previousEngine)trigger(V8Start,.8);
        if(!input.engine&&previousEngine){for(unsigned n=0;n<8;++n)if(voices[n].clip==V8Start)voices[n]=Voice();trigger(V8Stop,.8);}
        if(input.overturned&&!previousOverturned&&time-lastRollover>1.5){trigger(V8Rollover1+(variant++%2),.7);lastRollover=time;}
        if(_finite(input.impact)&&input.impact>20&&time-lastImpact>.12){trigger((input.impact>65?V8ImpactHeavy1:V8ImpactMedium1)+(variant++%4),soundClamp(input.impact/100,.25,.85));lastImpact=time;}
        previousEngine=input.engine;previousOverturned=input.overturned;state=input;
        state.speed=soundClamp(state.speed,0,1000);state.throttle=soundClamp(state.throttle,0,1);state.brake=soundClamp(state.brake,0,1);state.slip=soundClamp(state.slip,0,1);
    }
    void render(short* out,unsigned count,double masterVolume) {
        const unsigned layers[]={V8Idle,V8Rev,V8Cruise,V8CoastSlow,V8CoastFast,V8SkidNormal,V8SkidHigh};
        const double motion=soundClamp(state.speed/180,0,1),moving=soundClamp(state.speed/25,0,1),load=state.throttle;
        const double coast=moving*(1-load),skid=state.slip*moving;
        double desired[]={state.engine?.58*(1-.65*moving):0,state.engine?.70*load:0,
            state.engine?.45*moving*(.25+.75*load):0,state.engine?.45*coast*(1-motion):0,
            state.engine?.45*coast*motion:0,.60*skid*(1-state.brake),.65*skid*state.brake};
        const double targetPitch=.85+.45*motion+.22*load;
        for(unsigned frame=0;frame<count;++frame) {
            time+=1.0/44100;pitch+=(targetPitch-pitch)/6615;volume+=(soundClamp(masterVolume,0,1)-volume)/441;
            double value=0,startDuck=1;
            for(unsigned n=0;n<8;++n)if(voices[n].clip==V8Start)startDuck=.2+.8*load;
            for(unsigned n=0;n<7;++n){gain[n]+=(desired[n]-gain[n])/3528;value+=bank[layers[n]].at(cursor[n],n<5?pitch:1,true)*gain[n]*(n<5?startDuck:1);}
            for(unsigned n=0;n<8;++n)if(voices[n].clip>=0) {
                Voice& voice=voices[n];const VehicleWave& sample=bank[voice.clip];
                value+=sample.at(voice.cursor,1,false)*voice.gain;
                if(voice.cursor>=sample.samples.size())voice=Voice();
            }
            value*=volume;out[frame]=static_cast<short>(soundClamp(value/(1+.25*fabs(value)),-.96,.96)*32767);
        }
    }
};
}
