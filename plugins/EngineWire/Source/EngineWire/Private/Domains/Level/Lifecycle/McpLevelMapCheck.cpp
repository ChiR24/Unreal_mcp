// run_map_check: the editor's own Map Check (Build > Map Check) on the open level, answered as rows instead of a
// Message Log tab: each message's severity, text, the actor it names and the map error it is (the key of the engine's
// own guidance for that error). Unbuilt lighting, meshes with no mesh, overlapping stationary lights, volumes with a bad
// brush and the rest of what the engine checks are only readable here.
#include "Domains/Level/McpLevelActions.h"

#include "Editor.h"
#include "IMessageLogListing.h"
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
    TArray<TSharedPtr<FJsonValue>> Rows;
    TMap<FString, int32> Counts;
    for (const TSharedRef<FTokenizedMessage>& Message : Listing->GetFilteredMessages())
    {
        const TSharedPtr<FJsonObject> Row = MapCheckRow(*Message);
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
    for (const TPair<const TCHAR*, const TCHAR*>& Field : {TPair<const TCHAR*, const TCHAR*>(TEXT("error"), TEXT("errors")),
                                                           TPair<const TCHAR*, const TCHAR*>(TEXT("warning"), TEXT("warnings")),
                                                           TPair<const TCHAR*, const TCHAR*>(TEXT("performance"), TEXT("performanceWarnings")),
                                                           TPair<const TCHAR*, const TCHAR*>(TEXT("info"), TEXT("notes"))})
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
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Map Check of '%s': %d error(s), %d warning(s), %d performance warning(s), %d note(s)."),
                        *World->GetName(), Counts.FindRef(TEXT("error")), Counts.FindRef(TEXT("warning")),
                        Counts.FindRef(TEXT("performance")), Counts.FindRef(TEXT("info"))),
        Result);
    return true;
}
} // namespace McpLevelHandlers
