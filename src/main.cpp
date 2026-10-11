// SPDX-License-Identifier: 0BSD
// Copyright (C) 2026 by I.C.KaZe

#include "App.h"

int WINAPI wWinMain(HINSTANCE h, HINSTANCE, LPWSTR, int)
{
    std::wstring project, language;
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::wstring arg = argv[i];
            if ((arg == L"--lang" || arg == L"--language" || arg == L"-lang") && i + 1 < argc)
                language = argv[++i];
            else if (arg.rfind(L"--lang=", 0) == 0)
                language = arg.substr(7);
            else if (arg.rfind(L"--language=", 0) == 0)
                language = arg.substr(11);
            else if (arg == L"ja" || arg == L"en")
                language = arg;
            else
            {
                auto ext = fs::path(arg).extension().wstring();
                if (!_wcsicmp(ext.c_str(), L".pdx") || !_wcsicmp(ext.c_str(), L".pdxedit"))
                    project = arg;
            }
        }
        LocalFree(argv);
    }
    App a;
    return a.Run(h, project, language);
}
