#include "Dom/JsonObject.h"
// McpAutomationBridge_LayerHandlers.cpp
// Actor Layer Management Handlers
//
// Layer operations including:
// - Create/Delete/Rename layers
// - Add/Remove actors to/from layers
// - Query actor layers and layer actors
// - Toggle layer visibility

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Layers/Layer.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Layers/LayersSubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpLayerHandlers, Log, All);

// ============================================================================
// Helper Functions
// ============================================================================
// NOTE: Uses consolidated JSON helpers from McpAutomationBridgeHelpers.h:
//   - GetJsonStringField(Obj, Field, Default)
//   - GetJsonBoolField(Obj, Field, Default)
// ============================================================================

#if WITH_EDITOR

// Helper to find actor by label or name
static AActor* FindActorByName(UWorld* World, const FString& ActorName)
{
    if (!World || ActorName.IsEmpty()) return nullptr;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            return *It;
        }
    }
    return nullptr;
}

// ============================================================================
// Sub-Handlers
// ============================================================================

static bool HandleCreateLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString LayerName = GetJsonStringField(Payload, TEXT("layerName"), TEXT(""));
    if (LayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("layerName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
    if (!LayersSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("LayersSubsystem not available"), nullptr);
        return true;
    }

    LayersSubsystem->CreateLayer(FName(*LayerName));

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("layerName"), LayerName);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created layer: %s"), *LayerName), ResponseJson);
    return true;
}

static bool HandleDeleteLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString LayerName = GetJsonStringField(Payload, TEXT("layerName"), TEXT(""));
    if (LayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("layerName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
    if (!LayersSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("LayersSubsystem not available"), nullptr);
        return true;
    }

    LayersSubsystem->DeleteLayer(FName(*LayerName));

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("layerName"), LayerName);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Deleted layer: %s"), *LayerName), ResponseJson);
    return true;
}

static bool HandleRenameLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString LayerName = GetJsonStringField(Payload, TEXT("layerName"), TEXT(""));
    FString NewName = GetJsonStringField(Payload, TEXT("newName"), TEXT(""));

    if (LayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("layerName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (NewName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("newName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
    if (!LayersSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("LayersSubsystem not available"), nullptr);
        return true;
    }

    bool bSuccess = LayersSubsystem->RenameLayer(FName(*LayerName), FName(*NewName));

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("oldName"), LayerName);
    ResponseJson->SetStringField(TEXT("newName"), NewName);
    ResponseJson->SetBoolField(TEXT("success"), bSuccess);

    if (bSuccess)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Renamed layer '%s' to '%s'"), *LayerName, *NewName), ResponseJson);
    }
    else
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to rename layer '%s' to '%s'"), *LayerName, *NewName), ResponseJson);
    }
    return true;
}

static bool HandleListLayers(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
    if (!LayersSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("LayersSubsystem not available"), nullptr);
        return true;
    }

    TArray<TWeakObjectPtr<ULayer>> Layers;
    LayersSubsystem->AddAllLayersTo(Layers);

    TArray<TSharedPtr<FJsonValue>> LayerArray;
    for (const TWeakObjectPtr<ULayer>& LayerPtr : Layers)
    {
        if (ULayer* Layer = LayerPtr.Get())
        {
            TSharedPtr<FJsonObject> LayerObj = MakeShareable(new FJsonObject());
            LayerObj->SetStringField(TEXT("name"), Layer->GetLayerName().ToString());
            LayerObj->SetBoolField(TEXT("visible"), Layer->IsVisible());
            LayerArray.Add(MakeShareable(new FJsonValueObject(LayerObj)));
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetArrayField(TEXT("layers"), LayerArray);
    ResponseJson->SetNumberField(TEXT("count"), LayerArray.Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d layers"), LayerArray.Num()), ResponseJson);
    return true;
}

static bool HandleAddActorToLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    FString LayerName = GetJsonStringField(Payload, TEXT("layerName"), TEXT(""));

    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (LayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("layerName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
    if (!LayersSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("LayersSubsystem not available"), nullptr);
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AActor* Actor = FindActorByName(World, ActorName);
    if (!Actor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    bool bSuccess = LayersSubsystem->AddActorToLayer(Actor, FName(*LayerName));

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("actorName"), ActorName);
    ResponseJson->SetStringField(TEXT("layerName"), LayerName);
    ResponseJson->SetBoolField(TEXT("success"), bSuccess);

    if (bSuccess)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Added actor '%s' to layer '%s'"), *ActorName, *LayerName), ResponseJson);
    }
    else
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to add actor '%s' to layer '%s'"), *ActorName, *LayerName), ResponseJson);
    }
    return true;
}

static bool HandleRemoveActorFromLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    FString LayerName = GetJsonStringField(Payload, TEXT("layerName"), TEXT(""));

    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }
    if (LayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("layerName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
    if (!LayersSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("LayersSubsystem not available"), nullptr);
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AActor* Actor = FindActorByName(World, ActorName);
    if (!Actor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    bool bSuccess = LayersSubsystem->RemoveActorFromLayer(Actor, FName(*LayerName));

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("actorName"), ActorName);
    ResponseJson->SetStringField(TEXT("layerName"), LayerName);
    ResponseJson->SetBoolField(TEXT("success"), bSuccess);

    if (bSuccess)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Removed actor '%s' from layer '%s'"), *ActorName, *LayerName), ResponseJson);
    }
    else
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to remove actor '%s' from layer '%s'"), *ActorName, *LayerName), ResponseJson);
    }
    return true;
}

static bool HandleGetActorLayers(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Editor world not available"), nullptr);
        return true;
    }

    AActor* Actor = FindActorByName(World, ActorName);
    if (!Actor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> LayerArray;
    for (const FName& LayerName : Actor->Layers)
    {
        LayerArray.Add(MakeShareable(new FJsonValueString(LayerName.ToString())));
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("actorName"), ActorName);
    ResponseJson->SetArrayField(TEXT("layers"), LayerArray);
    ResponseJson->SetNumberField(TEXT("count"), LayerArray.Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Actor '%s' has %d layers"), *ActorName, LayerArray.Num()), ResponseJson);
    return true;
}

static bool HandleSetLayerVisibility(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString LayerName = GetJsonStringField(Payload, TEXT("layerName"), TEXT(""));
    if (LayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("layerName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    bool bVisible = GetJsonBoolField(Payload, TEXT("visible"), true);

    ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
    if (!LayersSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("LayersSubsystem not available"), nullptr);
        return true;
    }

    LayersSubsystem->SetLayerVisibility(FName(*LayerName), bVisible);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("layerName"), LayerName);
    ResponseJson->SetBoolField(TEXT("visible"), bVisible);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set layer '%s' visibility to %s"), *LayerName, bVisible ? TEXT("true") : TEXT("false")), ResponseJson);
    return true;
}

static bool HandleGetLayerActors(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString LayerName = GetJsonStringField(Payload, TEXT("layerName"), TEXT(""));
    if (LayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("layerName is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();
    if (!LayersSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("LayersSubsystem not available"), nullptr);
        return true;
    }

    TArray<AActor*> Actors = LayersSubsystem->GetActorsFromLayer(FName(*LayerName));

    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : Actors)
    {
        if (!Actor) continue;

        TSharedPtr<FJsonObject> ActorObj = MakeShareable(new FJsonObject());
        ActorObj->SetStringField(TEXT("name"), Actor->GetName());
        ActorObj->SetStringField(TEXT("label"), Actor->GetActorLabel());
        ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
        ActorArray.Add(MakeShareable(new FJsonValueObject(ActorObj)));
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("layerName"), LayerName);
    ResponseJson->SetArrayField(TEXT("actors"), ActorArray);
    ResponseJson->SetNumberField(TEXT("count"), ActorArray.Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Layer '%s' has %d actors"), *LayerName, ActorArray.Num()), ResponseJson);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageLayersAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpLayerHandlers, Verbose, TEXT("HandleManageLayersAction: SubAction=%s"), *SubAction);

    if (SubAction == TEXT("create_layer"))
    {
        return HandleCreateLayer(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("delete_layer"))
    {
        return HandleDeleteLayer(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("rename_layer"))
    {
        return HandleRenameLayer(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("list_layers"))
    {
        return HandleListLayers(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("add_actor_to_layer"))
    {
        return HandleAddActorToLayer(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("remove_actor_from_layer"))
    {
        return HandleRemoveActorFromLayer(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("get_actor_layers"))
    {
        return HandleGetActorLayers(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_layer_visibility"))
    {
        return HandleSetLayerVisibility(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("get_layer_actors"))
    {
        return HandleGetLayerActors(this, RequestId, Payload, Socket);
    }

    // Unknown action
    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown layer subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;

#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Layer operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
