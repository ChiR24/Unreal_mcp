#include "McpAutomationBridge_PCGHandlersPrivate.h"

#if WITH_EDITOR && MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandleExecutePCGGraph(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave)
{
    UWorld* World = GetPCGEditorWorld();
    if (!World)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not resolve the editor world for PCG execution."), TEXT("WORLD_NOT_FOUND"));
        return true;
    }

    FString Error;
    FString GraphPath;
    UPCGGraph* Graph = nullptr;
    const FString GraphRawPath = GetFirstStringField(Payload, {TEXT("graphPath"), TEXT("assetPath")});
    if (!GraphRawPath.IsEmpty())
    {
        Graph = LoadPCGGraph(GraphRawPath, GraphPath, Error);
        if (!Graph)
        {
            Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("ASSET_NOT_FOUND"));
            return true;
        }
    }

    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    const FString ComponentPath = GetJsonStringField(Payload, TEXT("componentPath"));
    const FString ComponentSelector = !ComponentPath.IsEmpty() ? ComponentPath : ComponentName;
    const bool bCreateComponent = GetJsonBoolField(Payload, TEXT("createComponent"), false);
    AActor* Actor = nullptr;
    UPCGComponent* Component = FindPCGComponent(World, ActorName, ComponentSelector, Actor);
    if (!Component && !bCreateComponent && !HasPCGComponentSelector(ActorName, ComponentSelector))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("execute_pcg_graph requires actorName, componentName, or componentPath when createComponent is false."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!Component && bCreateComponent)
    {
        Actor = FindPCGActor(World, ActorName);
        if (!Actor)
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("createComponent requires an existing actorName."), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        Component = CreatePCGComponent(Actor, ComponentName);
    }
    if (!Component)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not resolve a PCG component. Provide actorName/componentName, or actorName with createComponent=true."), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    Component->Modify();
    if (Graph)
    {
        Component->SetGraphLocal(Graph);
    }

    const bool bForceGenerate = GetJsonBoolField(Payload, TEXT("force"), true);
    const FPCGTaskId TaskId = Component->GenerateLocalGetTaskId(bForceGenerate);
    if (TaskId == InvalidPCGTaskId)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("PCG generation was not scheduled. The component may already be generating, be up to date with force=false, have no graph, or have invalid bounds."), TEXT("GENERATION_NOT_SCHEDULED"));
        return true;
    }

    bool bLevelSaved = false;
    if (!SaveEditorWorldIfRequested(World, bSave, bLevelSaved, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    Result->SetStringField(TEXT("actorName"), Actor ? Actor->GetName() : FString());
    Result->SetStringField(TEXT("componentName"), Component->GetName());
    Result->SetStringField(TEXT("componentPath"), Component->GetPathName());
    Result->SetNumberField(TEXT("taskId"), static_cast<double>(TaskId));
    Result->SetBoolField(TEXT("force"), bForceGenerate);
    Result->SetBoolField(TEXT("saved"), bLevelSaved);
    McpHandlerUtils::AddVerification(Result, Component);
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG graph generation started."), Result);
    return true;
}

bool HandleSetComponentGridSize(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, int32 GridSize, bool bSave)
{
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    const FString ComponentPath = GetJsonStringField(Payload, TEXT("componentPath"));
    const FString ComponentSelector = !ComponentPath.IsEmpty() ? ComponentPath : ComponentName;
    if (!HasPCGComponentSelector(ActorName, ComponentSelector))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("component-scoped partition grid size requires actorName, componentName, or componentPath."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    AActor* Actor = nullptr;
    UPCGComponent* Component = FindPCGComponent(World, ActorName, ComponentSelector, Actor);
    if (!Component)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not resolve a PCG component for component-scoped grid size."), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    const uint32 PreviousGridSize = Component->GetGenerationGridSize();
    Component->Modify();
    Component->SetGenerationGridSize(static_cast<uint32>(GridSize));
    Component->PostEditChange();

    FString Error;
    bool bLevelSaved = false;
    if (!SaveEditorWorldIfRequested(World, bSave, bLevelSaved, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("scope"), TEXT("component"));
    Result->SetStringField(TEXT("actorName"), Actor ? Actor->GetName() : FString());
    Result->SetStringField(TEXT("componentName"), Component->GetName());
    Result->SetStringField(TEXT("componentPath"), Component->GetPathName());
    Result->SetNumberField(TEXT("previousGridSize"), PreviousGridSize);
    Result->SetNumberField(TEXT("gridSize"), GridSize);
    Result->SetBoolField(TEXT("saved"), bLevelSaved);
    McpHandlerUtils::AddVerification(Result, Component);
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG component generation grid size updated."), Result);
    return true;
}

bool HandleSetWorldGridSize(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, int32 GridSize, bool bSave)
{
    APCGWorldActor* PCGWorldActor = PCGHelpers::GetPCGWorldActor(World);
    if (!PCGWorldActor)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not resolve or create the PCG world actor."), TEXT("PCG_WORLD_ACTOR_NOT_FOUND"));
        return true;
    }

    const uint32 PreviousGridSize = PCGWorldActor->PartitionGridSize;
    FProperty* PartitionGridSizeProperty = FindFProperty<FProperty>(APCGWorldActor::StaticClass(), GET_MEMBER_NAME_CHECKED(APCGWorldActor, PartitionGridSize));
    PCGWorldActor->Modify();
    if (PartitionGridSizeProperty)
    {
        PCGWorldActor->PreEditChange(PartitionGridSizeProperty);
    }
    PCGWorldActor->PartitionGridSize = static_cast<uint32>(GridSize);
    if (PartitionGridSizeProperty)
    {
        FPropertyChangedEvent PropertyChangedEvent(PartitionGridSizeProperty, EPropertyChangeType::ValueSet);
        PCGWorldActor->PostEditChangeProperty(PropertyChangedEvent);
    }
    else
    {
        PCGWorldActor->PostEditChange();
    }

    FString Error;
    bool bLevelSaved = false;
    if (!SaveEditorWorldIfRequested(World, bSave, bLevelSaved, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("scope"), TEXT("world"));
    Result->SetNumberField(TEXT("previousGridSize"), PreviousGridSize);
    Result->SetNumberField(TEXT("gridSize"), PCGWorldActor->PartitionGridSize);
    Result->SetBoolField(TEXT("saved"), bLevelSaved);
    McpHandlerUtils::AddVerification(Result, PCGWorldActor);
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG partition grid size updated."), Result);
    return true;
}

bool HandleSetPCGPartitionGridSize(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave)
{
    UWorld* World = GetPCGEditorWorld();
    if (!World)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not resolve the editor world for PCG partition grid size."), TEXT("WORLD_NOT_FOUND"));
        return true;
    }

    const int32 GridSize = GetJsonIntField(Payload, TEXT("gridSize"), 0);
    if (GridSize <= 0)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("set_pcg_partition_grid_size requires a positive gridSize."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString Scope = TEXT("world");
    if (Payload.IsValid() && Payload->HasField(TEXT("scope")) && !Payload->TryGetStringField(TEXT("scope"), Scope))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("set_pcg_partition_grid_size scope must be a string: 'world' or 'component'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    Scope = Scope.ToLower();
    if (Scope != TEXT("world") && Scope != TEXT("component"))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("set_pcg_partition_grid_size scope must be 'world' or 'component'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (Scope == TEXT("component"))
    {
        return HandleSetComponentGridSize(Bridge, RequestId, Payload, Socket, World, GridSize, bSave);
    }

    return HandleSetWorldGridSize(Bridge, RequestId, Socket, World, GridSize, bSave);
}
}
#endif
