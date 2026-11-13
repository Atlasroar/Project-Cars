#pragma once

#include <kenshi/util/hand.h>
#include <ogre/OgreVector3.h>
#include <kenshi/SensoryData.h>
#include <kenshi/Havok.h>

// The installed KenshiLib export mangles this forward declaration as a struct.
struct HavokCharacterMessage;

struct EdgePathNode
{
    hkVector4f mLeft; // 0x0 Member
    hkVector4f mRight; // 0x10 Member
    unsigned int face; // 0x20 Member
    unsigned int edge; // 0x24 Member
    float leftClearance; // 0x28 Member
    float rightClearance; // 0x2C Member
    float maxPoint; // 0x30 Member
    EdgePathNode(const EdgePathNode& __that);// public RVA = 0x14AD20
    EdgePathNode* _CONSTRUCTOR(const EdgePathNode& __that);// public RVA = 0x14AD20
    EdgePathNode();// public RVA = 0x14A990
    EdgePathNode* _CONSTRUCTOR();// public RVA = 0x14A990
    EdgePathNode& operator=(const EdgePathNode& __that);// public RVA = 0x14AE00
};

struct EdgeCache
{
    struct Edge
    {
        hkVector4f a; // 0x0 Member
        hkVector4f b; // 0x10 Member
        // no_addr void Edge(const struct EdgeCache::Edge & _a1);// public missing arg names
        Edge();// public RVA = 0x3B4150
        Edge* _CONSTRUCTOR();// public RVA = 0x3B4150
        // no_addr struct EdgeCache::Edge & operator=(const struct EdgeCache::Edge & _a1);// public missing arg names
    };
    hkVector4f centre; // 0x0 Member
    float radius; // 0x10 Member
    bool hasDoor; // 0x14 Member
    hkArray<EdgeCache::Edge, hkContainerHeapAllocator> edges; // 0x18 Member
    // no_addr void EdgeCache(const struct EdgeCache & _a1);// public missing arg names
    EdgeCache();// public RVA = 0x14D4C0
    EdgeCache* _CONSTRUCTOR();// public RVA = 0x14D4C0
    ~EdgeCache();// public RVA = 0x14D4E0
    void _DESTRUCTOR();// public RVA = 0x14D4E0
    // no_addr struct EdgeCache & operator=(const struct EdgeCache & _a1);// public missing arg names
    // no_addr void * __vecDelDtor(unsigned int _a1);// public missing arg names
};

class HavokCharacter
{
public:
    static void* operator new(unsigned __int64 s);// public RVA = 0x663EF0
    static void operator delete(void* p);// public RVA = 0x663F00
    enum CharacterState
    {
        IDLE,
        GOAL_REACHED,
        FOLLOWING_PATH,
        MANUAL
    };

    enum PathState
    {
        NONE,
        COMPLETE,
        INCOMPLETE,
        FAILED,
        WAITING,
        UPDATING,
        BROKEN
    };

    // no_addr void HavokCharacter(const class HavokCharacter & _a1);// public missing arg names
    HavokCharacter(const hand& h, const Ogre::Vector3& pos, float radius, float acceleration, SensoryData* sense);// public RVA = 0x146780
    HavokCharacter* _CONSTRUCTOR(const hand& h, const Ogre::Vector3& pos, float radius, float acceleration, SensoryData* sense);// public RVA = 0x146780
    ~HavokCharacter();// public RVA = 0x145AA0
    void _DESTRUCTOR();// public RVA = 0x145AA0
    void setHandle(const hand& h);// public RVA = 0x143EE0
    void setPosition(const Ogre::Vector3& p, unsigned int key);// public RVA = 0x145D30
    Ogre::Vector3 getPosition() const;// public RVA = 0x144040
    void setVelocity(const Ogre::Vector3& v);// public RVA = 0x143F30
    Ogre::Vector3 getVelocity() const;// public RVA = 0x144070
    Ogre::Vector3 getWaypointDirection() const;// public RVA = 0x1440A0
    Ogre::Vector3 getDirectionMoved() const;// public RVA = 0x1440D0
    void setWaterModifier(float m);// public RVA = 0x143F20
    void setManualMode(bool on);// public RVA = 0x143FE0
    void setDesiredSpeed(float speed);// public RVA = 0x143F90
    HavokCharacter::CharacterState getState() const;// public RVA = 0x663F10
    HavokCharacter::PathState getPathState() const;// public RVA = 0x663F20
    bool isOnNavmesh() const;// public RVA = 0x663F30
    const hand& getCollidedCharacter() const;// public RVA = 0x2ADC20
    Ogre::Vector3 getClosestPoint(const Ogre::Vector3& target, float max, Building* building);// public RVA = 0x144020
    void requestPath(const Ogre::Vector3& goal, int priority);// public RVA = 0x145BD0
    void clearPath();// public RVA = 0x1442E0
    int calculateFuturePosition(float d, hkVector4f& out, hkVector4f& dir, bool stopAtScreenEdge);// protected RVA = 0x147200
    Ogre::Vector3 calculateFuturePosition(float distance);// public RVA = 0x148C50
    void update(float time);// public RVA = 0x147EB0
    bool moveAlongPath(float time);// public RVA = 0x1489A0
    bool handleCollision();// public RVA = 0x145200
    void invalidate(int rid);// public RVA = 0x144180
    void updateVelocity(const hkVector4f& direction, float time);// public RVA = 0x147B80
    void _getFace(hkVector4f& p, unsigned int& key) const;// public RVA = 0x3B2650
    unsigned int _getFaceKey() const;// public RVA = 0x3B2670
    void setPath(HavokCharacterMessage* msg);// public RVA = 0x144BB0
    int calculatePathVector(hkVector4f& out, hkVector4f& slowTarget, bool pullthrough);// protected RVA = 0x146940
    int calculateAvoidanceVector(hkVector4f& out, hkSimdFloat32& penetration);// protected RVA = 0x1475D0
    int pullThroughVertex(int side, int index);// protected RVA = 0x1463F0
    int updateNextEdge();// protected RVA = 0x144210
    bool updateEdgeCache(const hkVector4f& centre, float range);// protected RVA = 0x144420
    int moveCollide(const hkVector4f& start, const hkVector4f& move, float radius, float& moved, hkVector4f& hitNormal) const;// protected RVA = 0x1444B0
    int edgeCollision(float radius, hkVector4f& out) const;// protected RVA = 0x145760
    void resolveProblems();// protected RVA = 0x146130
    bool atGoal() const;// protected RVA = 0x144320
    hkArray<EdgePathNode, hkContainerHeapAllocator> currentPath; // 0x0 Member
    unsigned int startingFace; // 0x10 Member
    unsigned int currentFace; // 0x14 Member
    int nextEdge; // 0x18 Member
    hkVector4f position; // 0x20 Member
    hkVector4f velocity; // 0x30 Member
    hkVector4f goal; // 0x40 Member
    hkVector4f waypointDirection; // 0x50 Member
    hkVector4f directionMoved; // 0x60 Member
    float radius; // 0x70 Member
    float waterModifier; // 0x74 Member
    float acceleration; // 0x78 Member
    float desiredSpeed; // 0x7C Member
    bool positionChanged; // 0x80 Member
    float timeOffPath; // 0x84 Member
    HavokCharacter::CharacterState characterState; // 0x88 Member
    HavokCharacter::CharacterState lastState; // 0x8C Member
    HavokCharacter::PathState pathState; // 0x90 Member
    hand handle; // 0x98 Member
    hand collided; // 0xB8 Member
    hkVector4f boundaryPos; // 0xE0 Member
    hkArray<EdgePathNode, hkContainerHeapAllocator> boundary; // 0xF0 Member
    SensoryData* sensoryData; // 0x100 Member
    EdgePathNode cachedEdge; // 0x110 Member
    EdgeCache edgeCache; // 0x150 Member
    char resolveFlag; // 0x180 Member
    // no_addr class HavokCharacter & operator=(const class HavokCharacter & _a1);// public missing arg names
    // no_addr void * __vecDelDtor(unsigned int _a1);// public missing arg names
};