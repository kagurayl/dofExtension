#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "squirrel.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: closure_source_selftest <utf8.nut>\n";
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) return 3;
    std::string utf8((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    if (utf8.empty() || utf8.size() > static_cast<std::size_t>(INT_MAX)) return 4;
    const int wchar_count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()),
        nullptr, 0);
    if (wchar_count <= 0) return 4;
    std::vector<wchar_t> source(static_cast<std::size_t>(wchar_count) + 1u);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                            static_cast<int>(utf8.size()), source.data(),
                            wchar_count) != wchar_count) return 5;
    source[static_cast<std::size_t>(wchar_count)] = L'\0';

    HSQUIRRELVM vm = sq_open(1024);
    if (!vm) return 6;
    const SQInteger old_top = sq_gettop(vm);
    sq_pushroottable(vm);
    sq_pushstring(vm, L"compilestring", -1);
    if (SQ_FAILED(sq_get(vm, -2))) {
        sq_close(vm);
        std::cerr << "compilestring missing\n";
        return 1;
    }
    sq_pushroottable(vm);
    sq_pushstring(vm, source.data(), static_cast<SQInteger>(wchar_count));
    sq_pushstring(vm, L"generated_closure_payload", -1);
    if (SQ_FAILED(sq_call(vm, 3, SQTrue, SQTrue))) {
        sq_close(vm);
        std::cerr << "compilestring call failed\n";
        return 1;
    }

    sq_pushroottable(vm);
    if (SQ_FAILED(sq_call(vm, 1, SQFalse, SQTrue))) {
        sq_close(vm);
        std::cerr << "execution failed\n";
        return 7;
    }
    sq_settop(vm, old_top);

    const std::string marker = "this.rawset(\"ChangQingFunctionChar";
    std::vector<std::wstring> required;
    for (std::size_t pos = 0; (pos = utf8.find(marker, pos)) != std::string::npos;) {
        const std::size_t begin = pos + std::string("this.rawset(\"").size();
        const std::size_t end = utf8.find('"', begin);
        if (end == std::string::npos) break;
        const std::string name = utf8.substr(begin, end - begin);
        required.emplace_back(name.begin(), name.end());
        pos = end + 1;
    }
    if (required.size() != 1803u) {
        sq_close(vm);
        std::cerr << "unexpected payload closure count\n";
        return 8;
    }
    for (const std::wstring& name : required) {
        sq_pushroottable(vm);
        sq_pushstring(vm, name.c_str(), -1);
        if (SQ_FAILED(sq_get(vm, -2))) {
            sq_close(vm);
            std::cerr << "missing generated closure\n";
            return 8;
        }
        sq_settop(vm, 0);
    }

    // Mirror GameCallNamed's success path: callback slot 1 is the root table,
    // sq_get resolves directly against it, and exactly one return object remains.
    const wchar_t call_test_source[] =
        L"this.rawset(\"__callnamed_stack_test\", function() { return 7; });";
    if (SQ_FAILED(sq_compilebuffer(
            vm, call_test_source,
            static_cast<SQInteger>(_countof(call_test_source) - 1),
            L"callnamed_stack_test", SQTrue))) {
        sq_close(vm);
        return 9;
    }
    sq_pushroottable(vm);
    if (SQ_FAILED(sq_call(vm, 1, SQFalse, SQTrue))) {
        sq_close(vm);
        return 10;
    }
    sq_settop(vm, 0);
    sq_pushroottable(vm); // callback environment at positive index 1
    const SQInteger call_old_top = sq_gettop(vm);
    sq_pushstring(vm, L"__callnamed_stack_test", -1);
    if (SQ_FAILED(sq_get(vm, 1))) {
        sq_close(vm);
        return 11;
    }
    sq_push(vm, 1);
    const SQRESULT named_call_result = sq_call(vm, 1, SQTrue, SQTrue);
    if (SQ_SUCCEEDED(named_call_result)) sq_remove(vm, -2);
    const SQInteger call_new_top = sq_gettop(vm);
    if (SQ_FAILED(named_call_result) || call_new_top != call_old_top + 1) {
        std::cerr << "callnamed stack contract failed result=" << named_call_result
                  << " old_top=" << call_old_top
                  << " new_top=" << call_new_top << "\n";
        sq_close(vm);
        return 12;
    }
    SQInteger call_result = 0;
    if (SQ_FAILED(sq_getinteger(vm, -1, &call_result)) || call_result != 7) {
        sq_close(vm);
        return 13;
    }
    sq_settop(vm, 0);
    sq_close(vm);
    std::cout << "compile_execute_ok wchar_count=" << wchar_count
              << " closures=" << required.size()
              << " callnamed_stack_ok=1\n";
    return 0;
}
