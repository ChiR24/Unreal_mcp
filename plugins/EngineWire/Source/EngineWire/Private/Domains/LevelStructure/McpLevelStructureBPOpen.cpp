#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Subsystems/AssetEditorSubsystem.h"

namespace McpLevelStructure
{

bool HandleOpenLevelBlueprint(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("No editor world available"), TEXT("NO_WORLD"));
        return true;
    }

    // Honor levelPath, and default to the persistent level: the level blueprint
    // belongs to a level, not to the world or to a sublevel selected in the
    // panel. Opening writes nothing, so a transient /Temp/ level is allowed
    // here (bAllowTransient) and the "unsaved level" branch below stays
    // reachable; node authoring still refuses a transient level.
    FString OpenLevelError;
    ULevel* Level = ResolveTargetLevelForBlueprintRequest(World, Payload, OpenLevelError, true);
    if (!Level)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            OpenLevelError, TEXT("LEVEL_NOT_OPEN"));
        return true;
    }

    // Check if the level is saved (has a valid package path)
    FString LevelPackageName = Level->GetOutermost()->GetName();
    const FString LevelName = FPackageName::GetShortName(LevelPackageName);
    bool bIsSavedLevel = !LevelPackageName.IsEmpty() && !LevelPackageName.StartsWith(TEXT("/Temp/"));

    // For unsaved levels, GetLevelScriptBlueprint(false) may fail to create the blueprint
    // because it requires a valid package path
    // Pass false to allow creation of Level Blueprint if it doesn't exist
    ULevelScriptBlueprint* LevelBP = Level->GetLevelScriptBlueprint(false);
    if (!LevelBP)
    {
        if (!bIsSavedLevel)
        {
            Subsystem->SendAutomationError(Socket, RequestId,
                TEXT("Level Blueprint unavailable for unsaved levels. Please save the level first."), TEXT("TRANSIENT_LEVEL"));
            return true;
        }
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Failed to get or create Level Blueprint"), TEXT("CREATE_FAILED"));
        return true;
    }

    bool bOpenedEditor = false;
    const bool bCanOpenEditorUi = !FApp::IsUnattended() && !IsRunningCommandlet();
    if (bCanOpenEditorUi)
    {
        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            bOpenedEditor = AssetEditorSubsystem->OpenEditorForAsset(LevelBP);
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, LevelBP);
    ResponseJson->SetStringField(TEXT("levelName"), LevelName);
    ResponseJson->SetBoolField(TEXT("openedEditor"), bOpenedEditor);
    ResponseJson->SetBoolField(TEXT("headlessSafeMode"), !bCanOpenEditorUi);

    // An editor that did not open still answered "Opened Level Blueprint".
    if (bCanOpenEditorUi && !bOpenedEditor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("The Level Blueprint of %s exists, but "
            "its editor did not open."), *LevelName), ResponseJson, TEXT("EDITOR_NOT_OPENED"));
        return true;
    }
    FString Message = bCanOpenEditorUi
        ? FString::Printf(TEXT("Opened Level Blueprint for: %s"), *LevelName)
        : FString::Printf(TEXT("Verified Level Blueprint for: %s (headless editor UI skipped)"), *LevelName);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
