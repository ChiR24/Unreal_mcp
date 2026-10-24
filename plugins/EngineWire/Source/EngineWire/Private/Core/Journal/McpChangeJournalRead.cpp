// Copyright (c) 2024 EngineWire Contributors
//
// The journal's reader: changes_since lists what changed after a cursor, oldest first, with the cursor to send next; and
// a reply says once when the user changed something in the editor since the previous request through this server.

#include "Core/Journal/McpChangeJournal.h"

#include "Core/Journal/McpChangeJournalState.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

#include "GameFramework/Actor.h"

namespace McpChangeJournal
{
namespace
{
constexpr int32 DefaultChangeRows = 50;
constexpr int32 MaxChangeRows = 200;
constexpr int32 MaxWarningNames = 5;

const TCHAR* const JournalEventWords[] = {TEXT("added"), TEXT("modified"), TEXT("deleted"), TEXT("saved"),
                                          TEXT("renamed"), TEXT("removed"), TEXT("opened")};
const TCHAR* const JournalByWords[] = {TEXT("mcp"), TEXT("user"), TEXT("editor")};

FString JournalCursor(uint64 Seq)
{
    return JournalState().Session + TEXT(".") + LexToString(Seq);
}

// What a row calls its object: a live actor's reply name, a deleted one's label, an asset's package path.
FString JournalObjectName(const FJournalEntry& Entry)
{
    if (const AActor* Actor = Entry.Actor.Get())
    {
        return McpActorRef(Actor);
    }
    return Entry.Label.IsEmpty() ? Entry.Path : Entry.Label;
}

TArray<FString> JournalWords(uint8 Bits, const TCHAR* const* Words, int32 Count)
{
    TArray<FString> Out;
    for (int32 Bit = 0; Bit < Count; ++Bit)
    {
        if (Bits & (1 << Bit))
        {
            Out.Add(Words[Bit]);
        }
    }
    return Out;
}

// Where an actor stood when the journal first saw it and where it stands now, for each of location, rotation and scale
// that differs.
void AddTransform(const TSharedPtr<FJsonObject>& Row, const FTransform& From, const FTransform& To)
{
    TSharedPtr<FJsonObject> Moved = MakeShared<FJsonObject>();
    const auto Part = [&Moved](const TCHAR* Name, const TArray<TSharedPtr<FJsonValue>>& Before, const TArray<TSharedPtr<FJsonValue>>& After)
    {
        TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
        Change->SetArrayField(TEXT("from"), Before);
        Change->SetArrayField(TEXT("to"), After);
        Moved->SetObjectField(Name, Change);
    };
    if (!From.GetLocation().Equals(To.GetLocation(), 0.01))
    {
        Part(TEXT("location"), McpHandlerUtils::VectorToJsonArray(From.GetLocation()), McpHandlerUtils::VectorToJsonArray(To.GetLocation()));
    }
    if (!From.Rotator().Equals(To.Rotator(), 0.01))
    {
        Part(TEXT("rotation"), McpHandlerUtils::RotatorToJsonArray(From.Rotator()), McpHandlerUtils::RotatorToJsonArray(To.Rotator()));
    }
    if (!From.GetScale3D().Equals(To.GetScale3D(), 0.0001))
    {
        Part(TEXT("scale"), McpHandlerUtils::VectorToJsonArray(From.GetScale3D()), McpHandlerUtils::VectorToJsonArray(To.GetScale3D()));
    }
    if (Moved->Values.Num() > 0)
    {
        Row->SetObjectField(TEXT("transform"), Moved);
    }
}

TSharedPtr<FJsonObject> JournalRow(const FJournalEntry& Entry)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("object"), JournalObjectName(Entry));
    Row->SetStringField(TEXT("path"), Entry.Path);
    Row->SetStringField(TEXT("kind"), Entry.bActor ? TEXT("actor") : Entry.bLevel ? TEXT("level") : TEXT("asset"));
    Row->SetArrayField(TEXT("events"), McpHandlerUtils::ToJsonStringArray(
        JournalWords(Entry.Events, JournalEventWords, UE_ARRAY_COUNT(JournalEventWords))));
    Row->SetArrayField(TEXT("by"), McpHandlerUtils::ToJsonStringArray(
        JournalWords(Entry.By, JournalByWords, UE_ARRAY_COUNT(JournalByWords))));
    if (Entry.Properties.Num() > 0)
    {
        TArray<FString> Properties;
        for (const FName& Property : Entry.Properties)
        {
            Properties.Add(Property.ToString());
        }
        Row->SetArrayField(TEXT("properties"), McpHandlerUtils::ToJsonStringArray(Properties));
    }
    if (Entry.Actions.Num() > 0)
    {
        Row->SetArrayField(TEXT("actions"), McpHandlerUtils::ToJsonStringArray(Entry.Actions));
    }
    if (!Entry.OldPath.IsEmpty())
    {
        Row->SetStringField(TEXT("oldPath"), Entry.OldPath);
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FJournalValue& Value : Entry.Values)
    {
        if (Value.bTo && !(Value.bFrom && Value.From == Value.To))
        {
            TSharedPtr<FJsonObject> Changed = MakeShared<FJsonObject>();
            Changed->SetStringField(TEXT("property"), Value.Property.ToString());
            if (Value.bFrom)
            {
                Changed->SetStringField(TEXT("from"), Value.From);
            }
            Changed->SetStringField(TEXT("to"), Value.To);
            Values.Add(MakeShared<FJsonValueObject>(Changed));
        }
    }
    if (Values.Num() > 0)
    {
        Row->SetArrayField(TEXT("values"), Values);
    }
    const AActor* Actor = Entry.Actor.Get();
    if (Actor && Entry.bStart)
    {
        AddTransform(Row, Entry.Start, Actor->GetActorTransform());
    }
    return Row;
}

// since is "<session>.<seq>"; false with the refusal for a cursor this journal cannot answer.
bool ParseSince(const FString& Since, uint64& OutSeq, FString& OutCode, FString& OutMessage)
{
    const FJournalState& State = JournalState();
    FString Session;
    FString SeqText;
    if (!Since.Split(TEXT("."), &Session, &SeqText) || SeqText.IsEmpty() || !SeqText.IsNumeric())
    {
        OutCode = TEXT("INVALID_ARGUMENT");
        OutMessage = FString::Printf(TEXT("since '%s' is not a cursor changes_since returned."), *Since);
        return false;
    }
    OutSeq = FCString::Strtoui64(*SeqText, nullptr, 10);
    if (Session != State.Session || OutSeq > State.Seq || OutSeq < State.DroppedThrough)
    {
        OutCode = TEXT("CHANGE_CURSOR_EXPIRED");
        OutMessage = Session != State.Session || OutSeq > State.Seq
            ? FString(TEXT("That cursor is from before the editor restarted: read what you need again, then use the cursor this reply gives."))
            : FString(TEXT("More objects changed since that cursor than the journal keeps (4096): read what you need again, then use the cursor this reply gives."));
        return false;
    }
    return true;
}
} // namespace

bool HandleChangesSince(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                        TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FJournalState& State = JournalState();
    FString Since;
    double LimitNumber = DefaultChangeRows;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("since"), Since);
        Payload->TryGetNumberField(TEXT("limit"), LimitNumber);
    }
    const int32 Limit = FMath::Clamp(static_cast<int32>(LimitNumber), 1, MaxChangeRows);
    uint64 After = 0;
    FString Code;
    FString Message;
    if (!Since.IsEmpty() && !ParseSince(Since, After, Code, Message))
    {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetStringField(TEXT("cursor"), JournalCursor(State.Seq));
        Subsystem.SendAutomationResponse(Socket, RequestId, false, Message, Details, Code);
        return true;
    }
    TArray<const FJournalEntry*> Changed;
    for (const FJournalEntry& Entry : State.Entries)
    {
        if (Entry.LastSeq > After)
        {
            Changed.Add(&Entry);
        }
    }
    Changed.Sort([](const FJournalEntry& A, const FJournalEntry& B) { return A.LastSeq < B.LastSeq; });
    // With a cursor, the oldest changes after it and a cursor that continues from the last listed; without one, the newest.
    const bool bNewest = Since.IsEmpty();
    const int32 First = bNewest ? FMath::Max(0, Changed.Num() - Limit) : 0;
    const int32 End = bNewest ? Changed.Num() : FMath::Min(Changed.Num(), Limit);
    const bool bTruncated = !bNewest && Changed.Num() > End;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Index = First; Index < End; ++Index)
    {
        Rows.Add(MakeShared<FJsonValueObject>(JournalRow(*Changed[Index])));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("cursor"), JournalCursor(bTruncated ? Changed[End - 1]->LastSeq : State.Seq));
    Result->SetNumberField(TEXT("count"), Rows.Num());
    Result->SetArrayField(TEXT("changes"), Rows);
    if (bTruncated)
    {
        Result->SetBoolField(TEXT("truncated"), true);
    }
    Subsystem.SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%d change(s) %s%s."), Rows.Num(),
                        bNewest ? TEXT("kept (the most recent)") : *FString::Printf(TEXT("since %s"), *Since),
                        bTruncated ? TEXT("; more follow: call again with the cursor returned") : TEXT("")),
        Result);
    return true;
}

void AddUserChangeWarning(const FString& RequestId, const TSharedPtr<FJsonObject>& Result)
{
    FJournalState& State = JournalState();
    if (!Result.IsValid() || RequestId != State.RequestId || FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId) ||
        State.WarnUntil <= State.WarnedThrough)
    {
        return;
    }
    TArray<FString> Named;
    int32 Count = 0;
    for (const FJournalEntry& Entry : State.Entries)
    {
        if (Entry.LastUserSeq > State.WarnedThrough && Entry.LastUserSeq <= State.WarnUntil && ++Count <= MaxWarningNames)
        {
            Named.Add(JournalObjectName(Entry));
        }
    }
    State.WarnedThrough = State.WarnUntil;
    if (Count == 0)
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Warnings;
    const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
    if (Result->TryGetArrayField(TEXT("warnings"), Existing) && Existing)
    {
        Warnings = *Existing;
    }
    Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
        TEXT("The user changed %d object(s) in the editor since the previous call through this server (%s%s): inspect "
             "changes_since lists what and how."),
        Count, *FString::Join(Named, TEXT(", ")), Count > Named.Num() ? TEXT(", ...") : TEXT(""))));
    Result->SetArrayField(TEXT("warnings"), Warnings);
}

void AssetsChangedBy(const FString& RequestId, TArray<FString>& OutPackages)
{
    const FJournalState& State = JournalState();
    if (RequestId.IsEmpty() || RequestId != State.RequestId || State.Seq <= State.WarnUntil)
    {
        return;
    }
    // Inside a request every change kept is its own, so a row touched since it began is one it changed.
    for (const FJournalEntry& Entry : State.Entries)
    {
        if (!Entry.bActor && !Entry.bLevel && Entry.LastSeq > State.WarnUntil)
        {
            OutPackages.Add(Entry.Path);
        }
    }
}
} // namespace McpChangeJournal
