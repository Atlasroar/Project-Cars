// SPDX-License-Identifier: GPL-3.0-only
#include "lifecycle.h"
#include "text.h"

#include <cstring>
#include <iostream>
#include <limits>
#include <process.h>
#include <sstream>
#include <vector>

namespace projectcars
{
    struct LifecycleTestAccess
    {
        static void ExhaustEpoch(LifecycleProbe& probe)
        {
            AcquireSRWLockExclusive(&probe.lock_);
            probe.state_.epoch = (std::numeric_limits<LifecycleEpoch>::max)();
            ReleaseSRWLockExclusive(&probe.lock_);
        }
    };
}

namespace
{
    using namespace projectcars;
    unsigned int assertions = 0;

    void Check(bool condition, const char* expression, int line)
    {
        ++assertions;
        if (!condition)
        {
            std::ostringstream message;
            message << "lifecycle test line " << line << ": " << expression;
            throw std::runtime_error(message.str());
        }
    }
#define CHECK(expression) Check((expression), #expression, __LINE__)

    class Handle
    {
    public:
        explicit Handle(HANDLE handle) : handle_(handle)
        {
            if (handle == NULL || handle == INVALID_HANDLE_VALUE)
                throw std::runtime_error("cannot create lifecycle fixture handle");
        }
        ~Handle() { CloseHandle(handle_); }
        HANDLE Get() const { return handle_; }
    private:
        Handle(const Handle&);
        Handle& operator=(const Handle&);
        HANDLE handle_;
    };

    class TestLock
    {
    public:
        explicit TestLock(SRWLOCK& lock) : lock_(lock) { AcquireSRWLockExclusive(&lock_); }
        ~TestLock() { ReleaseSRWLockExclusive(&lock_); }
    private:
        TestLock(const TestLock&);
        TestLock& operator=(const TestLock&);
        SRWLOCK& lock_;
    };

    void Wait(HANDLE handle)
    {
        if (WaitForSingleObject(handle, 10000) != WAIT_OBJECT_0)
            throw std::runtime_error("lifecycle fixture wait failed or timed out");
    }

    void Signal(HANDLE handle)
    {
        if (!SetEvent(handle))
            throw std::runtime_error("lifecycle fixture SetEvent failed");
    }

    struct Fixture;
    Fixture* current = NULL;
    void __cdecl TickOriginal(GameWorld* world, float time);
    void __cdecl ResetOriginal(GameWorld* world);
    void __cdecl ClearOriginal(GameWorld* world);
    void __cdecl ChainedTickOriginal(GameWorld* world, float time);

    struct Fixture
    {
        Fixture() : probe(&Diagnostic, NULL), tickCalls(0), resetCalls(0), clearCalls(0),
                    chainCalls(0), nestedReset(false), nestedClear(false), tickInsideClear(false),
                    throwingHook(LIFECYCLE_NO_HOOK), throwSink(false), blockTick(false),
                    blockReset(false), entered(CreateEventW(NULL, TRUE, FALSE, NULL)),
                    release(CreateEventW(NULL, TRUE, FALSE, NULL)), lastWorld(NULL), lastTime(0),
                    insideResult(LIFECYCLE_REQUEST_INVALID)
        {
            InitializeSRWLock(&logLock);
            current = this;
        }

        static void Diagnostic(void*, const LifecycleDiagnostic& diagnostic)
        {
            Fixture& self = *current;
            self.probe.Snapshot(); // Reentry proves no probe lock is held by the emitter.
            if (self.throwSink)
                throw std::runtime_error("fixture diagnostic failure");
            TestLock lock(self.logLock);
            self.diagnostics.push_back(diagnostic.kind);
        }

        GameWorld* World() { return reinterpret_cast<GameWorld*>(&markerA); }
        GameWorld* OtherWorld() { return reinterpret_cast<GameWorld*>(&markerB); }
        void Trace(int event)
        {
            TestLock lock(logLock);
            trace.push_back(event);
        }
        size_t DiagnosticCount(LifecycleDiagnosticKind kind) const
        {
            size_t count = 0;
            for (size_t i = 0; i < diagnostics.size(); ++i)
                if (diagnostics[i] == kind)
                    ++count;
            return count;
        }

        LifecycleProbe probe;
        volatile LONG tickCalls;
        volatile LONG resetCalls;
        volatile LONG clearCalls;
        volatile LONG chainCalls;
        bool nestedReset;
        bool nestedClear;
        bool tickInsideClear;
        LifecycleHook throwingHook;
        bool throwSink;
        bool blockTick;
        bool blockReset;
        Handle entered;
        Handle release;
        GameWorld* lastWorld;
        float lastTime;
        LifecycleSnapshot atTick;
        LifecycleSnapshot atReset;
        LifecycleSnapshot atClear;
        LifecycleRequestResult insideResult;
        char markerA;
        char markerB;
        SRWLOCK logLock;
        std::vector<int> trace;
        std::vector<LifecycleDiagnosticKind> diagnostics;
    };

    void __cdecl TickOriginal(GameWorld* world, float time)
    {
        Fixture& self = *current;
        InterlockedIncrement(&self.tickCalls);
        self.lastWorld = world;
        self.lastTime = time;
        self.atTick = self.probe.Snapshot();
        self.Trace(1);
        if (self.blockTick)
        {
            Signal(self.entered.Get());
            Wait(self.release.Get());
        }
        if (self.nestedReset)
        {
            self.nestedReset = false;
            self.probe.Reset(world);
        }
        if (self.throwingHook == LIFECYCLE_TICK)
            throw 731;
        self.Trace(2);
    }

    void __cdecl ResetOriginal(GameWorld* world)
    {
        Fixture& self = *current;
        InterlockedIncrement(&self.resetCalls);
        self.atReset = self.probe.Snapshot();
        self.Trace(3);
        self.insideResult = self.probe.EnqueueObservation(self.atReset.epoch, 700);
        if (self.blockReset)
        {
            Signal(self.entered.Get());
            Wait(self.release.Get());
        }
        if (self.nestedClear)
        {
            self.nestedClear = false;
            self.probe.Clear(world);
        }
        if (self.throwingHook == LIFECYCLE_RESET)
            throw 731;
        self.Trace(4);
    }

    void __cdecl ClearOriginal(GameWorld* world)
    {
        Fixture& self = *current;
        InterlockedIncrement(&self.clearCalls);
        self.atClear = self.probe.Snapshot();
        self.Trace(5);
        if (self.tickInsideClear)
        {
            self.tickInsideClear = false;
            self.probe.Tick(world, 0.5f);
        }
        if (self.throwingHook == LIFECYCLE_CLEAR)
            throw 731;
        self.Trace(6);
    }

    void __cdecl ChainedTickOriginal(GameWorld* world, float time)
    {
        InterlockedIncrement(&current->chainCalls);
        TickOriginal(world, time);
    }

    struct FakeInstaller : LifecycleInstaller
    {
        explicit FakeInstaller(Fixture& fixture)
            : self(fixture), prepareCalls(0), addCalls(0), failAt(LIFECYCLE_NO_HOOK),
              missingAt(LIFECYCLE_NO_HOOK), throwAt(LIFECYCLE_NO_HOOK),
              throwPrepare(false), immediateCallbacks(false), publishOnFail(true),
              nonStandardThrow(false), chain(false) {}

        virtual void Prepare()
        {
            ++prepareCalls;
            if (throwPrepare)
                throw std::runtime_error("fixture target resolution/pin failure");
        }
        bool Result(LifecycleHook hook)
        {
            ++addCalls;
            if (immediateCallbacks && hook != missingAt && (hook != failAt || publishOnFail))
            {
                if (hook == LIFECYCLE_TICK)
                    self.probe.Tick(self.World(), 2.5f);
                else if (hook == LIFECYCLE_RESET)
                    self.probe.Reset(self.World());
                else
                    self.probe.Clear(self.World());
            }
            if (hook == throwAt)
            {
                if (nonStandardThrow)
                    throw 411;
                throw std::runtime_error("fixture AddHook threw after publication");
            }
            return hook != failAt;
        }
        virtual bool AddTick(WorldTickOriginal* original)
        {
            if (missingAt != LIFECYCLE_TICK && (failAt != LIFECYCLE_TICK || publishOnFail))
                *original = chain ? &ChainedTickOriginal : &TickOriginal;
            return Result(LIFECYCLE_TICK);
        }
        virtual bool AddReset(WorldBoundaryOriginal* original)
        {
            if (missingAt != LIFECYCLE_RESET && (failAt != LIFECYCLE_RESET || publishOnFail))
                *original = &ResetOriginal;
            return Result(LIFECYCLE_RESET);
        }
        virtual bool AddClear(WorldBoundaryOriginal* original)
        {
            if (missingAt != LIFECYCLE_CLEAR && (failAt != LIFECYCLE_CLEAR || publishOnFail))
                *original = &ClearOriginal;
            return Result(LIFECYCLE_CLEAR);
        }
        Fixture& self;
        unsigned int prepareCalls;
        unsigned int addCalls;
        LifecycleHook failAt;
        LifecycleHook missingAt;
        LifecycleHook throwAt;
        bool throwPrepare;
        bool immediateCallbacks;
        bool publishOnFail;
        bool nonStandardThrow;
        bool chain;
    private:
        FakeInstaller(const FakeInstaller&);
        FakeInstaller& operator=(const FakeInstaller&);
    };

    struct Worker
    {
        Worker(Fixture& fixture, LifecycleHook callback)
            : self(fixture), hook(callback), enqueue(false), epoch(0), accepted(0),
              full(0), stale(0), resetting(0), failed(false) {}
        static unsigned __stdcall Run(void* context)
        {
            Worker& worker = *static_cast<Worker*>(context);
            try
            {
                if (worker.enqueue)
                {
                    for (unsigned int i = 1; i <= 128; ++i)
                    {
                        const LifecycleRequestResult result =
                            worker.self.probe.EnqueueObservation(worker.epoch, i);
                        if (result == LIFECYCLE_REQUEST_ACCEPTED) ++worker.accepted;
                        else if (result == LIFECYCLE_REQUEST_FULL) ++worker.full;
                        else if (result == LIFECYCLE_REQUEST_STALE) ++worker.stale;
                        else if (result == LIFECYCLE_REQUEST_RESETTING) ++worker.resetting;
                        else throw std::runtime_error("unexpected concurrent enqueue rejection");
                    }
                }
                else if (worker.hook == LIFECYCLE_TICK)
                    worker.self.probe.Tick(worker.self.World(), 1.25f);
                else if (worker.hook == LIFECYCLE_RESET)
                    worker.self.probe.Reset(worker.self.World());
                else
                    worker.self.probe.Clear(worker.self.World());
            }
            catch (...)
            {
                worker.failed = true;
            }
            return 0;
        }
        HANDLE Start()
        {
            return reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, &Run, this, 0, NULL));
        }
        Fixture& self;
        LifecycleHook hook;
        bool enqueue;
        LifecycleEpoch epoch;
        unsigned int accepted;
        unsigned int full;
        unsigned int stale;
        unsigned int resetting;
        bool failed;
    private:
        Worker(const Worker&);
        Worker& operator=(const Worker&);
    };

    class InFlightInstaller : public FakeInstaller
    {
    public:
        explicit InFlightInstaller(Fixture& fixture)
            : FakeInstaller(fixture), reset_(fixture, LIFECYCLE_RESET), thread_(NULL) {}
        ~InFlightInstaller()
        {
            if (thread_ != NULL)
            {
                SetEvent(self.release.Get());
                WaitForSingleObject(thread_, 10000);
                CloseHandle(thread_);
            }
        }
        virtual bool AddReset(WorldBoundaryOriginal* original)
        {
            ++addCalls;
            *original = &ResetOriginal;
            self.blockReset = true;
            thread_ = reset_.Start();
            if (thread_ == NULL)
                throw std::runtime_error("cannot start in-flight installation fixture");
            Wait(self.entered.Get());
            return true;
        }
        void Finish()
        {
            Signal(self.release.Get());
            Wait(thread_);
            CHECK(!reset_.failed);
            CHECK(CloseHandle(thread_) != FALSE);
            thread_ = NULL;
        }
    private:
        InFlightInstaller(const InFlightInstaller&);
        InFlightInstaller& operator=(const InFlightInstaller&);
        Worker reset_;
        HANDLE thread_;
    };

    void DisabledAndInstallation()
    {
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, false) == LIFECYCLE_DISABLED);
            CHECK(installer.prepareCalls == 0 && installer.addCalls == 0);
            CHECK(f.probe.EnqueueObservation(1, 1) == LIFECYCLE_REQUEST_DISABLED);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_DISABLED);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_DISABLED);
            CHECK(installer.addCalls == 0);
            CHECK(f.DiagnosticCount(LIFECYCLE_DUPLICATE_DIAGNOSTIC) == 1);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            installer.immediateCallbacks = true;
            installer.chain = true;
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            CHECK(installer.prepareCalls == 1 && installer.addCalls == 3);
            CHECK(f.tickCalls == 1 && f.resetCalls == 1 && f.clearCalls == 1 && f.chainCalls == 1);
            CHECK(f.atTick.installation == LIFECYCLE_INSTALLING);
            CHECK(f.atReset.boundaryDepth == 1 && f.atClear.boundaryDepth == 1);
            LifecycleSnapshot state = f.probe.Snapshot();
            CHECK(state.attemptedMask == 7 && state.successMask == 7 && state.originalMask == 7);
            CHECK(state.epoch == 3 && state.boundaryEntries == 2 && state.tickEntries == 1);
            CHECK(state.callbackOwners[0] == GetCurrentThreadId() &&
                  state.callbackOwners[1] == GetCurrentThreadId() &&
                  state.callbackOwners[2] == GetCurrentThreadId());
            CHECK(f.probe.EnqueueObservation(state.epoch, 1) == LIFECYCLE_REQUEST_UNOBSERVED);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            CHECK(installer.addCalls == 3);
            const unsigned int bits = 0x80000000;
            float negativeZero;
            std::memcpy(&negativeZero, &bits, sizeof(bits));
            f.probe.Tick(f.World(), negativeZero);
            unsigned int actualBits = 0;
            std::memcpy(&actualBits, &f.lastTime, sizeof(actualBits));
            CHECK(actualBits == bits && f.lastWorld == f.World());
            CHECK(f.tickCalls == 2 && f.chainCalls == 2);
            CHECK(f.probe.Snapshot().callbackOwners[LIFECYCLE_TICK] == GetCurrentThreadId());
            const unsigned int argumentBits[] = {
                0, 0x80000000, 0x7fc12345, 0x7f800000, 0xff800000, 0xc2f70000
            };
            for (size_t i = 0; i < sizeof(argumentBits) / sizeof(argumentBits[0]); ++i)
            {
                float argument;
                std::memcpy(&argument, &argumentBits[i], sizeof(argument));
                f.probe.Tick(f.World(), argument);
                std::memcpy(&actualBits, &f.lastTime, sizeof(actualBits));
                CHECK(actualBits == argumentBits[i] && f.lastWorld == f.World());
            }
        }
        {
            Fixture f;
            InFlightInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            CHECK(f.atReset.installation == LIFECYCLE_INSTALLING && f.atReset.boundaryDepth == 1);
            CHECK(f.insideResult == LIFECYCLE_REQUEST_DISABLED);
            CHECK(f.probe.Snapshot().boundaryDepth == 1 && f.probe.Snapshot().worldIdentity == 0);
            f.probe.Tick(f.World(), 1);
            CHECK(f.probe.Snapshot().worldIdentity == 0 && f.probe.Snapshot().tickCompletions == 0);
            CHECK(f.probe.EnqueueObservation(f.probe.Snapshot().epoch, 1) ==
                  LIFECYCLE_REQUEST_RESETTING);
            installer.Finish();
            CHECK(f.probe.Snapshot().boundaryDepth == 0 && f.resetCalls == 1);
            f.probe.Tick(f.World(), 1);
            CHECK(f.probe.Snapshot().worldIdentity == reinterpret_cast<uintptr_t>(f.World()));
            CHECK(f.probe.EnqueueObservation(f.probe.Snapshot().epoch, 1) ==
                  LIFECYCLE_REQUEST_ACCEPTED);
        }
        {
            Fixture f;
            InFlightInstaller installer(f);
            installer.failAt = LIFECYCLE_CLEAR;
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_FAILED);
            CHECK(f.probe.Snapshot().boundaryDepth == 1 && f.probe.Snapshot().attemptedMask == 7);
            f.probe.Tick(f.World(), 1);
            CHECK(f.tickCalls == 1 && f.probe.Snapshot().tickEntries == 0);
            CHECK(f.probe.EnqueueObservation(f.probe.Snapshot().epoch, 1) ==
                  LIFECYCLE_REQUEST_DISABLED);
            installer.Finish();
            CHECK(f.probe.Snapshot().boundaryDepth == 0 && f.resetCalls == 1);
            CHECK(f.probe.Snapshot().installation == LIFECYCLE_FAILED);
        }
    }

    void PartialFailures()
    {
        for (unsigned int index = 0; index != 3; ++index)
        {
            for (unsigned int missing = 0; missing != 2; ++missing)
            {
                Fixture f;
                FakeInstaller installer(f);
                const LifecycleHook hook = static_cast<LifecycleHook>(index);
                if (missing != 0)
                    installer.missingAt = hook;
                else
                    installer.failAt = hook;
                CHECK(f.probe.Install(installer, true) == LIFECYCLE_FAILED);
                const LifecycleSnapshot state = f.probe.Snapshot();
                CHECK(state.failureHook == hook);
                CHECK(state.failure == (missing ? LIFECYCLE_MISSING_ORIGINAL :
                                                   LIFECYCLE_ADD_HOOK_FAILED));
                CHECK(state.attemptedMask == (1u << (index + 1)) - 1);
                CHECK(state.successMask == (missing ? state.attemptedMask : (1u << index) - 1));
                CHECK(state.originalMask == (missing ? (1u << index) - 1 : state.attemptedMask));
                CHECK(state.worldIdentity == 0 && state.queued == 0);
                CHECK(f.probe.Install(installer, true) == LIFECYCLE_FAILED);
                CHECK(installer.addCalls == index + 1);
                CHECK(f.probe.EnqueueObservation(state.epoch, 1) == LIFECYCLE_REQUEST_DISABLED);
                if (missing == 0 || index > 0)
                {
                    f.probe.Tick(f.World(), 3.5f);
                    CHECK(f.tickCalls == 1 && f.lastWorld == f.World() && f.lastTime == 3.5f);
                    CHECK(f.probe.Snapshot().epoch == state.epoch);
                    CHECK(f.probe.Snapshot().tickEntries == 0);
                }
            }
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            installer.failAt = LIFECYCLE_TICK;
            installer.publishOnFail = false;
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_FAILED);
            CHECK(f.probe.Snapshot().originalMask == 0);
        }
        for (unsigned int nonStandard = 0; nonStandard != 2; ++nonStandard)
        {
            Fixture f;
            FakeInstaller installer(f);
            installer.throwAt = LIFECYCLE_RESET;
            installer.nonStandardThrow = nonStandard != 0;
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_FAILED);
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_INSTALLER_EXCEPTION);
            CHECK(f.probe.Snapshot().attemptedMask == 3 && f.probe.Snapshot().originalMask == 3);
            CHECK(installer.addCalls == 2);
            f.probe.Tick(f.World(), 1);
            f.probe.Reset(f.World());
            CHECK(f.tickCalls == 1 && f.resetCalls == 1);
            CHECK(f.probe.Snapshot().boundaryDepth == 0);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            installer.throwPrepare = true;
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_FAILED);
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_PREPARATION_FAILED);
            CHECK(f.probe.Snapshot().attemptedMask == 0 && installer.addCalls == 0);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_FAILED);
            CHECK(installer.prepareCalls == 1);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            f.probe.InitializationFailed("fixture catalog/configuration failure");
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_FAILED);
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_INITIALIZATION_FAILED);
            CHECK(installer.prepareCalls == 0 && installer.addCalls == 0);
        }
    }

    void QueueAndNestedReset()
    {
        Fixture f;
        FakeInstaller installer(f);
        CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
        f.probe.Tick(f.World(), 0);
        LifecycleEpoch epoch = f.probe.Snapshot().epoch;
        CHECK(f.probe.EnqueueObservation(0, 1) == LIFECYCLE_REQUEST_INVALID);
        CHECK(f.probe.EnqueueObservation(epoch, 0) == LIFECYCLE_REQUEST_INVALID);
        CHECK(f.probe.EnqueueObservation(epoch + 1, 1) == LIFECYCLE_REQUEST_STALE);
        for (unsigned int i = 1; i <= LifecycleProbe::QUEUE_CAPACITY; ++i)
            CHECK(f.probe.EnqueueObservation(epoch, i) == LIFECYCLE_REQUEST_ACCEPTED);
        CHECK(f.probe.EnqueueObservation(epoch, 65) == LIFECYCLE_REQUEST_FULL);
        f.probe.Tick(f.World(), 1);
        CHECK(f.atTick.queued == LifecycleProbe::QUEUE_CAPACITY);
        CHECK(f.probe.Snapshot().queued == 0 && f.probe.Snapshot().retired == 64);
        CHECK(f.probe.Snapshot().lastRetiredRequest == 64);

        for (unsigned int i = 1; i <= 3; ++i)
            CHECK(f.probe.EnqueueObservation(epoch, i) == LIFECYCLE_REQUEST_ACCEPTED);
        f.trace.clear();
        f.nestedReset = true;
        f.nestedClear = true;
        f.tickInsideClear = true;
        f.probe.Tick(f.World(), 1);
        const int ordering[] = { 1, 3, 5, 1, 2, 6, 4, 2 };
        CHECK(f.trace.size() == sizeof(ordering) / sizeof(ordering[0]));
        for (size_t i = 0; i < f.trace.size(); ++i)
            CHECK(f.trace[i] == ordering[i]);
        CHECK(f.atReset.boundaryDepth == 1 && f.atReset.queued == 0 &&
              f.atReset.worldIdentity == 0 && f.atReset.epoch == epoch + 1);
        CHECK(f.atClear.boundaryDepth == 2 && f.atClear.epoch == epoch + 2);
        CHECK(f.insideResult == LIFECYCLE_REQUEST_RESETTING);
        LifecycleSnapshot state = f.probe.Snapshot();
        CHECK(state.boundaryDepth == 0 && state.epoch == epoch + 2 && state.worldIdentity == 0);
        CHECK(state.invalidated == 3 && state.retired == 64 && state.queued == 0);
        CHECK(state.tickCompletions == 2); // Neither a teardown tick nor the old outer tick retires work.
        CHECK(f.probe.EnqueueObservation(epoch, 90) == LIFECYCLE_REQUEST_STALE);
        CHECK(f.probe.EnqueueObservation(state.epoch, 90) == LIFECYCLE_REQUEST_UNOBSERVED);
        f.probe.Tick(f.OtherWorld(), 1);
        epoch = f.probe.Snapshot().epoch;
        CHECK(f.probe.Snapshot().worldIdentity == reinterpret_cast<uintptr_t>(f.OtherWorld()));
        CHECK(f.probe.EnqueueObservation(epoch, 90) == LIFECYCLE_REQUEST_ACCEPTED);
        f.probe.Tick(f.OtherWorld(), 1);
        CHECK(f.probe.Snapshot().retired == 65 && f.probe.Snapshot().lastRetiredRequest == 90);
        for (unsigned int i = 0; i != 40; ++i)
            f.probe.Reset(f.OtherWorld());
        CHECK(f.DiagnosticCount(LIFECYCLE_BOUNDARY_DIAGNOSTIC) == LifecycleProbe::BOUNDARY_LOG_LIMIT);
        CHECK(f.DiagnosticCount(LIFECYCLE_BOUNDARY_LOG_LIMIT_DIAGNOSTIC) == 1);
        CHECK(f.DiagnosticCount(LIFECYCLE_FIRST_CALLBACK_DIAGNOSTIC) == 3);
        CHECK(f.probe.Snapshot().boundaryEntries == 42);
    }

    void OwnershipAndFailures()
    {
        for (unsigned int index = 0; index != 3; ++index)
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.probe.Tick(f.World(), 1);
            const LifecycleHook hook = static_cast<LifecycleHook>(index);
            if (hook == LIFECYCLE_RESET) f.probe.Reset(f.World());
            if (hook == LIFECYCLE_CLEAR) f.probe.Clear(f.World());
            f.probe.Tick(f.World(), 1);
            const LifecycleEpoch epoch = f.probe.Snapshot().epoch;
            CHECK(f.probe.EnqueueObservation(epoch, 1) == LIFECYCLE_REQUEST_ACCEPTED);
            Worker worker(f, hook);
            Handle thread(worker.Start());
            Wait(thread.Get());
            CHECK(!worker.failed);
            const LifecycleSnapshot state = f.probe.Snapshot();
            CHECK(state.installation == LIFECYCLE_FAILED && state.failure == LIFECYCLE_OWNER_CHANGED);
            CHECK(state.failureHook == hook && state.callbackOwners[hook] == GetCurrentThreadId());
            CHECK(state.queued == 0 && state.invalidated == 1 && state.epoch == epoch + 1);
            CHECK(state.boundaryDepth == 0 && state.worldIdentity == 0);
            CHECK(f.tickCalls == (hook == LIFECYCLE_TICK ? 3 : 2));
            CHECK(f.resetCalls == (hook == LIFECYCLE_RESET ? 2 : 0));
            CHECK(f.clearCalls == (hook == LIFECYCLE_CLEAR ? 2 : 0));
        }
        for (unsigned int index = 0; index != 3; ++index)
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.probe.Tick(f.World(), 1);
            CHECK(f.probe.EnqueueObservation(f.probe.Snapshot().epoch, 1) == LIFECYCLE_REQUEST_ACCEPTED);
            f.throwingHook = static_cast<LifecycleHook>(index);
            bool propagated = false;
            try
            {
                if (index == LIFECYCLE_TICK) f.probe.Tick(f.World(), 1);
                else if (index == LIFECYCLE_RESET) f.probe.Reset(f.World());
                else f.probe.Clear(f.World());
            }
            catch (int error) { propagated = error == 731; }
            CHECK(propagated);
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_ORIGINAL_EXCEPTION);
            CHECK(f.probe.Snapshot().boundaryDepth == 0 && f.probe.Snapshot().queued == 0);
            CHECK(f.probe.Snapshot().invalidated == 1 && f.probe.Snapshot().retired == 0);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.probe.Tick(f.World(), 1);
            const LifecycleEpoch epoch = f.probe.Snapshot().epoch;
            CHECK(f.probe.EnqueueObservation(epoch, 1) == LIFECYCLE_REQUEST_ACCEPTED);
            f.nestedClear = true;
            f.throwingHook = LIFECYCLE_CLEAR;
            bool propagated = false;
            try { f.probe.Reset(f.World()); }
            catch (int error) { propagated = error == 731; }
            CHECK(propagated && f.resetCalls == 1 && f.clearCalls == 1);
            CHECK(f.atReset.boundaryDepth == 1 && f.atClear.boundaryDepth == 2);
            const LifecycleSnapshot state = f.probe.Snapshot();
            CHECK(state.failure == LIFECYCLE_ORIGINAL_EXCEPTION &&
                  state.failureHook == LIFECYCLE_CLEAR);
            CHECK(state.boundaryDepth == 0 && state.epoch == epoch + 3);
            CHECK(state.invalidated == 1 && state.queued == 0 && state.retired == 0);
            CHECK(f.DiagnosticCount(LIFECYCLE_FAILURE_DIAGNOSTIC) == 1);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.probe.Tick(f.World(), 1);
            f.probe.Tick(f.OtherWorld(), 1);
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_WORLD_CHANGED_WITHOUT_BOUNDARY);
            CHECK(f.tickCalls == 2 && f.lastWorld == f.OtherWorld());
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.probe.Tick(NULL, 1);
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_INVALID_CALLBACK);
            CHECK(f.tickCalls == 1 && f.lastWorld == NULL);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.probe.Tick(f.World(), 1);
            LifecycleTestAccess::ExhaustEpoch(f.probe);
            f.probe.Reset(f.World());
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_EPOCH_EXHAUSTED);
            CHECK(f.probe.Snapshot().epoch == (std::numeric_limits<LifecycleEpoch>::max)());
            CHECK(f.resetCalls == 1 && f.probe.Snapshot().boundaryDepth == 0);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            f.throwSink = true;
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_FAILED);
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_DIAGNOSTIC_FAILED);
            CHECK(installer.prepareCalls == 0 && installer.addCalls == 0);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.throwSink = true;
            f.probe.Tick(f.World(), 1);
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_DIAGNOSTIC_FAILED);
            CHECK(f.tickCalls == 1);
            f.probe.Tick(f.World(), 1);
            CHECK(f.tickCalls == 2 && f.probe.Snapshot().retired == 0);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.probe.Tick(f.World(), 1);
            CHECK(f.probe.EnqueueObservation(f.probe.Snapshot().epoch, 1) ==
                  LIFECYCLE_REQUEST_ACCEPTED);
            f.probe.InitializationFailed("fixture startup logging failed after hooks were armed");
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_INITIALIZATION_FAILED);
            CHECK(f.probe.Snapshot().invalidated == 1 && f.probe.Snapshot().queued == 0);
            f.probe.Tick(f.World(), 3);
            CHECK(f.tickCalls == 2 && f.lastTime == 3 && f.probe.Snapshot().tickEntries == 1);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.probe.Tick(f.World(), 1);
            f.throwingHook = LIFECYCLE_TICK;
            f.throwSink = true;
            bool propagated = false;
            try { f.probe.Tick(f.World(), 1); }
            catch (int error) { propagated = error == 731; }
            CHECK(propagated && f.tickCalls == 2);
            CHECK(f.probe.Snapshot().failure == LIFECYCLE_ORIGINAL_EXCEPTION);
        }
    }

    void ConcurrentObservation()
    {
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.probe.Tick(f.World(), 1);
            Worker one(f, LIFECYCLE_TICK);
            Worker two(f, LIFECYCLE_TICK);
            one.enqueue = two.enqueue = true;
            one.epoch = two.epoch = f.probe.Snapshot().epoch;
            Handle first(one.Start());
            Handle second(two.Start());
            f.probe.Reset(f.World());
            Wait(first.Get());
            Wait(second.Get());
            CHECK(!one.failed && !two.failed);
            const unsigned int accepted = one.accepted + two.accepted;
            CHECK(accepted <= LifecycleProbe::QUEUE_CAPACITY);
            CHECK(accepted + one.full + two.full + one.stale + two.stale +
                  one.resetting + two.resetting == 256);
            const LifecycleSnapshot state = f.probe.Snapshot();
            CHECK(state.accepted == accepted && state.invalidated == accepted);
            CHECK(state.queued == 0 && state.retired == 0 && state.accepted + state.rejected == 257);
            CHECK(state.epoch == one.epoch + 1 && state.installation == LIFECYCLE_ACTIVE);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.probe.Tick(f.World(), 1);
            Worker one(f, LIFECYCLE_TICK);
            Worker two(f, LIFECYCLE_TICK);
            one.enqueue = two.enqueue = true;
            one.epoch = two.epoch = f.probe.Snapshot().epoch;
            Handle first(one.Start());
            Handle second(two.Start());
            Wait(first.Get());
            Wait(second.Get());
            CHECK(!one.failed && !two.failed);
            CHECK(one.accepted + two.accepted == LifecycleProbe::QUEUE_CAPACITY);
            CHECK(one.full + two.full == 256 - LifecycleProbe::QUEUE_CAPACITY);
            CHECK(f.probe.Snapshot().queued == LifecycleProbe::QUEUE_CAPACITY);
            f.blockReset = true;
            Worker reset(f, LIFECYCLE_RESET);
            Handle resetting(reset.Start());
            Wait(f.entered.Get());
            CHECK(f.atReset.queued == 0 && f.atReset.boundaryDepth == 1);
            Worker producer(f, LIFECYCLE_TICK);
            producer.enqueue = true;
            producer.epoch = f.probe.Snapshot().epoch;
            Handle producing(producer.Start());
            Wait(producing.Get());
            CHECK(!producer.failed && producer.resetting == 128 && producer.accepted == 0);
            // A different first owner for clear is allowed; overlapping teardown stays nested.
            f.probe.Clear(f.World());
            CHECK(f.atClear.boundaryDepth == 2 && f.probe.Snapshot().boundaryDepth == 1);
            Signal(f.release.Get());
            Wait(resetting.Get());
            CHECK(!reset.failed);
            const LifecycleSnapshot state = f.probe.Snapshot();
            CHECK(state.installation == LIFECYCLE_ACTIVE && state.boundaryDepth == 0);
            CHECK(state.callbackOwners[LIFECYCLE_TICK] == GetCurrentThreadId());
            CHECK(state.callbackOwners[LIFECYCLE_CLEAR] == GetCurrentThreadId());
            CHECK(state.callbackOwners[LIFECYCLE_RESET] != GetCurrentThreadId());
            CHECK(state.queued == 0 && state.accepted == state.invalidated && state.retired == 0);
            CHECK(state.invalidated == LifecycleProbe::QUEUE_CAPACITY && state.worldIdentity == 0);
        }
        {
            Fixture f;
            FakeInstaller installer(f);
            CHECK(f.probe.Install(installer, true) == LIFECYCLE_ACTIVE);
            f.blockTick = true;
            Worker tick(f, LIFECYCLE_TICK);
            Handle ticking(tick.Start());
            Wait(f.entered.Get());
            const LifecycleEpoch oldEpoch = f.probe.Snapshot().epoch;
            CHECK(f.probe.EnqueueObservation(oldEpoch, 1) == LIFECYCLE_REQUEST_ACCEPTED);
            f.probe.Reset(f.World());
            const LifecycleEpoch newEpoch = f.probe.Snapshot().epoch;
            CHECK(newEpoch == oldEpoch + 1);
            CHECK(f.probe.EnqueueObservation(oldEpoch, 2) == LIFECYCLE_REQUEST_STALE);
            CHECK(f.probe.EnqueueObservation(newEpoch, 2) == LIFECYCLE_REQUEST_UNOBSERVED);
            Signal(f.release.Get());
            Wait(ticking.Get());
            CHECK(!tick.failed);
            CHECK(f.probe.Snapshot().tickCompletions == 0 && f.probe.Snapshot().retired == 0);
            CHECK(f.probe.Snapshot().worldIdentity == 0 && f.probe.Snapshot().invalidated == 1);
        }
    }

    bool Options(const std::string& text)
    {
        std::istringstream input(text);
        return ParseLifecycleOptions(input, "fixture.lifecycle.cfg");
    }

    class ConfigFixture
    {
    public:
        ConfigFixture()
        {
            wchar_t module[MAX_PATH];
            const DWORD length = GetModuleFileNameW(NULL, module, MAX_PATH);
            if (length == 0 || length == MAX_PATH)
                throw std::runtime_error("cannot locate lifecycle test output directory");
            std::wstring directory(module, length);
            directory.erase(directory.find_last_of(L'\\') + 1);
            wchar_t path[MAX_PATH];
            if (!GetTempFileNameW(directory.c_str(), L"PCL", 0, path))
                throw std::runtime_error("cannot create ignored lifecycle configuration fixture");
            path_ = path;
            directory_ = directory;
        }
        ~ConfigFixture() { DeleteFileW(path_.c_str()); }
        const std::wstring& Path() const { return path_; }
        const std::wstring& Directory() const { return directory_; }
        void Write(const std::string& text)
        {
            Handle file(CreateFileW(path_.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, NULL));
            DWORD written = 0;
            if (!WriteFile(file.Get(), text.data(), static_cast<DWORD>(text.size()), &written, NULL) ||
                written != text.size())
                throw std::runtime_error("cannot write lifecycle configuration fixture");
        }
    private:
        std::wstring path_;
        std::wstring directory_;
    };

    void Configuration()
    {
        CHECK(Options("projectcars-remake-lifecycle 1\nobserve enabled\n"));
        CHECK(!Options("projectcars-remake-lifecycle 1\nobserve disabled\n"));
        CHECK(Options("\xef\xbb\xbf# reference\r\nprojectcars-remake-lifecycle 1\r\n"
                      "\tobserve enabled # explicit\r\n"));
        const char* malformed[] = {
            "", "# absent required records", "observe enabled\n",
            "projectcars-remake-lifecycle 2\nobserve enabled\n",
            "projectcars-remake-lifecycle 1\n", "projectcars-remake-lifecycle 1\nobserve true\n",
            "projectcars-remake-lifecycle 1\nobserve enabled\nobserve disabled\n",
            "projectcars-remake-lifecycle 1\nobserve disabled\nobserve enabled\n",
            "projectcars-remake-lifecycle 1\nobserve enabled\nunknown record\n",
            "projectcars-remake-lifecycle 1\nobserve \"enabled\"\n",
            "projectcars-remake-lifecycle 1\nobserve enabled extra\n",
            "projectcars-remake-lifecycle 1\nprojectcars-remake-lifecycle 1\n"
        };
        for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); ++i)
        {
            bool rejected = false;
            try { Options(malformed[i]); }
            catch (const text::Error& error)
            {
                rejected = error.Source() == "fixture.lifecycle.cfg" && error.Line() != 0;
            }
            CHECK(rejected);
        }
        std::istringstream broken("projectcars-remake-lifecycle 1\nobserve enabled\n");
        broken.setstate(std::ios::badbit);
        bool readRejected = false;
        try { ParseLifecycleOptions(broken, "broken.lifecycle.cfg"); }
        catch (const text::Error& error) { readRejected = error.Detail() == "input read failed"; }
        CHECK(readRejected);
        ConfigFixture file;
        const LifecycleOptions staged = LoadLifecycleOptions(
            file.Directory() + L"ProjectCarsRemake.lifecycle.cfg.example");
        CHECK(staged.present && !staged.enabled);
        const LifecycleOptions absent = LoadLifecycleOptions(file.Path() + L".missing");
        CHECK(!absent.present && !absent.enabled);
        const std::wstring failures[] = { file.Path(), file.Directory(),
                                         file.Path() + L".missing\\option.cfg" };
        for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i)
        {
            bool rejected = false;
            try { LoadLifecycleOptions(failures[i]); }
            catch (const std::exception&) { rejected = true; }
            CHECK(rejected);
        }
        file.Write("projectcars-remake-lifecycle 1\nobserve disabled\n");
        LifecycleOptions loaded = LoadLifecycleOptions(file.Path());
        CHECK(loaded.present && !loaded.enabled);
        file.Write("projectcars-remake-lifecycle 1\nobserve enabled\n");
        loaded = LoadLifecycleOptions(file.Path());
        CHECK(loaded.present && loaded.enabled);
        {
            Handle locked(CreateFileW(file.Path().c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, NULL));
            bool rejected = false;
            try { LoadLifecycleOptions(file.Path()); }
            catch (const std::exception&) { rejected = true; }
            CHECK(rejected);
        }
        std::string maximum = "projectcars-remake-lifecycle 1\nobserve enabled\n#";
        maximum.append(4096 - maximum.size(), 'x');
        file.Write(maximum);
        loaded = LoadLifecycleOptions(file.Path());
        CHECK(loaded.present && loaded.enabled);
        file.Write(maximum + 'x');
        bool oversizedRejected = false;
        try { LoadLifecycleOptions(file.Path()); }
        catch (const std::exception& error)
        {
            oversizedRejected = std::string(error.what()).find("4096-byte") != std::string::npos;
        }
        CHECK(oversizedRejected);
    }

    void MissingOriginalDeathTest()
    {
        wchar_t path[MAX_PATH];
        const DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
        CHECK(length != 0 && length < MAX_PATH);
        std::wstring command = L"\"" + std::wstring(path, length) +
                               L"\" --lifecycle-missing-original";
        STARTUPINFOW startup;
        PROCESS_INFORMATION process;
        std::memset(&startup, 0, sizeof(startup));
        std::memset(&process, 0, sizeof(process));
        startup.cb = sizeof(startup);
        CHECK(CreateProcessW(NULL, &command[0], NULL, NULL, FALSE, CREATE_NO_WINDOW,
                             NULL, NULL, &startup, &process) != FALSE);
        Handle thread(process.hThread);
        Handle child(process.hProcess);
        Wait(child.Get());
        DWORD exit = 0;
        CHECK(GetExitCodeProcess(child.Get(), &exit) != FALSE);
        CHECK(exit == 0xe050434c);
    }
}

void RunLifecycleMissingOriginalFixture()
{
    Fixture f;
    FakeInstaller installer(f);
    installer.missingAt = projectcars::LIFECYCLE_TICK;
    f.probe.Install(installer, true);
    f.probe.Tick(f.World(), 1);
    throw std::runtime_error("missing original returned instead of failing fatally");
}

void RunLifecycleTests()
{
    Configuration();
    DisabledAndInstallation();
    PartialFailures();
    QueueAndNestedReset();
    OwnershipAndFailures();
    ConcurrentObservation();
    MissingOriginalDeathTest();
    std::cout << "PASS: " << assertions
              << " lifecycle assertions; standalone fake hooks/ownership/queue only; no SDK loaded\n";
}
