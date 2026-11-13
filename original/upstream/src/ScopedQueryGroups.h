#pragma once
namespace cars {
// One synchronous query batch owns its exclusions and remembers which objects
// have already been inspected. Keys (including native pointers) never escape
// this scope; a fresh batch observes door-state and actor changes next step.
template<class Mask,class Key,unsigned Capacity=32>
class ScopedQueryBatch {
    Key visited[Capacity];unsigned count;
public:
    Mask exclusions;
    ScopedQueryBatch():count(0){}
    bool contains(Key key)const{for(unsigned n=0;n<count;++n)if(visited[n]==key)return true;return false;}
    void remember(Key key){if(count<Capacity && !contains(key))visited[count++]=key;}
    bool restore(){count=0;return exclusions.restore();}
private:ScopedQueryBatch(const ScopedQueryBatch&);ScopedQueryBatch& operator=(const ScopedQueryBatch&);
};
// Query-only exclusion. Preserve each shape's own group, deduplicate borrowed
// shapes and restore on every exit. No borrowed pointer survives this scope.
template<class Shape,class Access,unsigned Capacity=1024>
class ScopedQueryGroups {
    Shape shapes[Capacity];unsigned short groups[Capacity];unsigned count;
public:
    ScopedQueryGroups():count(0){}
    bool exclude(Shape shape) {
        if(!shape)return false;
        for(unsigned n=0;n<count;++n)if(shapes[n]==shape)return true;
        if(count==Capacity)return false;
        unsigned short group;if(!Access::read(shape,group)||group>31)return false;
        shapes[count]=shape;groups[count++]=group;
        unsigned short actual;
        return Access::write(shape,31)&&Access::read(shape,actual)&&actual==31;
    }
    unsigned size()const{return count;}
    bool restore() {
        bool ok=true;while(count){--count;unsigned short actual;
            if(!Access::write(shapes[count],groups[count])||!Access::read(shapes[count],actual)||actual!=groups[count])ok=false;
        }return ok;
    }
    ~ScopedQueryGroups(){restore();}
private:ScopedQueryGroups(const ScopedQueryGroups&);ScopedQueryGroups& operator=(const ScopedQueryGroups&);
};
}
