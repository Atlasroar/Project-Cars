#pragma once
#include "VehicleDynamics.h"
namespace cars {
// Owned geometry only. Native junction addresses are equality tokens during
// snapshot construction, never pointers to dereference from a route.
class RoadNetwork {
    std::map<unsigned __int64,unsigned> junctions;
    unsigned node(unsigned __int64 key) {
        if(key && junctions.count(key))return junctions[key];
        unsigned id=static_cast<unsigned>(links.size());links.push_back(std::vector<unsigned>());
        if(key)junctions[key]=id;return id;
    }
public:
    struct Road {unsigned ends[2];std::vector<RigPoint> points;std::vector<double> along;double length()const{return along.back();}};
    std::vector<Road> roads;std::vector<std::vector<unsigned> > links;unsigned pointCount;
    RoadNetwork():pointCount(0){}
    void clear(){junctions.clear();roads.clear();links.clear();pointCount=0;}
    bool add(unsigned __int64 a,unsigned __int64 b,const std::vector<RigPoint>& p) {
        if(p.size()<2||p.size()>4096||roads.size()>=32768||pointCount+p.size()>262144)return false;
        Road r;r.points=p;r.along.push_back(0);
        for(unsigned n=0;n<p.size();++n){if(!validPoint(p[n])||fabs(p[n].x)>1e7||fabs(p[n].y)>1e7||fabs(p[n].z)>1e7)return false;
            if(n)r.along.push_back(r.along.back()+(p[n]-p[n-1]).length());}
        if(r.length()<.01)return false;
        r.ends[0]=node(a);r.ends[1]=node(b);unsigned id=static_cast<unsigned>(roads.size());
        links[r.ends[0]].push_back(id);links[r.ends[1]].push_back(id);roads.push_back(r);pointCount+=static_cast<unsigned>(p.size());return true;
    }
    static RigPoint position(const Road& r,double at) {
        if(at<=0)return r.points.front();if(at>=r.length())return r.points.back();
        unsigned n=static_cast<unsigned>(std::upper_bound(r.along.begin(),r.along.end(),at)-r.along.begin());
        double length=r.along[n]-r.along[n-1];return r.points[n-1]+(r.points[n]-r.points[n-1])*((at-r.along[n-1])/length);
    }
};
inline double roadHorizontalDistance(const RigPoint& a,const RigPoint& b){RigPoint d=a-b;d.y=0;return d.length();}
// A bounded coarse search. Local VehicleRoute surveys still decide whether the
// car can traverse each approach, road section and departure.
class RoadJourney {
    struct Projection {double at,off;Projection():at(0),off(1e30){}};
    struct Entry {unsigned road;Projection p;bool operator<(const Entry& e)const{return p.off<e.p.off;}};
    struct Visit {double cost;int parent,road,seed,seedEnd;bool closed;Visit():cost(1e30),parent(-1),road(-1),seed(-1),seedEnd(0),closed(false){}};
    struct Pending {double cost;unsigned node;bool operator<(const Pending& p)const{return cost>p.cost;}};
    struct Choice {int road,node,seed,end;double at,off,cost;Choice():road(-1),node(-1),seed(-1),end(0),at(0),off(1e30),cost(1e30){}} choice;
    std::vector<Projection> exits;std::vector<Entry> entries;std::vector<Visit> visits;
    std::priority_queue<Pending> open;
    RigPoint origin,destination;Projection nearStart,nearEnd;
    unsigned scanRoad,scanSegment,phase,nextGuide;int pendingGuide;double direct,approach,limit;
    bool attempted,roadsUsed;std::vector<RigPoint> guides;
    void project(const RoadNetwork::Road& r,unsigned n,const RigPoint& p,Projection& out) {
        RigPoint d=r.points[n]-r.points[n-1],h=d;h.y=0;RigPoint from=p-r.points[n-1];from.y=0;
        double f=h.dot(h)>1e-8?bounded(from.dot(h)/h.dot(h),0,1):0;
        double off=(r.points[n-1]+d*f-p).length();
        if(off<out.off){out.off=off;out.at=r.along[n-1]+d.length()*f;}
    }
    void relax(unsigned node,double cost,int parent,int road,int seed,int seedEnd) {
        if(cost>=visits[node].cost||cost>limit)return;Visit& v=visits[node];
        v.cost=cost;v.parent=parent;v.road=road;v.seed=seed;v.seedEnd=seedEnd;Pending p={cost,node};open.push(p);
    }
    void consider(int road,int node,int seed,double at,double off,double cost,int end) {
        if(seed<0 || off>direct*.55 || cost>limit)return;
        // Avoid taking a tiny fragment of road only to drive most of the trip off-road.
        double onRoad=cost-2*(entries[seed].p.off+off);
        if(onRoad<std::min(150.0,direct*.2))return;
        // Prefer the reachable exit nearest the target, then the shorter journey.
        if(off<choice.off-1 || (fabs(off-choice.off)<=1 && cost<choice.cost)) {
            choice.road=road;choice.node=node;choice.seed=seed;choice.at=at;choice.off=off;choice.cost=cost;choice.end=end;
        }
    }
    bool append(const RigPoint& p) {
        if(guides.size()>=16384)return false;
        if(!guides.empty() && roadHorizontalDistance(guides.back(),p)<.1)return true;
        // Remove only collinear points; never shortcut a road bend or a crossing.
        if(guides.size()>1){RigPoint a=guides.back()-guides[guides.size()-2],b=p-guides.back();
            if(a.dot(b)>0 && cross(a,b).length()<.001*a.length()*b.length())guides.pop_back();}
        guides.push_back(p);return true;
    }
    bool piece(const RoadNetwork::Road& r,double from,double to) {
        if(!append(RoadNetwork::position(r,from)))return false;
        if(to>=from){for(unsigned n=0;n<r.points.size();++n)if(r.along[n]>from && r.along[n]<to && !append(r.points[n]))return false;}
        else {for(int n=static_cast<int>(r.points.size())-1;n>=0;--n)if(r.along[n]<from && r.along[n]>to && !append(r.points[n]))return false;}
        return append(RoadNetwork::position(r,to));
    }
    void finish(const RoadNetwork& net) {
        phase=3;if(choice.road<0){fallback();return;}guides.clear();append(origin);
        const Entry& seed=entries[choice.seed];const RoadNetwork::Road& entryRoad=net.roads[seed.road];
        bool good=true;
        if(choice.node<0)good=piece(entryRoad,seed.p.at,choice.at);
        else {
            std::vector<unsigned> path;int node=choice.node;
            while(visits[node].parent>=0 && path.size()<32768){path.push_back(static_cast<unsigned>(node));node=visits[node].parent;}
            if(path.size()>=32768){fallback();return;}
            good=piece(entryRoad,seed.p.at,visits[node].seedEnd?entryRoad.length():0);
            for(std::vector<unsigned>::reverse_iterator i=path.rbegin();good && i!=path.rend();++i){
                const Visit& v=visits[*i];const RoadNetwork::Road& r=net.roads[v.road];
                bool forward=r.ends[0]==static_cast<unsigned>(v.parent);good=piece(r,forward?0:r.length(),forward?r.length():0);
            }
            const RoadNetwork::Road& last=net.roads[choice.road];
            if(good)good=piece(last,choice.end?last.length():0,choice.at);
        }
        if(!good||!append(destination)){fallback();return;}roadsUsed=true;nextGuide=1;
        entries.clear();exits.clear();visits.clear();open=std::priority_queue<Pending>();
    }
public:
    RoadJourney():scanRoad(0),scanSegment(1),phase(3),nextGuide(1),pendingGuide(-1),direct(0),approach(0),limit(0),attempted(false),roadsUsed(false){}
    void reset(){*this=RoadJourney();}
    static bool eligible(const RigPoint& from,const RigPoint& to){return validPoint(from)&&validPoint(to)&&roadHorizontalDistance(from,to)>=1000;}
    bool started()const{return attempted;}
    bool ready()const{return attempted && phase==3;}
    bool usingRoads()const{return roadsUsed;}
    const std::vector<RigPoint>& route()const{return guides;}
    void begin(const RoadNetwork& net,const RigPoint& from,const RigPoint& to) {
        reset();attempted=true;origin=from;destination=to;direct=roadHorizontalDistance(from,to);
        if(!eligible(from,to)||net.roads.empty()){fallback();return;}
        approach=std::min(800.0,direct*.45);limit=direct*2.5+300;
        exits.resize(net.roads.size());phase=0;
    }
    void fallback(){attempted=true;phase=3;roadsUsed=false;pendingGuide=-1;guides.clear();entries.clear();exits.clear();visits.clear();open=std::priority_queue<Pending>();}
    // Budget counts segment projections, junction expansions and exit candidates.
    bool advance(const RoadNetwork& net,unsigned budget) {
        if(phase==3)return true;
        while(budget && phase==0){--budget;
            if(scanRoad>=net.roads.size()) {
                if(entries.empty()){fallback();return true;}
                visits.resize(net.links.size());
                for(unsigned e=0;e<entries.size();++e){const Entry& seed=entries[e];const RoadNetwork::Road& r=net.roads[seed.road];
                    relax(r.ends[0],2*seed.p.off+seed.p.at,-1,-1,e,0);
                    relax(r.ends[1],2*seed.p.off+r.length()-seed.p.at,-1,-1,e,1);}
                phase=1;scanRoad=0;break;
            }
            const RoadNetwork::Road& r=net.roads[scanRoad];
            project(r,scanSegment,origin,nearStart);project(r,scanSegment,destination,nearEnd);
            if(++scanSegment==r.points.size()){
                exits[scanRoad]=nearEnd;
                if(nearStart.off<=approach){Entry e;e.road=scanRoad;e.p=nearStart;entries.push_back(e);std::sort(entries.begin(),entries.end());if(entries.size()>8)entries.pop_back();}
                ++scanRoad;scanSegment=1;nearStart=Projection();nearEnd=Projection();
            }
        }
        while(budget && phase==1){--budget;
            if(open.empty()){phase=2;break;}Pending p=open.top();open.pop();Visit& v=visits[p.node];
            if(v.closed||p.cost!=v.cost)continue;v.closed=true;
            for(unsigned n=0;n<net.links[p.node].size();++n){unsigned id=net.links[p.node][n];const RoadNetwork::Road& r=net.roads[id];
                unsigned to=r.ends[0]==p.node?r.ends[1]:r.ends[0];relax(to,v.cost+r.length(),p.node,id,v.seed,v.seedEnd);}
        }
        while(budget && phase==2){--budget;
            if(scanRoad>=net.roads.size()){finish(net);break;}
            const RoadNetwork::Road& r=net.roads[scanRoad];const Projection& exit=exits[scanRoad];
            for(unsigned end=0;end<2;++end){const Visit& v=visits[r.ends[end]];
                consider(scanRoad,r.ends[end],v.seed,exit.at,exit.off,v.cost+(end?r.length()-exit.at:exit.at)+2*exit.off,end);}
            for(unsigned e=0;e<entries.size();++e)if(entries[e].road==scanRoad)
                consider(scanRoad,-1,e,exit.at,exit.off,2*entries[e].p.off+fabs(entries[e].p.at-exit.at)+2*exit.off,0);
            ++scanRoad;
        }
        return phase==3;
    }
    // Guides advance only when the local planner accepts the entire segment.
    // The controller can stop within its arrival tolerance of that endpoint.
    void acceptTarget(){if(pendingGuide>=0)nextGuide=static_cast<unsigned>(pendingGuide)+1;pendingGuide=-1;}
    RigPoint target(const RigPoint& from,double maximum,bool& final) {
        RigPoint goal=destination;final=true;pendingGuide=-1;
        if(roadsUsed){while(nextGuide<guides.size() && roadHorizontalDistance(from,guides[nextGuide])<1)++nextGuide;
            if(nextGuide<guides.size()){goal=guides[nextGuide];pendingGuide=nextGuide;final=nextGuide+1==guides.size();}}
        RigPoint delta=goal-from;double length=roadHorizontalDistance(goal,from);
        if(length>maximum){goal=from+delta*(maximum/length);final=false;pendingGuide=-1;}return goal;
    }
};
}
