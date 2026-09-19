#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Misc/McpAutomationBridge_MiscHandlersSupport.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/BookMark.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Modules/ModuleManager.h"

namespace McpMiscHandlers
{
bool HandleSetViewportResolution(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    int32 Width = static_cast<int32>(GetJsonNumberField(Payload, TEXT("width"), 1920.0));
    int32 Height = static_cast<int32>(GetJsonNumberField(Payload, TEXT("height"), 1080.0));

    if (Width <= 0 || Height <= 0)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Invalid resolution dimensions"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    // The handler validated the numbers and then stored them NOWHERE, replying
    // "Viewport resolution preference set to WxH" for a call that changed
    // nothing at all. The resolution the editor actually persists is the PIE
    // "New Editor Window" size on ULevelEditorPlaySettings, so write that and
    // say which setting moved.
    ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
    if (!PlaySettings)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Level editor play settings are not available"), nullptr, TEXT("SETTINGS_UNAVAILABLE"));
        return true;
    }
    PlaySettings->NewWindowWidth = Width;
    PlaySettings->NewWindowHeight = Height;
    PlaySettings->PostEditChange();
    PlaySettings->SaveConfig();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetNumberField(TEXT("width"), PlaySettings->NewWindowWidth);
    ResponseJson->SetNumberField(TEXT("height"), PlaySettings->NewWindowHeight);
    ResponseJson->SetStringField(TEXT("appliedTo"), TEXT("LevelEditorPlaySettings.NewWindowWidth/NewWindowHeight"));
    ResponseJson->SetStringField(TEXT("note"),
        TEXT("Persisted PIE 'New Editor Window' size. A docked level viewport is sized by the editor layout and cannot be set here."));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("PIE window resolution set to %dx%d"), Width, Height), ResponseJson);
    return true;
}

bool HandleSetGameSpeed(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    double Speed = GetJsonNumberField(Payload, TEXT("speed"), 1.0);

    if (Speed < 0.0 || Speed > 100.0)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Speed must be between 0.0 and 100.0"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = nullptr;
    if (GEditor && GEditor->PlayWorld)
    {
        World = GEditor->PlayWorld;
    }
    else
    {
        World = GetEditorWorld();
    }

    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("No world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (!WorldSettings)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("World settings not available"), nullptr, TEXT("NO_WORLD_SETTINGS"));
        return true;
    }

    WorldSettings->SetTimeDilation(static_cast<float>(Speed));

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetNumberField(TEXT("speed"), Speed);
    ResponseJson->SetNumberField(TEXT("actualTimeDilation"), WorldSettings->TimeDilation);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Game speed set to %.2fx"), Speed), ResponseJson);
    return true;
}

bool HandleCreateBookmark(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    int32 BookmarkIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("index"), 0.0));
    FString BookmarkName = GetJsonStringField(Payload, TEXT("name"), TEXT(""));
    FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

    if (BookmarkIndex < 0 || BookmarkIndex > 9)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Bookmark index must be between 0 and 9"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Editor world not available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    // This used to log the coordinates and reply "Created bookmark at index N"
    // without creating one -- the level's bookmark slot stayed empty and
    // pressing the matching number key in the viewport did nothing.
    // AWorldSettings owns the bookmark array; GetOrAddBookmark allocates the
    // slot and UBookMark is the concrete class carrying Location/Rotation.
    AWorldSettings* WorldSettings = World->GetWorldSettings();
    UBookMark* Bookmark = WorldSettings
        ? Cast<UBookMark>(WorldSettings->GetOrAddBookmark(static_cast<uint32>(BookmarkIndex), true))
        : nullptr;
    if (!Bookmark)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Could not allocate bookmark slot %d on the level's WorldSettings"), BookmarkIndex),
            nullptr, TEXT("BOOKMARK_FAILED"));
        return true;
    }
    WorldSettings->Modify();
    Bookmark->Location = Location;
    Bookmark->Rotation = Rotation;
    WorldSettings->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetNumberField(TEXT("index"), BookmarkIndex);
    ResponseJson->SetBoolField(TEXT("bookmarkStored"), true);
    if (!BookmarkName.IsEmpty())
    {
        ResponseJson->SetStringField(TEXT("name"), BookmarkName);
        ResponseJson->SetStringField(TEXT("nameNote"),
            TEXT("UBookMark stores only a transform; the name is echoed back but is not persisted on the bookmark."));
    }

    TSharedPtr<FJsonObject> LocationJson = McpHandlerUtils::CreateResultObject();
    LocationJson->SetNumberField(TEXT("x"), Location.X);
    LocationJson->SetNumberField(TEXT("y"), Location.Y);
    LocationJson->SetNumberField(TEXT("z"), Location.Z);
    ResponseJson->SetObjectField(TEXT("location"), LocationJson);

    TSharedPtr<FJsonObject> RotationJson = McpHandlerUtils::CreateResultObject();
    RotationJson->SetNumberField(TEXT("pitch"), Rotation.Pitch);
    RotationJson->SetNumberField(TEXT("yaw"), Rotation.Yaw);
    RotationJson->SetNumberField(TEXT("roll"), Rotation.Roll);
    ResponseJson->SetObjectField(TEXT("rotation"), RotationJson);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created bookmark at index %d"), BookmarkIndex), ResponseJson);
    return true;
}
}
#endif
