#pragma once

#include "dof_extension/extension_api.hpp"

namespace dofext {

VmApi MakeCurrentGameVmApi();
bool TryUnwrapCurrentGameObject(VmHandle vm, int index, std::uintptr_t* out);
bool CurrentGameRootSlotExists(VmHandle vm, const wchar_t* name);
bool InstallCurrentGameSerializedClosure(VmHandle vm, const wchar_t* name,
                                         const void* stream, int stream_size);
bool CompileAndRunCurrentGameSource(VmHandle vm, const wchar_t* source,
                                    int source_length,
                                    const wchar_t* source_name);

} // namespace dofext
