// run_map_check: the editor's own Map Check (Build > Map Check) on the open level, answered as rows instead of a
// Message Log tab: each message's severity, text, the actor it names and the map error it is (the key of the engine's
// own guidance for that error). Unbuilt lighting, meshes with no mesh, overlapping stationary lights, volumes with a bad
// brush and the rest of what the engine checks are only readable here.
#include "Domains/Level/Lifecycle/McpLevelMapCheck.h"

#include "Domains/Level/McpLevelActions.h"
#include "Editor.h"
#include "IMessageLogListing.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "MessageLogModule.h"
#include "Misc/UObjectToken.h"
#include "Modules/ModuleManager.h"

namespace McpLevelHandlers
{
namespace
{
constexpr int32 MaxMapCheckRows = 200;

FString MapCheckSeverity(const EMessageSeverity::Type Severity)
{
    switch (Severity)
    {
    case EMessageSeverity::Error:
        return TEXT("error");
    case EMessageSeverity::PerformanceWarning:
        return TEXT("performance");
    case EMessageSeverity::Warning:
        return TEXT("warning");
    case EMessageSeverity::Info:
        return TEXT("info");
    default:
        return TEXT("error"); // the removed CriticalError
    }
}

TSharedPtr<FJsonObject> MapCheckRow(const FTokenizedMessage& Message)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("severity"), MapCheckSeverity(Message.GetSeverity()));
    Row->SetStringField(TEXT("message"), Message.ToText().ToString());
    for (const TSharedRef<IMessageToken>& Token : Message.GetMessageTokens())
    {
        if (Token->GetType() == EMessageToken::Object && !Row->HasField(TEXT("actor")))
        {
            const UObject* Target = StaticCastSharedRef<FUObjectToken>(Token)->GetObject().Get();
            const AActor* Actor = Cast<AActor>(Target);
            Actor = Actor ? Actor : (Target ? Target->GetTypedOuter<AActor>() : nullptr);
            if (Actor)
            {
                Row->SetStringField(TEXT("actor"), Actor->GetActorLabel());
                Row->SetStringField(TEXT("actorPath"), Actor->GetPathName());
            }
        }
        else if (Token->GetType() == EMessageToken::Documentation && !Row->HasField(TEXT("code")))
        {
            // A map error token is a documentation link whose excerpt is the error's name (FMapErrors).
            const FString Code = StaticCastSharedRef<FDocumentationToken>(Token)->GetPreviewExcerptName();
            if (!Code.IsEmpty())
            {
                Row->SetStringField(TEXT("code"), Code);
            }
        }
    }
    return Row;
}
} // namespace

bool HandleRunMapCheckAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                             TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    (void)Payload;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("No level is open in the editor."), TEXT("NO_WORLD"));
        return true;
    }
    // A new page of the MapCheck log, filled by the same check Build > Map Check runs; no Message Log tab opens. Map_Check
    // itself is private to the editor engine, so the command its own build path sends is used (5.0-5.8 alike).
    GEditor->Exec(World, TEXT("MAP CHECK DONTDISPLAYDIALOG"), *GLog);
    const TSharedRef<IMessageLogListing> Listing =
        FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog")).GetLogListing(TEXT("MapCheck"));
    const TArray<TSharedRef<FTokenizedMessage>>& Messages = Listing->GetFilteredMessages();
    // The check ends its page with a summary note ("Map check complete: ...": its severity mark and text alone), which
    // is no finding: a clean level answered one note. Findings name an object or a map error.
    int32 Listed = Messages.Num();
    if (Listed > 0 && Messages.Last()->GetSeverity() == EMessageSeverity::Info &&
        !Messages.Last()->GetMessageTokens().ContainsByPredicate([](const TSharedRef<IMessageToken>& Token)
            { return Token->GetType() != EMessageToken::Severity && Token->GetType() != EMessageToken::Text; }))
    {
        --Listed;
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    TMap<FString, int32> Counts;
    for (int32 Index = 0; Index < Listed; ++Index)
    {
        const TSharedPtr<FJsonObject> Row = MapCheckRow(*Messages[Index]);
        ++Counts.FindOrAdd(Row->GetStringField(TEXT("severity")));
        if (Rows.Num() < MaxMapCheckRows)
        {
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelPath"), World->GetOutermost()->GetName());
    Result->SetArrayField(TEXT("messages"), Rows);
    int32 Total = 0;
    // Counts, not "warnings": a reply's warnings are the engine lines a call logged, and they took that field over.
    for (const TPair<const TCHAR*, const TCHAR*>& Field : {TPair<const TCHAR*, const TCHAR*>(TEXT("error"), TEXT("errorCount")),
                                                           TPair<const TCHAR*, const TCHAR*>(TEXT("warning"), TEXT("warningCount")),
                                                           TPair<const TCHAR*, const TCHAR*>(TEXT("performance"), TEXT("performanceWarningCount")),
                                                           TPair<const TCHAR*, const TCHAR*>(TEXT("info"), TEXT("noteCount"))})
    {
        const int32 Count = Counts.FindRef(Field.Key);
        Result->SetNumberField(Field.Value, Count);
        Total += Count;
    }
    Result->SetNumberField(TEXT("total"), Total);
    if (Total > Rows.Num())
    {
        Result->SetBoolField(TEXT("truncated"), true);
    }
    // The listing applies the Map Check tab's severity filters; the log itself counts every message, the summary too.
    const int32 Hidden = FMessageLog(TEXT("MapCheck")).NumMessages(EMessageSeverity::Info) - 1 - Total;
    if (Hidden > 0)
    {
        Result->SetNumberField(TEXT("hiddenByLogFilters"), Hidden);
    }
    // Each message is a row already: its copy in the output log is no engine warning of this call.
    Subsystem.ForgetCapturedMessages([](const FString& Line) { return Line.StartsWith(TEXT("[MapCheck]")); });
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Map Check of '%s': %d error(s), %d warning(s), %d performance warning(s), %d note(s)."),
                        *World->GetName(), Counts.FindRef(TEXT("error")), Counts.FindRef(TEXT("warning")),
                        Counts.FindRef(TEXT("performance")), Counts.FindRef(TEXT("info"))),
        Result);
    return true;
}

// Called while a reply is built: its captured log lines were taken before, so the [MapCheck] copies of these messages
// in the output log are no warning of the call.
void MapCheckActors(const TArray<AActor*>& Actors, TArray<FString>& OutIssues)
{
    if (Actors.Num() == 0)
    {
        return;
    }
    const TSharedRef<IMessageLogListing> Listing =
        FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog")).GetLogListing(TEXT("MapCheck"));
    const int32 Before = Listing->GetFilteredMessages().Num();
    for (AActor* Actor : Actors)
    {
        if (IsValid(Actor))
        {
            Actor->CheckForErrors();
        }
    }
    // ponytail: the listing applies the Map Check tab's severity filters, so a severity hidden there is not reported.
    const TArray<TSharedRef<FTokenizedMessage>>& Messages = Listing->GetFilteredMessages();
    for (int32 Index = Before; Index < Messages.Num(); ++Index)
    {
        const FTokenizedMessage& Message = *Messages[Index];
        if (Message.GetSeverity() == EMessageSeverity::Info)
        {
            continue;
        }
        // Named by its label, as every call addresses it (the message's object token is its internal name), with the
        // map error's name when it has one.
        FString Text;
        for (const TSharedRef<IMessageToken>& Token : Message.GetMessageTokens())
        {
            Text += Token->GetType() == EMessageToken::Text ? Token->ToText().ToString() : FString();
        }
        const TSharedPtr<FJsonObject> Row = MapCheckRow(Message);
        FString Actor, Code;
        Row->TryGetStringField(TEXT("actor"), Actor);
        Row->TryGetStringField(TEXT("code"), Code);
        const FString Kind = Code.IsEmpty() ? FString(TEXT("Map Check")) : FString::Printf(TEXT("Map Check (%s)"), *Code);
        OutIssues.Add(Actor.IsEmpty() ? Kind + TEXT(": ") + Message.ToText().ToString().TrimStartAndEnd()
                                      : FString::Printf(TEXT("'%s': %s: %s"), *Actor, *Kind, *Text.TrimStartAndEnd()));
    }
}
} // namespace McpLevelHandlers
