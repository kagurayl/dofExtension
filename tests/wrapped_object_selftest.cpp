#include "host/game_squirrel_vm_adapter.hpp"

#include <cstdint>
#include <cstdio>

int main() {
    alignas(8) std::uint32_t vm[16]{};
    alignas(8) std::uint32_t stack[16]{};
    alignas(8) std::uint32_t wrapper[9]{};

    vm[0x18 / 4] = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(stack));
    vm[0x34 / 4] = 1; // positive-index adjusted base becomes zero
    stack[2 * 2] = 0x0A008000u;
    stack[2 * 2 + 1] = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(wrapper));
    wrapper[0x20 / 4] = 0x12345678u;

    std::uintptr_t object = 0;
    const bool wrapped = dofext::TryUnwrapCurrentGameObject(vm, 2, &object);
    if (!wrapped || object != 0x12345678u) return 1;

    stack[2 * 2] = 0x00000001u;
    object = 0xFFFFFFFFu;
    const bool rejected = !dofext::TryUnwrapCurrentGameObject(vm, 2, &object) &&
                          object == 0;
    if (!rejected) return 2;

    std::printf("wrapped_type=0x0A008000 object=0x%08X invalid_type_rejected=1\n",
                static_cast<unsigned>(0x12345678u));
    return 0;
}
