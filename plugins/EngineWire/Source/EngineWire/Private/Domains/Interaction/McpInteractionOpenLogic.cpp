// The logic a door or a chest made by create_interactable runs: it opens while a pawn is near (recipe
// Interaction/OpenOnApproach, written in through McpBlueprintBehaviour::Author). They were components and variables
// only, so a door made over MCP never opened.
#include "Domains/Interaction/McpInteractionPrivate.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"

namespace McpInteractionHandlers
{
bool AuthorOpenOnApproach(UEngineWireSubsystem* Subsystem, const FString& RequestId,
                          TSharedPtr<FMcpBridgeWebSocket> Socket, UBlueprint* Blueprint, const TCHAR* Pivot,
                          const TCHAR* AngleVariable, const TCHAR* RotatorPin, const TSharedPtr<FJsonObject>& Result)
{
    TMap<FString, TSharedPtr<FJsonValue>> Values;
    Values.Add(TEXT("Pivot"), MakeShared<FJsonValueString>(Pivot));
    Values.Add(TEXT("AngleVariable"), MakeShared<FJsonValueString>(AngleVariable));
    Values.Add(TEXT("RotatorPin"), MakeShared<FJsonValueString>(RotatorPin));
    FString Error;
    const TSharedPtr<FJsonObject> Recipe =
        McpBlueprintBehaviour::LoadRecipe(TEXT("Interaction"), TEXT("OpenOnApproach"), Values, Error);
    if (!Recipe)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, Error, nullptr, TEXT("INVALID_RECIPE"));
        return false;
    }
    const McpBlueprintBehaviour::FAuthorResult Authored =
        McpBlueprintBehaviour::Author(*Subsystem, RequestId, Socket, Blueprint->GetPathName(), Recipe);
    if (!Authored.bApplied)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, Authored.Message, Authored.Report, Authored.ErrorCode);
        return false;
    }
    bool bCompiled = false;
    Authored.Report->TryGetBoolField(TEXT("compiled"), bCompiled);
    Result->SetBoolField(TEXT("opensOnApproach"), true);
    Result->SetBoolField(TEXT("compiled"), bCompiled);
    return true;
}
}
