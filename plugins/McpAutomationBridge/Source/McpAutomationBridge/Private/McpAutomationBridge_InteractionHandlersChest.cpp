#include "McpAutomationBridge_InteractionHandlersPrivate.h"

namespace McpInteractionHandlers
{
bool HandleChestAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("create_chest_actor"))
    {
        const FString Name = GetJsonStringField(Payload, TEXT("name"));
        const FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interactables"));
        const bool Locked = GetJsonBoolField(Payload, TEXT("locked"), false);
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
        UBlueprint* ChestBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));
        if (!ChestBP)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create chest blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
            return true;
        }

        USimpleConstructionScript* SCS = ChestBP->SimpleConstructionScript;
        USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
        USCS_Node* BaseMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("ChestBase"));
        USCS_Node* LidPivotNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("LidPivot"));
        USCS_Node* LidMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("LidMesh"));
        USCS_Node* TriggerNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionTrigger"));
        if (USphereComponent* TriggerTemplate = Cast<USphereComponent>(TriggerNode->ComponentTemplate))
        {
            TriggerTemplate->SetSphereRadius(150.0f);
            TriggerTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
            TriggerTemplate->SetGenerateOverlapEvents(true);
        }
        SCS->AddNode(RootNode);
        SCS->AddNode(BaseMeshNode);
        BaseMeshNode->SetParent(RootNode);
        SCS->AddNode(LidPivotNode);
        LidPivotNode->SetParent(RootNode);
        SCS->AddNode(LidMeshNode);
        LidMeshNode->SetParent(LidPivotNode);
        SCS->AddNode(TriggerNode);
        TriggerNode->SetParent(RootNode);
        FBlueprintEditorUtils::MarkBlueprintAsModified(ChestBP);
        McpSafeAssetSave(ChestBP);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("chestPath"), ChestBP->GetPathName());
        Result->SetStringField(TEXT("blueprintPath"), ChestBP->GetPathName());
        Result->SetBoolField(TEXT("locked"), Locked);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Chest actor created"), Result);
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("create_chest_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
        return true;
    }

    if (SubAction != TEXT("configure_chest_properties"))
    {
        return false;
    }

    const FString ChestPath = GetJsonStringField(Payload, TEXT("chestPath"));
    const bool Locked = GetJsonBoolField(Payload, TEXT("locked"), false);
    const double OpenAngle = GetJsonNumberField(Payload, TEXT("openAngle"), 90.0);
    const double OpenTime = GetJsonNumberField(Payload, TEXT("openTime"), 0.5);
    const FString LootTablePath = GetJsonStringField(Payload, TEXT("lootTablePath"));
#if WITH_EDITOR
    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(ChestPath, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
    AddBlueprintVariableIfMissing(Blueprint, TEXT("bIsLocked"), BoolType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("bIsOpen"), BoolType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("LidOpenAngle"), FloatType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("OpenTime"), FloatType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("LootTable"), SoftObjectType);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("locked"), Locked);
    Result->SetNumberField(TEXT("openAngle"), OpenAngle);
    Result->SetNumberField(TEXT("openTime"), OpenTime);
    if (!LootTablePath.IsEmpty())
    {
        Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    }
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetStringField(TEXT("chestPath"), ChestPath);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Chest properties configured"), Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("configure_chest_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
