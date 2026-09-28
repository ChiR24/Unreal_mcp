#include "Foundation/HandlerUtils/McpHandlerUtilsProjectConfig.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace McpHandlerUtils
{
bool WriteProjectConfigValue(const FString& Section, const FString& Key, const FString& Value,
    const FString& ConfigName, FString& OutFile, FString& OutError)
{
    OutFile = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectConfigDir() / FString::Printf(TEXT("Default%s.ini"), *ConfigName));

    // GConfig cannot write a Default*.ini: a file it did not load as part of a hierarchy is added
    // NoSave, so SetString plus Flush changes memory only. UpdateSinglePropertyInSection is the
    // engine's own in-place edit (what Project Settings uses for one property): it rewrites just this
    // key of this section on disk and keeps every other line of the file.
    FConfigFile Scratch;
    Scratch.SetString(*Section, *Key, *Value);
    const bool bWritten = Scratch.UpdateSinglePropertyInSection(*OutFile, *Key, *Section);

    // Evidence, not assumption: read the file back from disk.
    FConfigFile OnDisk;
    OnDisk.Read(OutFile);
    FString ReadBack;
    const bool bFound = OnDisk.GetString(*Section, *Key, ReadBack);
    if (bWritten && bFound && ReadBack == Value)
    {
        return true;
    }
    OutError = FString::Printf(TEXT("[%s] %s=%s was not written to %s (%s): reading the file back found %s."),
        *Section, *Key, *Value, *OutFile,
        bWritten ? TEXT("the write reported success") : TEXT("the file could not be written, check that it is not read-only"),
        bFound ? *FString::Printf(TEXT("%s=%s"), *Key, *ReadBack) : TEXT("no such entry"));
    return false;
}
}
