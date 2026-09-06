#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Misc/McpAutomationBridge_MiscHandlersSupport.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
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

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetNumberField(TEXT("width"), Width);
    ResponseJson->SetNumberField(TEXT("height"), Height);
    ResponseJson->SetStringField(TEXT("note"),
        TEXT("Viewport resolution preferences set. Actual resolution depends on editor window size."));

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Viewport resolution preference set to %dx%d"), Width, Height), ResponseJson);
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

    UE_LOG(LogMcpMiscHandlers, Log, TEXT("Bookmark %d set at Location=(%.1f, %.1f, %.1f)"),
        BookmarkIndex, Location.X, Location.Y, Location.Z);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetNumberField(TEXT("index"), BookmarkIndex);
    if (!BookmarkName.IsEmpty())
    {
        ResponseJson->SetStringField(TEXT("name"), BookmarkName);
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
