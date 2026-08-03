#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "squirrel.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct Cursor {
    const std::uint8_t* data;
    std::size_t size;
    std::size_t offset;
};

SQInteger ReadClosureBytes(SQUserPointer raw, SQUserPointer destination,
                           SQInteger requested) {
    auto* cursor = static_cast<Cursor*>(raw);
    if (!cursor || !destination || requested < 0 || cursor->offset > cursor->size)
        return 0;
    const auto count = static_cast<std::size_t>(requested);
    if (count > cursor->size - cursor->offset) return 0;
    std::memcpy(destination, cursor->data + cursor->offset, count);
    cursor->offset += count;
    return requested;
}

bool ReadU32(const std::vector<std::uint8_t>& payload, std::size_t& offset,
             std::uint32_t& value) {
    if (offset > payload.size() || payload.size() - offset < sizeof(value)) return false;
    std::memcpy(&value, payload.data() + offset, sizeof(value));
    offset += sizeof(value);
    return true;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 3;
    std::vector<std::uint8_t> payload(
        (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (payload.size() < 12 || std::memcmp(payload.data(), "CQBC", 4) != 0) return 4;

    std::size_t offset = 4;
    std::uint32_t version = 0, count = 0;
    if (!ReadU32(payload, offset, version) || !ReadU32(payload, offset, count) ||
        version != 1 || count != 1803) return 5;

    HSQUIRRELVM vm = sq_open(1024);
    if (!vm) return 6;
    std::vector<std::wstring> names;
    names.reserve(count);

    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t closure_id = 0, name_size = 0, stream_size = 0;
        if (!ReadU32(payload, offset, closure_id) ||
            !ReadU32(payload, offset, name_size) ||
            !ReadU32(payload, offset, stream_size) ||
            name_size == 0 || name_size > 63 || stream_size < 32 ||
            offset > payload.size() || name_size > payload.size() - offset ||
            stream_size > payload.size() - offset - name_size) {
            sq_close(vm);
            return 7;
        }
        std::string name(reinterpret_cast<const char*>(payload.data() + offset), name_size);
        offset += name_size;
        const std::string expected = "ChangQingFunctionChar" + std::to_string(closure_id);
        if (name != expected) {
            sq_close(vm);
            return 8;
        }
        std::wstring wide_name(name.begin(), name.end());
        const SQInteger old_top = sq_gettop(vm);
        sq_pushroottable(vm);
        sq_pushstring(vm, wide_name.c_str(), -1);
        Cursor cursor{payload.data() + offset, stream_size, 0};
        const SQRESULT read_result = sq_readclosure(vm, &ReadClosureBytes, &cursor);
        const bool trailer_ok = cursor.offset == cursor.size;
        const SQRESULT slot_result =
            (SQ_SUCCEEDED(read_result) && trailer_ok)
                ? sq_newslot(vm, -3, SQFalse) : SQ_ERROR;
        if (SQ_FAILED(read_result) || !trailer_ok || SQ_FAILED(slot_result)) {
            sq_getlasterror(vm);
            const SQChar* last_error = nullptr;
            sq_getstring(vm, -1, &last_error);
            std::wcerr << L"readclosure failed error="
                       << (last_error ? last_error : L"<non-string>") << L"\n";
            sq_pop(vm, 1);
            std::cerr << "readclosure failed index=" << index << " name=" << name
                      << " read_result=" << read_result
                      << " trailer_ok=" << trailer_ok
                      << " slot_result=" << slot_result
                      << " top=" << sq_gettop(vm)
                      << " offset=" << cursor.offset << '/' << cursor.size << "\n";
            sq_settop(vm, old_top);
            sq_close(vm);
            return 9;
        }
        sq_settop(vm, old_top);
        names.push_back(std::move(wide_name));
        offset += stream_size;
    }
    if (offset != payload.size()) {
        sq_close(vm);
        return 10;
    }

    for (const auto& name : names) {
        sq_pushroottable(vm);
        sq_pushstring(vm, name.c_str(), -1);
        if (SQ_FAILED(sq_get(vm, -2))) {
            sq_close(vm);
            return 11;
        }
        sq_settop(vm, 0);
    }
    sq_close(vm);
    std::cout << "readclosure_install_ok closures=" << names.size()
              << " payload_bytes=" << payload.size() << "\n";
    return 0;
}
