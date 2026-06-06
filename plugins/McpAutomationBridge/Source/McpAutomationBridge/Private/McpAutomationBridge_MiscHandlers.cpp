#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_MiscHandlersSupport.h"
#include "McpHandlerUtils.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY(LogMcpMiscHandlers);

#if WITH_EDITOR
namespace McpMiscHandlers
{
UWorld* GetEditorWorld()
{
    if (GEditor)
    {
        return GEditor->GetEditorWorldContext().World();
    }
    return nullptr;
}

FString GetStringField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, const FString& Default)
{
    FString Value = Default;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(FieldName, Value);
    }
    return Value;
}

double GetNumberField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, double Default)
{
    double Value = Default;
    if (Payload.IsValid())
    {
        Payload->TryGetNumberField(FieldName, Value);
    }
    return Value;
}

bool GetBoolField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, bool Default)
{
    bool Value = Default;
    if (Payload.IsValid())
    {
        Payload->TryGetBoolField(FieldName, Value);
    }
    return Value;
}

FVector GetVectorField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, FVector Default)
{
    return ExtractVectorField(Payload, *FieldName, Default);
}

FRotator GetRotatorField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, FRotator Default)
{
    return ExtractRotatorField(Payload, *FieldName, Default);
}
}
#endif

bool UMcpAutomationBridgeSubsystem::HandleMiscAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    using namespace McpMiscHandlers;

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));
    if (SubAction.IsEmpty())
    {
        SubAction = Action;
    }

    UE_LOG(LogMcpMiscHandlers, Verbose, TEXT("HandleMiscAction: %s"), *SubAction);

    struct FMiscRoute
    {
        const TCHAR* Name;
        bool (*Handler)(UMcpAutomationBridgeSubsystem*, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);
    };

    static const FMiscRoute Routes[] = {
        {TEXT("create_post_process_volume"), HandleCreatePostProcessVolume},
        {TEXT("create_camera"), HandleCreateCamera},
        {TEXT("set_camera_fov"), HandleSetCameraFOV},
        {TEXT("set_viewport_resolution"), HandleSetViewportResolution},
        {TEXT("set_game_speed"), HandleSetGameSpeed},
        {TEXT("create_bookmark"), HandleCreateBookmark},
        {TEXT("create_spline_component"), HandleCreateSplineComponent},
        {TEXT("set_replication"), HandleSetReplication},
        {TEXT("create_replicated_variable"), HandleCreateReplicatedVariable},
        {TEXT("set_net_update_frequency"), HandleSetNetUpdateFrequency},
        {TEXT("create_rpc"), HandleCreateRPC},
        {TEXT("configure_net_cull_distance"), HandleConfigureNetCullDistance},
    };

    for (const FMiscRoute& Route : Routes)
    {
        if (SubAction == Route.Name)
        {
            return Route.Handler(this, RequestId, Payload, Socket);
        }
    }

    return false;
#else
    return false;
#endif
}
