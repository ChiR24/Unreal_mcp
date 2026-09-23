#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/OutputDevice.h"

// The recent editor log, kept from startup so a caller can read what the log
// said without a live subscription: Live Coding results, PIE warnings,
// au.DumpActiveSounds. Before this the only way to see those lines was reading
// Saved/Logs from disk, outside the tool.
class FMcpLogHistory : public FOutputDevice
{
public:
    static FMcpLogHistory& Get();

    void Register();
    void Unregister();

    void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
    bool CanBeUsedOnAnyThread() const override { return true; }
    bool CanBeUsedOnMultipleThreads() const override { return true; }

    // The newest MaxLines lines that pass every filter, oldest first. OutMatched
    // counts every buffered match, so a caller can tell the cap cut some off.
    TArray<FString> Read(int32 MaxLines, const FString& Contains, const FString& Category,
                         ELogVerbosity::Type MinVerbosity, int32& OutMatched) const;

    // The same tail read over a log FILE some other process writes (the Live
    // Coding console's), opened shared because that process still holds it.
    static TArray<FString> ReadFileTail(const FString& Path, int32 MaxLines, const FString& Contains,
                                        int32& OutMatched);

    // A compiler diagnostic names its file by full host path, and the response
    // redactor removes that whole run up to the next quote, line number and
    // error code included: "X:\...\Foo.cpp(42): error C2440: 'x'" read back as
    // "[path redacted]'x'". This keeps the file name; the directory still never
    // leaves the editor.
    static FString KeepDiagnosticFileName(const FString& Line);

private:
    struct FLine
    {
        double Seconds = 0.0;
        FName Category;
        ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
        FString Message;
    };

    static constexpr int32 Capacity = 2000;
    mutable FCriticalSection Mutex;
    TArray<FLine> Lines;
    int32 Head = 0;
    bool bRegistered = false;
};
