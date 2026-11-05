// SPDX-License-Identifier: GPL-3.0-only
#include "lifecycle.h"

#pragma warning(push, 0)
#include <core/Functions.h>
#include <kenshi/GameWorld.h>
#pragma warning(pop)
#include <Debug.h>

#include <cstdio>
#include <stdexcept>
#include <sstream>

namespace projectcars
{
    namespace
    {
        LifecycleProbe* volatile publishedProbe = NULL;
        const char lifecycleModuleMarker = 0;

        const char* HookName(LifecycleHook hook)
        {
            switch (hook)
            {
            case LIFECYCLE_TICK: return "world-tick";
            case LIFECYCLE_RESET: return "resetGame";
            case LIFECYCLE_CLEAR: return "clear-world";
            default: return "installation";
            }
        }

        void DebuggerDiagnostic(void*, const LifecycleDiagnostic& diagnostic)
        {
            char message[1536];
            _snprintf_s(message, sizeof(message), _TRUNCATE,
                "ProjectCarsRemake LIFECYCLE: %s; %s; state=%u failure=%u "
                "attempted/success/original=%u/%u/%u epoch=%I64u depth=%u "
                "owners(tick/reset/clear)=%lu/%lu/%lu queued=%u invalidated=%I64u\n",
                HookName(diagnostic.hook), diagnostic.detail,
                static_cast<unsigned int>(diagnostic.snapshot.installation),
                static_cast<unsigned int>(diagnostic.snapshot.failure),
                diagnostic.snapshot.attemptedMask, diagnostic.snapshot.successMask,
                diagnostic.snapshot.originalMask, diagnostic.snapshot.epoch,
                diagnostic.snapshot.boundaryDepth,
                diagnostic.snapshot.callbackOwners[0], diagnostic.snapshot.callbackOwners[1],
                diagnostic.snapshot.callbackOwners[2], diagnostic.snapshot.queued,
                diagnostic.snapshot.invalidated);
            // Pinned SDK logging uses unsynchronized streams; callbacks never invoke it or UI.
            OutputDebugStringA(message);
        }

        LifecycleProbe& Probe()
        {
            LifecycleProbe* probe = static_cast<LifecycleProbe*>(InterlockedCompareExchangePointer(
                reinterpret_cast<PVOID volatile*>(&publishedProbe), NULL, NULL));
            if (probe == NULL)
                LifecycleFatal("native callback has no published process-lifetime context");
            return *probe;
        }

        void __cdecl TickDetour(GameWorld* world, float time) { Probe().Tick(world, time); }
        void __cdecl ResetDetour(GameWorld* world) { Probe().Reset(world); }
        void __cdecl ClearDetour(GameWorld* world) { Probe().Clear(world); }

        void ValidateTarget(intptr_t target, LifecycleHook hook)
        {
            MEMORY_BASIC_INFORMATION memory;
            if (target == 0 ||
                VirtualQuery(reinterpret_cast<LPCVOID>(target), &memory, sizeof(memory)) != sizeof(memory))
                throw std::runtime_error(std::string("SDK ") + HookName(hook) +
                                         " target is null or has no memory mapping");
            const DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                                     PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
            if (memory.State != MEM_COMMIT || (memory.Protect & executable) == 0 ||
                (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0 ||
                memory.AllocationBase != GetModuleHandleW(NULL))
                throw std::runtime_error(std::string("SDK ") + HookName(hook) +
                                         " target is not executable memory in the host EXE");
        }

        class NativeInstaller : public LifecycleInstaller
        {
        public:
            explicit NativeInstaller(LifecycleProbe& probe) : probe_(probe) {}
            virtual void Prepare()
            {
                typedef void (GameWorld::*TickStub)(float);
                typedef void (GameWorld::*BoundaryStub)();
                const TickStub tick = &GameWorld::_NV_mainLoop_GPUSensitiveStuff;
                const BoundaryStub reset = &GameWorld::resetGame;
                const BoundaryStub clear = &GameWorld::_clearAndDestroyGameWorldStuff;
                targets_[0] = KenshiLib::GetRealAddress(tick);
                targets_[1] = KenshiLib::GetRealAddress(reset);
                targets_[2] = KenshiLib::GetRealAddress(clear);
                for (unsigned int i = 0; i != 3; ++i)
                    ValidateTarget(targets_[i], static_cast<LifecycleHook>(i));
                if (targets_[0] == targets_[1] || targets_[0] == targets_[2] ||
                    targets_[1] == targets_[2])
                    throw std::runtime_error("SDK lifecycle targets unexpectedly alias one another");

                HMODULE module = NULL;
                if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                        GET_MODULE_HANDLE_EX_FLAG_PIN,
                                        reinterpret_cast<LPCWSTR>(&lifecycleModuleMarker), &module))
                    throw std::runtime_error("cannot pin lifecycle callback DLL for process lifetime");
                if (InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(&publishedProbe),
                                                       &probe_, NULL) != NULL)
                    throw std::runtime_error("native lifecycle context already published; no replacement");
            }

            virtual bool AddTick(WorldTickOriginal* original)
            {
                return KenshiLib::AddHook(targets_[0], reinterpret_cast<void*>(&TickDetour),
                                         original) == KenshiLib::SUCCESS;
            }
            virtual bool AddReset(WorldBoundaryOriginal* original)
            {
                return KenshiLib::AddHook(targets_[1], reinterpret_cast<void*>(&ResetDetour),
                                         original) == KenshiLib::SUCCESS;
            }
            virtual bool AddClear(WorldBoundaryOriginal* original)
            {
                return KenshiLib::AddHook(targets_[2], reinterpret_cast<void*>(&ClearDetour),
                                         original) == KenshiLib::SUCCESS;
            }
        private:
            NativeInstaller(const NativeInstaller&);
            NativeInstaller& operator=(const NativeInstaller&);
            LifecycleProbe& probe_;
            intptr_t targets_[3];
        };
    }

    void StartNativeLifecycle(const std::wstring& moduleDirectory)
    {
        // Deliberately never destroyed: AddHook has no per-plugin removal/lifetime handle.
        LifecycleProbe* probe = new LifecycleProbe(&DebuggerDiagnostic, NULL);
        try
        {
            const LifecycleOptions options =
                LoadLifecycleOptions(moduleDirectory + L"ProjectCarsRemake.lifecycle.cfg");
            NativeInstaller installer(*probe);
            const LifecycleInstallation installation = probe->Install(installer, options.enabled);
            if (installation == LIFECYCLE_FAILED)
            {
                const LifecycleSnapshot state = probe->Snapshot();
                std::ostringstream message;
                message << "lifecycle installation failed at " << HookName(state.failureHook)
                        << " (failure " << state.failure << ", attempted/success/original masks "
                        << state.attemptedMask << '/' << state.successMask << '/' << state.originalMask
                        << "). Any installed callbacks stay pass-through; no retry/unload";
                throw std::runtime_error(message.str());
            }
            if (!options.enabled)
                DebugLog(options.present ?
                    "Lifecycle DISABLED: DLL-local observe disabled; no SDK resolution or hooks." :
                    "Lifecycle DISABLED: optional DLL-local ProjectCarsRemake.lifecycle.cfg absent; "
                    "no SDK resolution or hooks.");
            else
                DebugLog("Lifecycle observation installed (opt-in, development only). "
                    "Callbacks use bounded OutputDebugString diagnostics, not SDK logging or UI. "
                    "DLL/context pinned until process exit; no native frame adoption or gameplay. "
                    "Actual host startup/reset/coexistence remain unverified.");
        }
        catch (const std::exception& error)
        {
            probe->InitializationFailed(error.what());
            throw;
        }
        catch (...)
        {
            probe->InitializationFailed("non-standard lifecycle startup exception");
            throw;
        }
    }
}
