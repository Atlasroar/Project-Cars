// SPDX-License-Identifier: GPL-3.0-only
#include "lifecycle.h"
#include "text.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>

namespace projectcars
{
    namespace
    {
        const DWORD fatalLifecycleExit = 0xe050434c;

        class Lock
        {
        public:
            explicit Lock(SRWLOCK& lock) : lock_(lock) { AcquireSRWLockExclusive(&lock_); }
            ~Lock() { ReleaseSRWLockExclusive(&lock_); }
        private:
            Lock(const Lock&);
            Lock& operator=(const Lock&);
            SRWLOCK& lock_;
        };

        class ConfigFile
        {
        public:
            explicit ConfigFile(HANDLE handle) : handle_(handle) {}
            ~ConfigFile()
            {
                if (handle_ != INVALID_HANDLE_VALUE && !CloseHandle(handle_))
                    OutputDebugStringA("ProjectCarsRemake: closing failed lifecycle configuration failed.\n");
            }
            HANDLE Get() const { return handle_; }
            void Close()
            {
                if (!CloseHandle(handle_))
                    throw std::runtime_error("CloseHandle failed for lifecycle configuration");
                handle_ = INVALID_HANDLE_VALUE;
            }
        private:
            ConfigFile(const ConfigFile&);
            ConfigFile& operator=(const ConfigFile&);
            HANDLE handle_;
        };

        std::runtime_error ConfigFailure(const char* operation, DWORD code)
        {
            std::ostringstream message;
            message << "DLL-local ProjectCarsRemake.lifecycle.cfg: " << operation
                    << " failed (Win32 error " << code << ')';
            return std::runtime_error(message.str());
        }

        void Increment(unsigned __int64& value)
        {
            if (value != (std::numeric_limits<unsigned __int64>::max)())
                ++value;
        }

        void AddCount(unsigned __int64& value, unsigned int amount)
        {
            const unsigned __int64 maximum = (std::numeric_limits<unsigned __int64>::max)();
            value = value > maximum - amount ? maximum : value + amount;
        }

        template<typename T>
        T ReadOriginal(T& storage)
        {
            // The SDK publishes this aligned slot before enabling; never read a torn pointer.
            return reinterpret_cast<T>(InterlockedCompareExchangePointer(
                reinterpret_cast<PVOID volatile*>(&storage), NULL, NULL));
        }
    }

    bool ParseLifecycleOptions(std::istream& input, const std::string& source)
    {
        bool header = false;
        bool observed = false;
        bool enabled = false;
        size_t lineNumber = 0;
        text::Location location;
        while (text::ReadLine(input, source, lineNumber, location))
        {
            const std::vector<text::Token> tokens = text::Tokenize(source, location);
            if (tokens.empty())
                continue;
            text::Arity(source, location, tokens, 2);
            if (tokens[0].quoted || tokens[1].quoted)
                text::Fail(source, location, "lifecycle configuration tokens must not be quoted");
            if (!header)
            {
                if (tokens[0].value != "projectcars-remake-lifecycle" || tokens[1].value != "1")
                    text::Fail(source, location, "expected projectcars-remake-lifecycle 1 header");
                header = true;
            }
            else
            {
                if (tokens[0].value != "observe" || observed)
                    text::Fail(source, location, "expected exactly one observe directive");
                if (tokens[1].value != "enabled" && tokens[1].value != "disabled")
                    text::Fail(source, location, "observe must be enabled or disabled");
                enabled = tokens[1].value == "enabled";
                observed = true;
            }
        }
        if (!header || !observed)
        {
            location.line = lineNumber + 1;
            location.context = "<end of file>";
            text::Fail(source, location, "lifecycle configuration requires header and observe directive");
        }
        return enabled;
    }

    LifecycleOptions LoadLifecycleOptions(const std::wstring& path)
    {
        LifecycleOptions options;
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            const DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND)
                return options;
            throw ConfigFailure("GetFileAttributesW", error);
        }
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            throw std::runtime_error("DLL-local lifecycle configuration is a directory, not a file");
        const HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle == INVALID_HANDLE_VALUE)
            throw ConfigFailure("CreateFileW for present configuration", GetLastError());
        ConfigFile file(handle);
        LARGE_INTEGER size;
        if (!GetFileSizeEx(file.Get(), &size))
            throw ConfigFailure("GetFileSizeEx", GetLastError());
        if (size.QuadPart < 0 || size.QuadPart > 4096)
            throw std::runtime_error("DLL-local lifecycle configuration exceeds the 4096-byte limit");

        char bytes[4096];
        const DWORD expected = static_cast<DWORD>(size.QuadPart);
        DWORD total = 0;
        while (total < expected)
        {
            DWORD read = 0;
            if (!ReadFile(file.Get(), bytes + total, expected - total, &read, NULL))
                throw ConfigFailure("ReadFile", GetLastError());
            if (read == 0)
                throw std::runtime_error("DLL-local lifecycle configuration changed or was truncated");
            total += read;
        }
        char extra;
        DWORD read = 0;
        if (!ReadFile(file.Get(), &extra, 1, &read, NULL))
            throw ConfigFailure("ReadFile at EOF", GetLastError());
        if (read != 0)
            throw std::runtime_error("DLL-local lifecycle configuration grew during reading");
        file.Close();
        std::istringstream input(std::string(bytes, total));
        options.enabled = ParseLifecycleOptions(input, "DLL-local ProjectCarsRemake.lifecycle.cfg");
        options.present = true;
        return options;
    }

    LifecycleProbe::LifecycleProbe(LifecycleDiagnosticSink sink, void* sinkContext)
        : tickOriginal_(NULL), resetOriginal_(NULL), clearOriginal_(NULL),
          sink_(sink), sinkContext_(sinkContext), boundaryLogs_(0),
          duplicateReported_(false), diagnosticFailureReported_(false)
    {
        if (sink == NULL)
            throw std::invalid_argument("lifecycle probe requires an explicit diagnostic sink");
        InitializeSRWLock(&lock_);
        std::memset(&state_, 0, sizeof(state_));
        state_.installation = LIFECYCLE_NOT_STARTED;
        state_.failureHook = LIFECYCLE_NO_HOOK;
        state_.epoch = 1;
    }

    LifecycleSnapshot LifecycleProbe::Snapshot() throw()
    {
        Lock lock(lock_);
        return state_;
    }

    LifecycleDiagnostic LifecycleProbe::DiagnosticLocked(LifecycleDiagnosticKind kind,
                                                          LifecycleHook hook,
                                                          const char* detail) const
    {
        LifecycleDiagnostic diagnostic;
        diagnostic.kind = kind;
        diagnostic.hook = hook;
        diagnostic.snapshot = state_;
        diagnostic.detail = detail;
        return diagnostic;
    }

    bool LifecycleProbe::InvalidateLocked()
    {
        AddCount(state_.invalidated, state_.queued);
        state_.queued = 0;
        state_.worldIdentity = 0;
        if (state_.epoch == (std::numeric_limits<LifecycleEpoch>::max)())
            return false;
        ++state_.epoch;
        return true;
    }

    LifecycleDiagnostic LifecycleProbe::FailLocked(LifecycleFailure failure,
                                                    LifecycleHook hook, const char* detail)
    {
        if (state_.installation == LIFECYCLE_FAILED)
            return LifecycleDiagnostic();
        state_.installation = LIFECYCLE_FAILED;
        state_.failure = failure;
        state_.failureHook = hook;
        InvalidateLocked();
        return DiagnosticLocked(LIFECYCLE_FAILURE_DIAGNOSTIC, hook, detail);
    }

    void LifecycleProbe::Fail(LifecycleFailure failure, LifecycleHook hook,
                              const char* detail) throw()
    {
        LifecycleDiagnostic diagnostic;
        {
            Lock lock(lock_);
            diagnostic = FailLocked(failure, hook, detail);
        }
        Emit(diagnostic);
    }

    void LifecycleProbe::InitializationFailed(const char* detail) throw()
    {
        Fail(LIFECYCLE_INITIALIZATION_FAILED, LIFECYCLE_NO_HOOK, detail);
    }

    void LifecycleProbe::Emit(const LifecycleDiagnostic& diagnostic) throw()
    {
        if (diagnostic.kind == LIFECYCLE_NO_DIAGNOSTIC)
            return;
        try
        {
            sink_(sinkContext_, diagnostic);
        }
        catch (...)
        {
            bool firstFailure = false;
            {
                Lock lock(lock_);
                firstFailure = !diagnosticFailureReported_;
                diagnosticFailureReported_ = true;
                if (state_.installation != LIFECYCLE_FAILED)
                    FailLocked(LIFECYCLE_DIAGNOSTIC_FAILED, diagnostic.hook,
                               "diagnostic sink threw; observation disabled");
            }
            if (firstFailure)
                OutputDebugStringA("ProjectCarsRemake LIFECYCLE DIAGNOSTIC FAILED: "
                                   "observation disabled; originals remain pass-through.\n");
        }
    }

    bool LifecycleProbe::RecordResult(LifecycleHook hook, bool success) throw()
    {
        const bool ready = hook == LIFECYCLE_TICK ? ReadOriginal(tickOriginal_) != NULL :
                           hook == LIFECYCLE_RESET ? ReadOriginal(resetOriginal_) != NULL :
                                                     ReadOriginal(clearOriginal_) != NULL;
        Lock lock(lock_);
        const unsigned int bit = 1u << hook;
        if (success)
            state_.successMask |= bit;
        if (ready)
            state_.originalMask |= bit;
        return ready;
    }

    LifecycleInstallation LifecycleProbe::Install(LifecycleInstaller& installer, bool enabled)
    {
        LifecycleDiagnostic diagnostic;
        bool started = false;
        {
            Lock lock(lock_);
            if (state_.installation != LIFECYCLE_NOT_STARTED)
            {
                if (!duplicateReported_)
                {
                    duplicateReported_ = true;
                    diagnostic = DiagnosticLocked(LIFECYCLE_DUPLICATE_DIAGNOSTIC,
                        LIFECYCLE_NO_HOOK, "duplicate installation refused; no retry or replacement");
                }
            }
            else
            {
                started = true;
                state_.installation = enabled ? LIFECYCLE_INSTALLING : LIFECYCLE_DISABLED;
                diagnostic = DiagnosticLocked(LIFECYCLE_INSTALLATION_DIAGNOSTIC,
                    LIFECYCLE_NO_HOOK, enabled ? "installing; observation not armed" :
                                                "disabled; no SDK target resolution or hooks");
            }
        }
        Emit(diagnostic);
        if (!started || !enabled || Snapshot().installation != LIFECYCLE_INSTALLING)
            return Snapshot().installation;

        try
        {
            installer.Prepare();
        }
        catch (const std::exception& error)
        {
            Fail(LIFECYCLE_PREPARATION_FAILED, LIFECYCLE_NO_HOOK, error.what());
            return Snapshot().installation;
        }
        catch (...)
        {
            Fail(LIFECYCLE_PREPARATION_FAILED, LIFECYCLE_NO_HOOK,
                 "non-standard preparation exception");
            return Snapshot().installation;
        }

        for (unsigned int index = 0; index != 3; ++index)
        {
            const LifecycleHook hook = static_cast<LifecycleHook>(index);
            const unsigned int bit = 1u << index;
            {
                Lock lock(lock_);
                if (state_.installation != LIFECYCLE_INSTALLING)
                    break;
                // Once AddHook is entered, even FAIL/throw can leave a live hook behind.
                state_.attemptedMask |= bit;
            }
            bool success = false;
            bool originalReady = false;
            try
            {
                if (hook == LIFECYCLE_TICK)
                {
                    success = installer.AddTick(&tickOriginal_);
                }
                else if (hook == LIFECYCLE_RESET)
                {
                    success = installer.AddReset(&resetOriginal_);
                }
                else
                {
                    success = installer.AddClear(&clearOriginal_);
                }
            }
            catch (const std::exception& error)
            {
                RecordResult(hook, false);
                Fail(LIFECYCLE_INSTALLER_EXCEPTION, hook, error.what());
                break;
            }
            catch (...)
            {
                RecordResult(hook, false);
                Fail(LIFECYCLE_INSTALLER_EXCEPTION, hook, "non-standard AddHook exception");
                break;
            }
            originalReady = RecordResult(hook, success);
            if (!success || !originalReady)
            {
                Fail(success ? LIFECYCLE_MISSING_ORIGINAL : LIFECYCLE_ADD_HOOK_FAILED,
                     hook, success ? "AddHook reported SUCCESS with no original trampoline" :
                                     "AddHook reported FAIL; partial/indeterminate install, no retry");
                break;
            }
        }
        diagnostic = LifecycleDiagnostic();
        {
            Lock lock(lock_);
            if (state_.installation == LIFECYCLE_INSTALLING &&
                state_.successMask == 7 && state_.originalMask == 7)
            {
                state_.installation = LIFECYCLE_ACTIVE;
                diagnostic = DiagnosticLocked(LIFECYCLE_INSTALLATION_DIAGNOSTIC,
                    LIFECYCLE_NO_HOOK, "all typed trampolines ready; observation armed");
            }
        }
        Emit(diagnostic);
        return Snapshot().installation;
    }

    bool LifecycleProbe::ObserveOwnerLocked(LifecycleHook hook, DWORD thread,
                                            LifecycleDiagnostic& diagnostic)
    {
        if (thread == 0)
        {
            diagnostic = FailLocked(LIFECYCLE_INVALID_CALLBACK, hook, "zero callback thread ID");
            return false;
        }
        DWORD& owner = state_.callbackOwners[hook];
        if (owner == 0)
        {
            owner = thread;
            diagnostic = DiagnosticLocked(LIFECYCLE_FIRST_CALLBACK_DIAGNOSTIC, hook,
                "first observed callback owner; not proof of UI or mutation safety");
        }
        else if (owner != thread)
        {
            diagnostic = FailLocked(LIFECYCLE_OWNER_CHANGED, hook,
                "callback changed observed thread owner; queued observations invalidated");
            return false;
        }
        return true;
    }

    LifecycleProbe::TickToken LifecycleProbe::BeginTick(GameWorld* world) throw()
    {
        const DWORD thread = GetCurrentThreadId();
        const uintptr_t identity = reinterpret_cast<uintptr_t>(world);
        TickToken token;
        LifecycleDiagnostic diagnostic;
        {
            Lock lock(lock_);
            if ((state_.installation == LIFECYCLE_ACTIVE ||
                 state_.installation == LIFECYCLE_INSTALLING) &&
                ObserveOwnerLocked(LIFECYCLE_TICK, thread, diagnostic))
            {
                Increment(state_.tickEntries);
                if (identity == 0)
                    diagnostic = FailLocked(LIFECYCLE_INVALID_CALLBACK, LIFECYCLE_TICK,
                                            "null world identity; original still receives null");
                else if (state_.installation == LIFECYCLE_ACTIVE && state_.boundaryDepth == 0)
                {
                    if (state_.worldIdentity != 0 && state_.worldIdentity != identity)
                        diagnostic = FailLocked(LIFECYCLE_WORLD_CHANGED_WITHOUT_BOUNDARY,
                            LIFECYCLE_TICK, "world identity changed without an observed reset/clear");
                    else
                    {
                        state_.worldIdentity = identity;
                        token.epoch = state_.epoch;
                        token.eligible = true;
                    }
                }
            }
        }
        Emit(diagnostic);
        return token;
    }

    void LifecycleProbe::CompleteTick(const TickToken& token) throw()
    {
        Lock lock(lock_);
        if (!token.eligible || state_.installation != LIFECYCLE_ACTIVE ||
            state_.boundaryDepth != 0 || state_.epoch != token.epoch)
            return;
        Increment(state_.tickCompletions);
        // Tickets are value-only observations, never callbacks or game-object work.
        for (unsigned int i = 0; i < state_.queued; ++i)
            state_.lastRetiredRequest = requests_[i].id;
        AddCount(state_.retired, state_.queued);
        state_.queued = 0;
    }

    bool LifecycleProbe::BeginBoundary(LifecycleHook hook, GameWorld* world) throw()
    {
        const DWORD thread = GetCurrentThreadId();
        bool entered = false;
        LifecycleDiagnostic ownerDiagnostic;
        LifecycleDiagnostic boundaryDiagnostic;
        {
            Lock lock(lock_);
            if ((state_.installation == LIFECYCLE_ACTIVE ||
                 state_.installation == LIFECYCLE_INSTALLING) &&
                ObserveOwnerLocked(hook, thread, ownerDiagnostic))
            {
                if (world == NULL)
                    boundaryDiagnostic = FailLocked(LIFECYCLE_INVALID_CALLBACK, hook,
                                                     "null world identity at teardown boundary");
                else if (state_.boundaryDepth == (std::numeric_limits<unsigned int>::max)())
                    boundaryDiagnostic = FailLocked(LIFECYCLE_BOUNDARY_DEPTH_EXHAUSTED, hook,
                                                     "teardown nesting depth exhausted");
                else if (!InvalidateLocked())
                    boundaryDiagnostic = FailLocked(LIFECYCLE_EPOCH_EXHAUSTED, hook,
                                                     "epoch exhausted; it must never wrap");
                else
                {
                    ++state_.boundaryDepth;
                    Increment(state_.boundaryEntries);
                    entered = true;
                    if (boundaryLogs_ < BOUNDARY_LOG_LIMIT)
                    {
                        ++boundaryLogs_;
                        boundaryDiagnostic = DiagnosticLocked(LIFECYCLE_BOUNDARY_DIAGNOSTIC,
                            hook, "epoch/queue invalidated BEFORE original teardown");
                    }
                    else if (boundaryLogs_ == BOUNDARY_LOG_LIMIT)
                    {
                        ++boundaryLogs_;
                        boundaryDiagnostic = DiagnosticLocked(LIFECYCLE_BOUNDARY_LOG_LIMIT_DIAGNOSTIC,
                            hook, "further reset/clear diagnostics suppressed; counters continue");
                    }
                }
            }
        }
        Emit(ownerDiagnostic);
        Emit(boundaryDiagnostic);
        return entered;
    }

    void LifecycleProbe::EndBoundary(LifecycleHook hook, bool entered,
                                     bool originalSucceeded) throw()
    {
        LifecycleDiagnostic diagnostic;
        {
            Lock lock(lock_);
            if (entered)
            {
                if (state_.boundaryDepth == 0)
                    diagnostic = FailLocked(LIFECYCLE_UNBALANCED_BOUNDARY, hook,
                                             "unbalanced teardown observation");
                else
                    --state_.boundaryDepth;
            }
            if (!originalSucceeded)
                diagnostic = FailLocked(LIFECYCLE_ORIGINAL_EXCEPTION, hook,
                    "original threw; observation disabled and exception rethrown unchanged");
        }
        Emit(diagnostic);
    }

    LifecycleRequestResult LifecycleProbe::EnqueueObservation(LifecycleEpoch epoch,
                                                              unsigned __int64 requestId) throw()
    {
        Lock lock(lock_);
        LifecycleRequestResult result = LIFECYCLE_REQUEST_ACCEPTED;
        if (epoch == 0 || requestId == 0)
            result = LIFECYCLE_REQUEST_INVALID;
        else if (state_.installation != LIFECYCLE_ACTIVE)
            result = LIFECYCLE_REQUEST_DISABLED;
        else if (state_.boundaryDepth != 0)
            result = LIFECYCLE_REQUEST_RESETTING;
        else if (epoch != state_.epoch)
            result = LIFECYCLE_REQUEST_STALE;
        else if (state_.worldIdentity == 0)
            result = LIFECYCLE_REQUEST_UNOBSERVED;
        else if (state_.queued == QUEUE_CAPACITY)
            result = LIFECYCLE_REQUEST_FULL;
        if (result != LIFECYCLE_REQUEST_ACCEPTED)
        {
            Increment(state_.rejected);
            return result;
        }
        requests_[state_.queued].epoch = epoch;
        requests_[state_.queued].id = requestId;
        ++state_.queued;
        Increment(state_.accepted);
        return result;
    }

    void LifecycleProbe::Tick(GameWorld* world, float time)
    {
        const WorldTickOriginal original = ReadOriginal(tickOriginal_);
        if (original == NULL)
            LifecycleFatal("tick callback has no original trampoline; cannot safely pass through");
        const TickToken token = BeginTick(world);
        try
        {
            original(world, time);
        }
        catch (...)
        {
            Fail(LIFECYCLE_ORIGINAL_EXCEPTION, LIFECYCLE_TICK,
                 "original tick threw; observation disabled and exception rethrown unchanged");
            throw;
        }
        CompleteTick(token);
    }

    void LifecycleProbe::Boundary(LifecycleHook hook, GameWorld* world,
                                   WorldBoundaryOriginal original)
    {
        if (original == NULL)
            LifecycleFatal("teardown callback has no original trampoline; cannot safely pass through");
        const bool entered = BeginBoundary(hook, world);
        try
        {
            original(world);
        }
        catch (...)
        {
            EndBoundary(hook, entered, false);
            throw;
        }
        EndBoundary(hook, entered, true);
    }

    void LifecycleProbe::Reset(GameWorld* world)
    {
        Boundary(LIFECYCLE_RESET, world, ReadOriginal(resetOriginal_));
    }

    void LifecycleProbe::Clear(GameWorld* world)
    {
        Boundary(LIFECYCLE_CLEAR, world, ReadOriginal(clearOriginal_));
    }

    __declspec(noreturn) void LifecycleFatal(const char* detail) throw()
    {
        OutputDebugStringA("ProjectCarsRemake LIFECYCLE FATAL CONTRACT FAILURE: ");
        OutputDebugStringA(detail);
        OutputDebugStringA(". Refusing recursive bypass or an apparent successful original call.\n");
        TerminateProcess(GetCurrentProcess(), fatalLifecycleExit);
        std::abort();
    }
}
