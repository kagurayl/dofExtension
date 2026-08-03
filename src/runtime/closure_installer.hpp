#pragma once

#include "dof_extension/extension_api.hpp"

namespace dofext {

bool InstallGeneratedClosures(VmHandle vm, void* module);
bool IsCurrentGameVm(VmHandle vm);

} // namespace dofext
