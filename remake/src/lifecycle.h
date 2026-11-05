// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#if !defined(_MSC_VER) || _MSC_VER != 1600 || !defined(_M_X64)
#error ProjectCarsRemake requires the VC100 x64 compiler.
#endif
#if !defined(_DLL) || defined(_DEBUG)
#error ProjectCarsRemake requires the Release DLL runtime (/MD).
#endif

#include <Windows.h>
#include <stdint.h>
#include <istream>
#include <string>

class GameWorld;

namespace projectcars
{
    typedef unsigned __int64 LifecycleEpoch;
    // Win64 member-call ABI: explicit this in RCX, second-position float in XMM1.
    typedef void (__cdecl *WorldTickOriginal)(GameWorld*, float);
    typedef void (__cdecl *WorldBoundaryOriginal)(GameWorld*);

    struct LifecycleOptions
    {
        LifecycleOptions() : present(false), enabled(false) {}
        bool present;
        bool enabled;
    };

    bool ParseLifecycleOptions(std::istream& input, const std::string& source);
    LifecycleOptions LoadLifecycleOptions(const std::wstring& path);

    enum LifecycleHook
    {
        LIFECYCLE_TICK,
        LIFECYCLE_RESET,
        LIFECYCLE_CLEAR,
        LIFECYCLE_NO_HOOK
    };

    enum LifecycleInstallation
    {
        LIFECYCLE_NOT_STARTED,
        LIFECYCLE_DISABLED,
        LIFECYCLE_INSTALLING,
        LIFECYCLE_ACTIVE,
        LIFECYCLE_FAILED
    };

    enum LifecycleFailure
    {
        LIFECYCLE_NO_FAILURE,
        LIFECYCLE_INITIALIZATION_FAILED,
        LIFECYCLE_PREPARATION_FAILED,
        LIFECYCLE_ADD_HOOK_FAILED,
        LIFECYCLE_INSTALLER_EXCEPTION,
        LIFECYCLE_MISSING_ORIGINAL,
        LIFECYCLE_OWNER_CHANGED,
        LIFECYCLE_INVALID_CALLBACK,
        LIFECYCLE_WORLD_CHANGED_WITHOUT_BOUNDARY,
        LIFECYCLE_EPOCH_EXHAUSTED,
        LIFECYCLE_BOUNDARY_DEPTH_EXHAUSTED,
        LIFECYCLE_UNBALANCED_BOUNDARY,
        LIFECYCLE_ORIGINAL_EXCEPTION,
        LIFECYCLE_DIAGNOSTIC_FAILED
    };

    enum LifecycleRequestResult
    {
        LIFECYCLE_REQUEST_ACCEPTED,
        LIFECYCLE_REQUEST_DISABLED,
        LIFECYCLE_REQUEST_UNOBSERVED,
        LIFECYCLE_REQUEST_RESETTING,
        LIFECYCLE_REQUEST_STALE,
        LIFECYCLE_REQUEST_FULL,
        LIFECYCLE_REQUEST_INVALID
    };

    struct LifecycleSnapshot
    {
        LifecycleInstallation installation;
        LifecycleFailure failure;
        LifecycleHook failureHook;
        unsigned int attemptedMask;
        unsigned int successMask;
        unsigned int originalMask;
        LifecycleEpoch epoch;
        uintptr_t worldIdentity;
        DWORD callbackOwners[3];
        unsigned int boundaryDepth;
        unsigned int queued;
        unsigned __int64 tickEntries;
        unsigned __int64 tickCompletions;
        unsigned __int64 boundaryEntries;
        unsigned __int64 accepted;
        unsigned __int64 rejected;
        unsigned __int64 retired;
        unsigned __int64 invalidated;
        unsigned __int64 lastRetiredRequest;
    };

    enum LifecycleDiagnosticKind
    {
        LIFECYCLE_NO_DIAGNOSTIC,
        LIFECYCLE_INSTALLATION_DIAGNOSTIC,
        LIFECYCLE_DUPLICATE_DIAGNOSTIC,
        LIFECYCLE_FIRST_CALLBACK_DIAGNOSTIC,
        LIFECYCLE_BOUNDARY_DIAGNOSTIC,
        LIFECYCLE_BOUNDARY_LOG_LIMIT_DIAGNOSTIC,
        LIFECYCLE_FAILURE_DIAGNOSTIC
    };

    struct LifecycleDiagnostic
    {
        LifecycleDiagnostic() : kind(LIFECYCLE_NO_DIAGNOSTIC),
                                hook(LIFECYCLE_NO_HOOK), detail(NULL) {}
        LifecycleDiagnosticKind kind;
        LifecycleHook hook;
        LifecycleSnapshot snapshot;
        const char* detail;
    };

    typedef void (*LifecycleDiagnosticSink)(void*, const LifecycleDiagnostic&);

    class LifecycleInstaller
    {
    public:
        virtual ~LifecycleInstaller() {}
        virtual void Prepare() = 0;
        virtual bool AddTick(WorldTickOriginal* original) = 0;
        virtual bool AddReset(WorldBoundaryOriginal* original) = 0;
        virtual bool AddClear(WorldBoundaryOriginal* original) = 0;
    };

    class LifecycleProbe
    {
    public:
        enum { QUEUE_CAPACITY = 64, BOUNDARY_LOG_LIMIT = 8 };

        LifecycleProbe(LifecycleDiagnosticSink sink, void* sinkContext);
        LifecycleInstallation Install(LifecycleInstaller& installer, bool enabled);
        void InitializationFailed(const char* detail) throw();
        LifecycleSnapshot Snapshot() throw();
        LifecycleRequestResult EnqueueObservation(LifecycleEpoch epoch,
                                                   unsigned __int64 requestId) throw();

        void Tick(GameWorld* world, float time);
        void Reset(GameWorld* world);
        void Clear(GameWorld* world);

    private:
        struct Request
        {
            LifecycleEpoch epoch;
            unsigned __int64 id;
        };

        struct TickToken
        {
            TickToken() : epoch(0), eligible(false) {}
            LifecycleEpoch epoch;
            bool eligible;
        };

        LifecycleProbe(const LifecycleProbe&);
        LifecycleProbe& operator=(const LifecycleProbe&);
        friend struct LifecycleTestAccess;

        LifecycleDiagnostic DiagnosticLocked(LifecycleDiagnosticKind kind,
                                               LifecycleHook hook, const char* detail) const;
        LifecycleDiagnostic FailLocked(LifecycleFailure failure,
                                         LifecycleHook hook, const char* detail);
        bool InvalidateLocked();
        bool ObserveOwnerLocked(LifecycleHook hook, DWORD thread,
                                LifecycleDiagnostic& diagnostic);
        void Emit(const LifecycleDiagnostic& diagnostic) throw();
        bool RecordResult(LifecycleHook hook, bool success) throw();
        void Fail(LifecycleFailure failure, LifecycleHook hook, const char* detail) throw();
        TickToken BeginTick(GameWorld* world) throw();
        void CompleteTick(const TickToken& token) throw();
        bool BeginBoundary(LifecycleHook hook, GameWorld* world) throw();
        void EndBoundary(LifecycleHook hook, bool entered, bool originalSucceeded) throw();
        void Boundary(LifecycleHook hook, GameWorld* world, WorldBoundaryOriginal original);

        SRWLOCK lock_;
        LifecycleSnapshot state_;
        Request requests_[QUEUE_CAPACITY];
        WorldTickOriginal tickOriginal_;
        WorldBoundaryOriginal resetOriginal_;
        WorldBoundaryOriginal clearOriginal_;
        LifecycleDiagnosticSink sink_;
        void* sinkContext_;
        unsigned int boundaryLogs_;
        bool duplicateReported_;
        bool diagnosticFailureReported_;
    };

    __declspec(noreturn) void LifecycleFatal(const char* detail) throw();
    void StartNativeLifecycle(const std::wstring& moduleDirectory);
}
