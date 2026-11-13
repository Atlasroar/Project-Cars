#pragma once
#include <map>
#include <vector>
#include <cmath>
#include <utility>
namespace cars {
// Fixed natural deposits survive visual streaming; only a world reset clears them.
// Values belong to the plugin. No engine pointers, handles or STL allocators cross frames.
class OilDeposits {
    struct Point { double x,y,z; Point(double a,double b,double c):x(a),y(b),z(c){} };
    typedef std::pair<int,int> Cell;
    typedef std::map<Cell,std::vector<Point> > Grid;
    Grid grid; unsigned count;
    static bool valid(double n){return n==n && n>-10000000 && n<10000000;}
    static Cell cell(double x,double z){return Cell(int(std::floor(x/128.0)),int(std::floor(z/128.0)));}
public:
    OilDeposits():count(0){}
    void clear(){grid.clear();count=0;}
    unsigned size()const{return count;}
    bool add(double x,double y,double z){
        if(!valid(x)||!valid(y)||!valid(z))return false;
        Cell key=cell(x,z);Grid::iterator found=grid.find(key);
        if(count>=32768 && found==grid.end())return false;
        std::vector<Point>& points=grid[key];
        for(size_t i=0;i<points.size();++i)if(std::fabs(points[i].x-x)<0.1 && std::fabs(points[i].z-z)<0.1 && std::fabs(points[i].y-y)<1)return true;
        if(count>=32768)return false;
        points.push_back(Point(x,y,z));++count;return true;
    }
    bool canPump(double x,double y,double z)const{
        if(!valid(x)||!valid(y)||!valid(z))return false;
        Cell c=cell(x,z);
        for(int a=-1;a<=1;++a)for(int b=-1;b<=1;++b){
            Grid::const_iterator it=grid.find(Cell(c.first+a,c.second+b));if(it==grid.end())continue;
            for(size_t i=0;i<it->second.size();++i){const Point& p=it->second[i];double dx=p.x-x,dz=p.z-z;
                if(dx*dx+dz*dz<=120.0*120.0 && std::fabs(p.y-y)<=60.0)return true;
            }
        }
        return false;
    }
};
}
