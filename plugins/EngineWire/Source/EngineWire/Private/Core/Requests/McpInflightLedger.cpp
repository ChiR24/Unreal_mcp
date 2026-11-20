#include "Core/Requests/McpInflightLedger.h"
#include "Core/Subsystem/McpEndpointFile.h"
#include "EngineWireLog.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace McpInflightLedgerParts // named: a unity blob merges files, and anonymous helper names collide
{
FString Tag(const FString& Xml, const TCHAR* Name)
{
    const FString Open = FString::Printf(TEXT("<%s>"), Name);
    const int32 Start = Xml.Find(Open, ESearchCase::CaseSensitive);
    const int32 End = Start == INDEX_NONE ? INDEX_NONE
        : Xml.Find(FString::Printf(TEXT("</%s>"), Name), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
    return End == INDEX_NONE ? FString() : Xml.Mid(Start + Open.Len(), End - Start - Open.Len());
}

// Another process still running (a second editor or a commandlet of the same project) has ended nothing: its calls stay
// out of last-crash.json. ponytail: such a process writes the same inflight.json; one editor per project is the case.
bool IsRunning(uint32 Pid)
{
    FProcHandle Handle = FPlatformProcess::OpenProcess(Pid);
    const bool bRunning = Handle.IsValid() && FPlatformProcess::IsProcRunning(Handle);
    if (Handle.IsValid())
    {
        FPlatformProcess::CloseProc(Handle);
    }
    return bRunning;
}

// The crash reporter's own record of how that process ended, when it wrote one (an ensure ends nothing).
void AddCrashReport(const TSharedRef<FJsonObject>& Out, uint32 Pid)
{
    const FString Crashes = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Crashes"));
    TArray<FString> Folders;
    IFileManager::Get().FindFiles(Folders, *FPaths::Combine(Crashes, TEXT("*")), false, true);
    for (const FString& Folder : Folders)
    {
        FString Xml;
        if (!FFileHelper::LoadFileToString(Xml, *FPaths::Combine(Crashes, Folder, TEXT("CrashContext.runtime-xml"))) ||
            Tag(Xml, TEXT("ProcessId")) != FString::Printf(TEXT("%u"), Pid) || Tag(Xml, TEXT("CrashType")) == TEXT("Ensure"))
        {
            continue;
        }
        for (const TCHAR* Field : {TEXT("CrashType"), TEXT("ErrorMessage"), TEXT("TimeOfCrash"), TEXT("EngineVersion")})
        {
            FString Key(Field);
            Key[0] = FChar::ToLower(Key[0]);
            Out->SetStringField(Key, Tag(Xml, Field).Left(1024));
        }
        return;
    }
}
} // namespace McpInflightLedgerParts

FMcpInflightLedger& FMcpInflightLedger::Get()
{
    static FMcpInflightLedger Ledger(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MCP")));
    return Ledger;
}

void FMcpInflightLedger::WriteLocked() const
{
    const double Pid = static_cast<double>(FPlatformProcess::GetCurrentProcessId());
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FEntry& Entry : Entries)
    {
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("requestId"), Entry.RequestId);
        Row->SetStringField(TEXT("capability"), Entry.Capability);
        Row->SetStringField(TEXT("phase"), Entry.bLatent ? TEXT("latent") : TEXT("dispatch"));
        Row->SetStringField(TEXT("startedAtUtc"), Entry.StartedAtUtc);
        Row->SetNumberField(TEXT("pid"), Pid);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetArrayField(TEXT("entries"), Rows);
    McpEndpointFile::WriteWhole(FPaths::Combine(Dir, TEXT("inflight.json")), Json);
}

void FMcpInflightLedger::Dispatch(const FString& RequestId, const FString& Capability)
{
    if (RequestId.IsEmpty())
    {
        return;
    }
    FScopeLock Guard(&Mutex);
    Entries.RemoveAll([&RequestId](const FEntry& Entry) { return Entry.RequestId == RequestId; });
    Entries.Add({RequestId, Capability, FDateTime::UtcNow().ToIso8601(), false});
    WriteLocked();
}

void FMcpInflightLedger::Wait(const FString& RequestId)
{
    FScopeLock Guard(&Mutex);
    FEntry* Entry = Entries.FindByPredicate([&RequestId](const FEntry& Each) { return Each.RequestId == RequestId; });
    if (Entry && !Entry->bLatent)
    {
        Entry->bLatent = true;
        WriteLocked();
    }
}

void FMcpInflightLedger::Done(const FString& RequestId)
{
    FScopeLock Guard(&Mutex);
    if (Entries.RemoveAll([&RequestId](const FEntry& Entry) { return Entry.RequestId == RequestId; }) > 0)
    {
        WriteLocked();
    }
}

bool FMcpInflightLedger::IsOpen(const FString& RequestId)
{
    FScopeLock Guard(&Mutex);
    return Entries.ContainsByPredicate([&RequestId](const FEntry& Entry) { return Entry.RequestId == RequestId; });
}

int32 FMcpInflightLedger::Start()
{
    FScopeLock Guard(&Mutex);
    const uint32 Self = FPlatformProcess::GetCurrentProcessId();
    FString Text;
    TSharedPtr<FJsonObject> Old;
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    TArray<TSharedPtr<FJsonValue>> Left;
    uint32 Pid = 0;
    if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, TEXT("inflight.json"))))
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        if (FJsonSerializer::Deserialize(Reader, Old) && Old.IsValid() && Old->TryGetArrayField(TEXT("entries"), Rows) && Rows)
        {
            for (const TSharedPtr<FJsonValue>& Row : *Rows)
            {
                const TSharedPtr<FJsonObject>* Entry = nullptr;
                double EntryPid = 0.0;
                if (Row.IsValid() && Row->TryGetObject(Entry) && Entry && (*Entry)->TryGetNumberField(TEXT("pid"), EntryPid) &&
                    static_cast<uint32>(EntryPid) != Self && !McpInflightLedgerParts::IsRunning(static_cast<uint32>(EntryPid)))
                {
                    Pid = static_cast<uint32>(EntryPid); // ponytail: one earlier editor per project; the last pid wins
                    Left.Add(Row);
                }
            }
        }
    }
    if (Left.Num() > 0)
    {
        const TSharedRef<FJsonObject> Crash = MakeShared<FJsonObject>();
        Crash->SetNumberField(TEXT("processId"), Pid);
        McpInflightLedgerParts::AddCrashReport(Crash, Pid);
        Crash->SetArrayField(TEXT("inflight"), Left);
        McpEndpointFile::WriteWhole(FPaths::Combine(Dir, TEXT("last-crash.json")), Crash);
        FString Type;
        FString Message;
        EndedHow = Crash->TryGetStringField(TEXT("crashType"), Type)
            ? FString::Printf(TEXT("a %s: %s"), *Type, Crash->TryGetStringField(TEXT("errorMessage"), Message) ? *Message.Left(200) : TEXT(""))
            : FString(TEXT("no crash report"));
        for (const TSharedPtr<FJsonValue>& Row : Left)
        {
            FString Capability;
            if (Row->AsObject()->TryGetStringField(TEXT("capability"), Capability))
            {
                EndedIn.Add(Capability);
            }
        }
    }
    Entries.Reset();
    WriteLocked();
    return Left.Num();
}

void FMcpInflightLedger::WarnIfEndedIn(const FString& Capability)
{
    FString How;
    {
        FScopeLock Guard(&Mutex);
        if (EndedIn.Remove(Capability) == 0)
        {
            return;
        }
        How = EndedHow;
    }
    // A warning logged inside the request's error capture goes on its reply's warnings.
    UE_LOG(LogEngineWireSubsystem, Warning, TEXT("The last editor of this project ended while running %s (%s); it runs "
        "again now, so save first if it could take the editor down again."), *Capability, *How);
}

void FMcpInflightLedger::Stop()
{
    FScopeLock Guard(&Mutex);
    Entries.Reset();
    WriteLocked();
}
