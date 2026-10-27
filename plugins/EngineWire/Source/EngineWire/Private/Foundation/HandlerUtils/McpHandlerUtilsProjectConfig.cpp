#include "Foundation/HandlerUtils/McpHandlerUtilsProjectConfig.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/Optional.h"
#include "Misc/Paths.h"

namespace McpHandlerUtils
{
namespace
{
FString ProjectConfigFilePath(const FString& ConfigName)
{
    return FPaths::ConvertRelativePathToFull(
        FPaths::ProjectConfigDir() / FString::Printf(TEXT("Default%s.ini"), *ConfigName));
}

// [Section] Key as it is on disk right now; unset when the file has no such entry.
TOptional<FString> ReadProjectConfigFileValue(const FString& File, const FString& Section, const FString& Key)
{
    FConfigFile OnDisk;
    OnDisk.Read(File);
    FString Value;
    return OnDisk.GetString(*Section, *Key, Value) ? TOptional<FString>(Value) : TOptional<FString>();
}
}

bool WriteProjectConfigValue(const FString& Section, const FString& Key, const FString& Value,
    const FString& ConfigName, FString& OutFile, FString& OutError)
{
    OutFile = ProjectConfigFilePath(ConfigName);

    // GConfig cannot write a Default*.ini: a file it did not load as part of a hierarchy is added
    // NoSave, so SetString plus Flush changes memory only. UpdateSinglePropertyInSection is the
    // engine's own in-place edit (what Project Settings uses for one property): it rewrites just this
    // key of this section on disk and keeps every other line of the file.
    FConfigFile Scratch;
    Scratch.SetString(*Section, *Key, *Value);
    const bool bWritten = Scratch.UpdateSinglePropertyInSection(*OutFile, *Key, *Section);

    // Evidence, not assumption: read the file back from disk.
    const TOptional<FString> ReadBack = ReadProjectConfigFileValue(OutFile, Section, Key);
    if (bWritten && ReadBack.IsSet() && ReadBack.GetValue() == Value)
    {
        return true;
    }
    OutError = FString::Printf(TEXT("[%s] %s=%s was not written to %s (%s): reading the file back found %s."),
        *Section, *Key, *Value, *OutFile,
        bWritten ? TEXT("the write reported success") : TEXT("the file could not be written, check that it is not read-only"),
        ReadBack.IsSet() ? *FString::Printf(TEXT("%s=%s"), *Key, *ReadBack.GetValue()) : TEXT("no such entry"));
    return false;
}

bool WriteProjectConfigValues(const TArray<FProjectConfigEntry>& Entries, const FString& ConfigName,
    FString& OutFile, TArray<FString>& OutWritten, FString& OutError)
{
    OutFile = ProjectConfigFilePath(ConfigName);
    OutWritten.Reset();
    TArray<TOptional<FString>> Previous;
    for (const FProjectConfigEntry& Entry : Entries)
    {
        Previous.Add(ReadProjectConfigFileValue(OutFile, Entry.Section, Entry.Key));
        FString WriteError;
        if (WriteProjectConfigValue(Entry.Section, Entry.Key, Entry.Value, ConfigName, OutFile, WriteError))
        {
            OutWritten.Add(FString::Printf(TEXT("[%s] %s=%s"), *Entry.Section, *Entry.Key, *Entry.Value));
            continue;
        }
        // Put back every entry already written, newest first: its old value, or no entry
        // (UpdateSinglePropertyInSection removes a key the scratch file does not hold).
        TArray<FString> NotRestored;
        for (int32 Index = OutWritten.Num() - 1; Index >= 0; --Index)
        {
            const FProjectConfigEntry& Done = Entries[Index];
            FConfigFile Scratch;
            if (Previous[Index].IsSet())
            {
                Scratch.SetString(*Done.Section, *Done.Key, *Previous[Index].GetValue());
            }
            Scratch.UpdateSinglePropertyInSection(*OutFile, *Done.Key, *Done.Section);
            if (ReadProjectConfigFileValue(OutFile, Done.Section, Done.Key) != Previous[Index])
            {
                NotRestored.Add(FString::Printf(TEXT("[%s] %s"), *Done.Section, *Done.Key));
            }
        }
        OutError = OutWritten.Num() == 0 ? WriteError
            : NotRestored.Num() == 0 ? FString::Printf(TEXT("%s The %d entries written before it were put back, so the file is unchanged."), *WriteError, OutWritten.Num())
            : FString::Printf(TEXT("%s Restoring the entries written before it failed for %s; check the file."), *WriteError, *FString::Join(NotRestored, TEXT(", ")));
        OutWritten.Reset();
        return false;
    }
    return true;
}
}
