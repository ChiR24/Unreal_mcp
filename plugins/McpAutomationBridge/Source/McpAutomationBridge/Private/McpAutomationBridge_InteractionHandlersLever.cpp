#include "McpAutomationBridge_InteractionHandlersPrivate.h"

namespace McpInteractionHandlers
{
bool HandleLeverAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction != TEXT("create_lever_actor"))
    {
        return false;
    }

    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    const FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interactables"));
    if (Name.IsEmpty())
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
        return true;
    }

#if WITH_EDITOR
    UPackage* Package = CreatePackage(*MakeLegacyPackageName(Folder, Name, TEXT("/Game/Interactables")));
    if (!Package)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
        return true;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AActor::StaticClass();
    UBlueprint* LeverBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));
    if (!LeverBP)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create lever blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
        return true;
    }

    USimpleConstructionScript* SCS = LeverBP->SimpleConstructionScript;
    USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
    USCS_Node* BaseMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("LeverBase"));
    USCS_Node* PivotNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("LeverPivot"));
    USCS_Node* HandleMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("LeverHandle"));
    USCS_Node* TriggerNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionTrigger"));
    if (USphereComponent* TriggerTemplate = Cast<USphereComponent>(TriggerNode->ComponentTemplate))
    {
        TriggerTemplate->SetSphereRadius(100.0f);
        TriggerTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
        TriggerTemplate->SetGenerateOverlapEvents(true);
    }
    SCS->AddNode(RootNode);
    SCS->AddNode(BaseMeshNode);
    BaseMeshNode->SetParent(RootNode);
    SCS->AddNode(PivotNode);
    PivotNode->SetParent(RootNode);
    SCS->AddNode(HandleMeshNode);
    HandleMeshNode->SetParent(PivotNode);
    SCS->AddNode(TriggerNode);
    TriggerNode->SetParent(RootNode);
    FBlueprintEditorUtils::MarkBlueprintAsModified(LeverBP);
    McpSafeAssetSave(LeverBP);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("leverPath"), LeverBP->GetPathName());
    Result->SetStringField(TEXT("blueprintPath"), LeverBP->GetPathName());
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lever actor created"), Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("create_lever_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
