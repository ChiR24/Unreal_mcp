#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"

namespace McpSystemControlHandlers {
struct FPythonTempFileCleanup {
  TArray<FString> Paths;

  ~FPythonTempFileCleanup() {
    IPlatformFile& PlatformFile =
        FPlatformFileManager::Get().GetPlatformFile();
    for (const FString& Path : Paths) {
      PlatformFile.DeleteFile(*Path);
    }
  }
};

inline FString NormalizePythonPath(const FString& Path) {
  return Path.Replace(TEXT("\\"), TEXT("/"));
}

inline void AppendPythonExec(FString& Wrapper, const FString& ScriptPath,
                             const FString& ScriptDir) {
  Wrapper += FString::Printf(TEXT("    _script_path = r'%s'\n"), *ScriptPath);
  Wrapper += FString::Printf(TEXT("    _script_dir = r'%s'\n"), *ScriptDir);
  Wrapper += TEXT("    _exec_globals = globals()\n");
  Wrapper += TEXT("    _exec_globals['__name__'] = '__main__'\n");
  Wrapper += TEXT("    _exec_globals['__file__'] = _script_path\n");
  Wrapper += TEXT("    _exec_globals['__package__'] = None\n");
  Wrapper += TEXT("    _exec_globals['__cached__'] = None\n");
  Wrapper += TEXT("    if _script_dir and _script_dir not in sys.path:\n");
  Wrapper += TEXT("        sys.path.insert(0, _script_dir)\n");
  Wrapper += FString::Printf(
      TEXT("    exec(compile(_user_code, r'%s', 'exec'), _exec_globals)\n"),
      *ScriptPath);
}
}
