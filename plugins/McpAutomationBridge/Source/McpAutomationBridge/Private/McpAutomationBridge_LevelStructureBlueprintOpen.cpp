#include "McpAutomationBridge_LevelStructureActions.h"
#include "McpAutomationBridge_LevelStructureEditorWorld.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Misc/App.h"
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#endif

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleOpenLevelBlueprint(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Get the persistent level (which is the level that has a level blueprint)
    ULevel* PersistentLevel = World->PersistentLevel;
    if (!PersistentLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No persistent level available"), nullptr);
        return true;
    }

    // Check if the level is saved (has a valid package path)
    FString LevelPackageName = World->GetOutermost()->GetName();
    bool bIsSavedLevel = !LevelPackageName.IsEmpty() && !LevelPackageName.StartsWith(TEXT("/Temp/"));

    // For unsaved levels, GetLevelScriptBlueprint(false) may fail to create the blueprint
    // because it requires a valid package path
    // Pass false to allow creation of Level Blueprint if it doesn't exist
    ULevelScriptBlueprint* LevelBP = PersistentLevel->GetLevelScriptBlueprint(false);
    if (!LevelBP)
    {
        // Try to create the level blueprint manually for unsaved levels
        if (!bIsSavedLevel)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                TEXT("Level Blueprint unavailable for unsaved levels. Please save the level first."), nullptr);
            return true;
        }
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to get or create Level Blueprint"), nullptr);
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
    ResponseJson->SetStringField(TEXT("levelName"), World->GetMapName());
    ResponseJson->SetBoolField(TEXT("openedEditor"), bOpenedEditor);
    ResponseJson->SetBoolField(TEXT("headlessSafeMode"), !bCanOpenEditorUi);

    FString Message = bCanOpenEditorUi
        ? FString::Printf(TEXT("Opened Level Blueprint for: %s"), *World->GetMapName())
        : FString::Printf(TEXT("Verified Level Blueprint for: %s (headless editor UI skipped)"), *World->GetMapName());
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
#endif
