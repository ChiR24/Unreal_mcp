#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandleExecutePCGGraph(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not resolve the editor world for PCG execution."), TEXT("WORLD_NOT_FOUND"));
        return true;
    }

    FString Error;
    FString GraphPath;
    UPCGGraph* Graph = nullptr;
    const FString GraphRawPath = McpGetFirstStringField(Payload, {TEXT("graphPath"), TEXT("assetPath")});
    if (!GraphRawPath.IsEmpty())
    {
        Graph = LoadPCGAsset<UPCGGraph>(GraphRawPath, TEXT("PCG graph"), GraphPath, Error);
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
    bool bCreatedComponent = false;
    UPCGComponent* Component = FindPCGComponent(World, ActorName, ComponentSelector, Actor);
    if (!Component && !bCreateComponent && ActorName.IsEmpty() && ComponentSelector.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("execute_pcg_graph requires actorName, componentName, or componentPath when createComponent is false."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!Component && bCreateComponent)
    {
        // A new component has no graph and never schedules: it was made, refused GENERATION_NOT_SCHEDULED, and left.
        if (!Graph)
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("createComponent needs graphPath: a new PCG component has no "
                "graph to generate. Nothing was created."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Actor = FindActorByNameInWorldForMcp(World, ActorName, true);
        if (!Actor)
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("createComponent requires an existing actorName."), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        // NewObject over a subobject of another class with that name collides; refuse instead.
        if (!ComponentName.IsEmpty() && FindObject<UObject>(Actor, *ComponentName))
        {
            Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s already has a %s named %s that is no PCG component; pick another componentName."),
                *McpActorRef(Actor), *FindObject<UObject>(Actor, *ComponentName)->GetClass()->GetName(), *ComponentName), TEXT("NAME_TAKEN"));
            return true;
        }
        Component = CreatePCGComponent(Actor, ComponentName);
        bCreatedComponent = Component != nullptr;
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
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    const FPCGTaskId TaskId = Component->GenerateLocalGetTaskId(bForceGenerate);
#else
    // UE 5.2 has no GenerateLocalGetTaskId. Its GenerateLocal schedules nothing while a generation runs and leaves
    // that generation's id in place, so a component that was already generating counts as not scheduled.
    const bool bWasGenerating = Component->IsGenerating();
    Component->GenerateLocal(bForceGenerate);
    const FPCGTaskId TaskId = bWasGenerating ? InvalidPCGTaskId : Component->GetGenerationTaskId();
#endif
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
    // The graph the component runs when none was sent (it answered an empty string).
    Result->SetStringField(TEXT("graphPath"), !GraphPath.IsEmpty() || !Component->GetGraph() ? GraphPath : Component->GetGraph()->GetPathName());
    // The owner's label (it named the object, and nothing when the component was found by path).
    Result->SetStringField(TEXT("actorName"), McpActorRef(Component->GetOwner()));
    Result->SetStringField(TEXT("componentName"), Component->GetName());
    Result->SetBoolField(TEXT("createdComponent"), bCreatedComponent);
    Result->SetStringField(TEXT("componentPath"), Component->GetPathName());
    Result->SetNumberField(TEXT("taskId"), static_cast<double>(TaskId));
    Result->SetBoolField(TEXT("force"), bForceGenerate);
    Result->SetBoolField(TEXT("saved"), bLevelSaved);
    McpHandlerUtils::AddVerification(Result, Component);
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG graph generation started."), Result);
    return true;
}

bool HandleSetComponentGridSize(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, int32 GridSize, bool bSave)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 3
    // The PCG component has a generation grid size from UE 5.3 on.
    Bridge->SendAutomationError(Socket, RequestId, TEXT("UE 5.2's PCG component has no generation grid size; use scope 'world' to set the PCG world actor's partition grid size."), TEXT("NOT_SUPPORTED"));
    return true;
#else
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    const FString ComponentPath = GetJsonStringField(Payload, TEXT("componentPath"));
    const FString ComponentSelector = !ComponentPath.IsEmpty() ? ComponentPath : ComponentName;
    if (ActorName.IsEmpty() && ComponentSelector.IsEmpty())
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
    // The owner's label, as execute_pcg_graph answers it: the object name, and nothing when found by path.
    Result->SetStringField(TEXT("actorName"), McpActorRef(Component->GetOwner()));
    Result->SetStringField(TEXT("componentName"), Component->GetName());
    Result->SetStringField(TEXT("componentPath"), Component->GetPathName());
    Result->SetNumberField(TEXT("previousGridSize"), PreviousGridSize);
    Result->SetNumberField(TEXT("gridSize"), GridSize);
    Result->SetBoolField(TEXT("saved"), bLevelSaved);
    McpHandlerUtils::AddVerification(Result, Component);
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG component generation grid size updated."), Result);
    return true;
#endif
}

bool HandleSetWorldGridSize(UEngineWireSubsystem* Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, int32 GridSize, bool bSave)
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
    PCGWorldActor->PreEditChange(PartitionGridSizeProperty);
    PCGWorldActor->PartitionGridSize = static_cast<uint32>(GridSize);
    FPropertyChangedEvent PropertyChangedEvent(PartitionGridSizeProperty, EPropertyChangeType::ValueSet);
    PCGWorldActor->PostEditChangeProperty(PropertyChangedEvent);

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

bool HandleSetPCGPartitionGridSize(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
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
    if (Payload->HasField(TEXT("scope")) && !Payload->TryGetStringField(TEXT("scope"), Scope))
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
    // Clamped to 400 by the engine only from 5.7: below it a small world grid re-created the partition actors of every
    // partitioned component on that grid.
    if (GridSize < 400)
    {
        Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("A world partition grid of %d cm is under the 400 cm "
            "the engine allows; give gridSize 400 or more (centimetres)."), GridSize), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    return HandleSetWorldGridSize(Bridge, RequestId, Socket, World, GridSize, bSave);
}
}
#endif
