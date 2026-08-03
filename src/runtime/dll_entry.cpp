#include "dof_extension/extension_api.hpp"
#include "host/absolute_host_api.hpp"
#include "host/game_squirrel_vm_adapter.hpp"
#include "runtime/closure_installer.hpp"
#include "host/host_addresses.hpp"
#include "host/object_hooks.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace {

dofext::ExtensionContext g_context{};
void* g_registered_vm = nullptr;
HMODULE g_module = nullptr;
volatile LONG g_worker_running = 0;
HANDLE g_worker_thread = nullptr;
void* g_attempted_vm = nullptr;
DWORD g_next_registration_attempt = 0;
DWORD g_vm_observed_since = 0;
SRWLOCK g_registration_lock = SRWLOCK_INIT;
SRWLOCK g_worker_lock = SRWLOCK_INIT;
SRWLOCK g_lifecycle_lock = SRWLOCK_INIT;
INIT_ONCE g_context_once = INIT_ONCE_STATIC_INIT;

bool IsReadableAddress(const void* address, std::size_t size) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(address, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto end = begin + size;
    const auto region_end = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return end >= begin && end <= region_end;
}

void* CurrentSquirrelVm() {
    const auto slot = reinterpret_cast<void* const*>(dofext::address::kSquirrelVmGlobal);
    if (!IsReadableAddress(slot, sizeof(*slot))) return nullptr;
    return *slot;
}

bool RegisterCurrentVm() {
    if (!TryAcquireSRWLockExclusive(&g_registration_lock)) return false;
    void* vm = CurrentSquirrelVm();
    if (!vm) {
        // A transient null marks the previous VM generation dead. Clearing the
        // pointer makes a later allocator reuse at the same address re-register.
        if (g_registered_vm || g_attempted_vm) dofext::ClearObjectPaths();
        g_registered_vm = nullptr;
        g_attempted_vm = nullptr;
        g_next_registration_attempt = 0;
        g_vm_observed_since = 0;
        ReleaseSRWLockExclusive(&g_registration_lock);
        return false;
    }
    if (vm == g_registered_vm &&
        dofext::CurrentGameRootSlotExists(
            vm, L"ChangQingFunctionChar486701398")) {
        ReleaseSRWLockExclusive(&g_registration_lock);
        return true;
    }

    const DWORD now = GetTickCount();
    if (vm != g_attempted_vm) {
        dofext::ClearObjectPaths();
        g_attempted_vm = vm;
        g_next_registration_attempt = 0;
        g_vm_observed_since = now;
        ReleaseSRWLockExclusive(&g_registration_lock);
        return false;
    }
    // Avoid entering a VM during its creation/destruction transition. Require
    // the same non-null pointer to remain published for at least 500 ms.
    if (now - g_vm_observed_since < 500u) {
        ReleaseSRWLockExclusive(&g_registration_lock);
        return false;
    }
    if (g_next_registration_attempt != 0 &&
        static_cast<LONG>(now - g_next_registration_attempt) < 0) {
        ReleaseSRWLockExclusive(&g_registration_lock);
        return false;
    }
    g_next_registration_attempt = now + 5000u;

    if (!dofext::RegisterSquirrelExtensions(vm) ||
        !dofext::InstallGeneratedClosures(vm, g_module)) {
        ReleaseSRWLockExclusive(&g_registration_lock);
        return false;
    }
    g_registered_vm = vm;
    g_next_registration_attempt = 0;
    ReleaseSRWLockExclusive(&g_registration_lock);
    return true;
}

void InitializeContext();

DWORD WINAPI RuntimeWorker(void*) {
    // DllMain creates this thread only to preserve the original auto-start
    // contract. Delay all initialization until process attach has returned and
    // the loader lock window has passed.
    Sleep(10);

    bool hooks_ready = false;
    while (InterlockedCompareExchange(&g_worker_running, 1, 1) == 1) {
        if (TryAcquireSRWLockExclusive(&g_lifecycle_lock)) {
            if (InterlockedCompareExchange(&g_worker_running, 1, 1) == 1) {
                InitializeContext();
                hooks_ready = dofext::InstallObjectHooks();
                if (!hooks_ready) InterlockedExchange(&g_worker_running, 0);
            }
            ReleaseSRWLockExclusive(&g_lifecycle_lock);
            break;
        }
        Sleep(1);
    }

    // The protected original launches its worker from process attach. The
    // gameplay-relevant part waits for the game's Squirrel VM and registers the
    // native root-table closures after the VM exists; it must also handle a VM
    // replacement on relog/character change.
    while (InterlockedCompareExchange(&g_worker_running, 1, 1) == 1) {
        RegisterCurrentVm();
        Sleep(50);
    }
    return 0;
}

BOOL CALLBACK InitializeContextOnce(PINIT_ONCE, PVOID, PVOID*) {
    g_context.vm = dofext::MakeCurrentGameVmApi();
    g_context.host = dofext::MakeAbsoluteHostApi();
    dofext::SetExtensionContext(&g_context);
    return TRUE;
}

void InitializeContext() {
    InitOnceExecuteOnce(&g_context_once, &InitializeContextOnce, nullptr, nullptr);
}

bool StartWorker() {
    bool worker_ready = true;
    AcquireSRWLockExclusive(&g_worker_lock);
    if (InterlockedCompareExchange(&g_worker_running, 0, 0) == 0 &&
        g_worker_thread) {
        if (GetThreadId(g_worker_thread) != GetCurrentThreadId()) {
            WaitForSingleObject(g_worker_thread, INFINITE);
        }
        CloseHandle(g_worker_thread);
        g_worker_thread = nullptr;
    }
    if (InterlockedCompareExchange(&g_worker_running, 1, 0) == 0) {
        HANDLE thread = CreateThread(
            nullptr, 0, &RuntimeWorker, nullptr, CREATE_SUSPENDED, nullptr);
        if (thread && ResumeThread(thread) != static_cast<DWORD>(-1)) {
            g_worker_thread = thread;
        } else {
            if (thread) {
                TerminateThread(thread, 1);
                CloseHandle(thread);
            }
            InterlockedExchange(&g_worker_running, 0);
            worker_ready = false;
        }
    }
    ReleaseSRWLockExclusive(&g_worker_lock);
    return worker_ready;
}

void StopRuntimeWorker();
void ClearRegistrationState();

bool StartRuntime(bool register_immediately) {
    InitializeContext();
    const bool hooks_ready = dofext::InstallObjectHooks();
    const bool worker_ready = hooks_ready && StartWorker();
    if (!hooks_ready || !worker_ready) {
        StopRuntimeWorker();
        dofext::RemoveObjectHooks();
        ClearRegistrationState();
        return false;
    }
    if (register_immediately) RegisterCurrentVm();
    return true;
}

void StopRuntimeWorker() {
    // Keep the lifecycle lock until the old worker has observed stop and exited.
    // A concurrent StartHook cannot set the shared run flag back to one while
    // this worker is still draining.
    AcquireSRWLockExclusive(&g_worker_lock);
    InterlockedExchange(&g_worker_running, 0);
    HANDLE thread = g_worker_thread;

    if (thread && GetThreadId(thread) != GetCurrentThreadId()) {
        WaitForSingleObject(thread, INFINITE);
    }
    g_worker_thread = nullptr;
    if (thread) CloseHandle(thread);
    ReleaseSRWLockExclusive(&g_worker_lock);
}

void ClearRegistrationState() {
    AcquireSRWLockExclusive(&g_registration_lock);
    dofext::ClearObjectPaths();
    g_registered_vm = nullptr;
    g_attempted_vm = nullptr;
    g_next_registration_attempt = 0;
    g_vm_observed_since = 0;
    ReleaseSRWLockExclusive(&g_registration_lock);
}

} // namespace

namespace dofext {
bool IsCurrentGameVm(VmHandle vm) {
    return vm && ::CurrentSquirrelVm() == vm;
}
} // namespace dofext

extern "C" BOOL __cdecl StartHook() {
    AcquireSRWLockExclusive(&g_lifecycle_lock);
    const BOOL result = StartRuntime(true) ? TRUE : FALSE;
    ReleaseSRWLockExclusive(&g_lifecycle_lock);
    return result;
}

extern "C" BOOL __cdecl StopHook() {
    AcquireSRWLockExclusive(&g_lifecycle_lock);
    StopRuntimeWorker();
    dofext::RemoveObjectHooks();
    ClearRegistrationState();
    ReleaseSRWLockExclusive(&g_lifecycle_lock);
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        HMODULE pinned_module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(instance), &pinned_module)) {
            return FALSE;
        }
        g_module = pinned_module;
        DisableThreadLibraryCalls(instance);
        // Preserve the protected original's automatic worker lifecycle, but do
        // not perform config I/O, patching, or VM calls under the loader lock.
        StartWorker();
    } else if (reason == DLL_PROCESS_DETACH) {
        // Never wait or patch code under the loader lock. Controlled unload calls
        // StopHook first; process termination reclaims the remaining state.
        InterlockedExchange(&g_worker_running, 0);
        g_module = nullptr;
    }
    return TRUE;
}
