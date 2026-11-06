#pragma once

#include <ryn/i18n.hpp>

#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

namespace ryn_test::i18n {

inline void suppress_dialogs() {
#if defined(_WIN32)
    SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if !defined(NDEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
#endif
}

inline void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <typename Function> void rejects(Function function, ryn::I18nErrorCode expected) {
    bool rejected = false;
    try {
        function();
    } catch (const ryn::I18nException& error) {
        require(error.error().code == expected, "incorrect typed failure category");
        rejected = true;
    }
    require(rejected, "invalid i18n input was accepted");
}

} // namespace ryn_test::i18n
