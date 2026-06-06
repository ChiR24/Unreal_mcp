#include "McpAutomationBridge_InteractionHandlersPrivate.h"

namespace McpInteractionHandlers
{
bool HandleSwitchAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("create_switch_actor"))
    {
        const FString Name = GetJsonStringField(Payload, TEXT("name"));
        const FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interactables"));
        const FString SwitchType = GetJsonStringField(Payload, TEXT("switchType"), TEXT("button"));
        if (Name.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
            return true;
        }
#if WITH_EDITOR
        FString PackageName;
        FString PathError;
        if (!ValidateAssetCreationPath(Folder, Name, PackageName, PathError))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, PathError, TEXT("INVALID_PATH"));
            return true;
        }
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
            return true;
        }

        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        Factory->ParentClass = AActor::StaticClass();
        const FString SanitizedName = SanitizeAssetName(Name);
        UBlueprint* SwitchBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *SanitizedName, RF_Public | RF_Standalone, nullptr, GWarn));
        if (!SwitchBP)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create switch blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
            return true;
        }

        USimpleConstructionScript* SCS = SwitchBP->SimpleConstructionScript;
        USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
        USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("SwitchMesh"));
        USCS_Node* TriggerNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionTrigger"));
        if (USphereComponent* TriggerTemplate = Cast<USphereComponent>(TriggerNode->ComponentTemplate))
        {
            TriggerTemplate->SetSphereRadius(100.0f);
            TriggerTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
            TriggerTemplate->SetGenerateOverlapEvents(true);
        }
        SCS->AddNode(RootNode);
        SCS->AddNode(MeshNode);
        MeshNode->SetParent(RootNode);
        SCS->AddNode(TriggerNode);
        TriggerNode->SetParent(RootNode);
        FBlueprintEditorUtils::MarkBlueprintAsModified(SwitchBP);
        McpSafeAssetSave(SwitchBP);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("switchPath"), SwitchBP->GetPathName());
        Result->SetStringField(TEXT("blueprintPath"), SwitchBP->GetPathName());
        Result->SetStringField(TEXT("switchType"), SwitchType);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Switch actor created"), Result);
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("create_switch_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
        return true;
    }

    if (SubAction != TEXT("configure_switch_properties"))
    {
        return false;
    }

    const FString SwitchPath = GetJsonStringField(Payload, TEXT("switchPath"));
    const FString SwitchType = GetJsonStringField(Payload, TEXT("switchType"), TEXT("button"));
    const bool CanToggle = GetJsonBoolField(Payload, TEXT("canToggle"), true);
    const double ResetTime = GetJsonNumberField(Payload, TEXT("resetTime"), 0.0);
#if WITH_EDITOR
    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(SwitchPath, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    AddBlueprintVariableIfMissing(Blueprint, TEXT("SwitchType"), NameType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("bCanToggle"), BoolType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("bIsActivated"), BoolType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("ResetTime"), FloatType);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("switchType"), SwitchType);
    Result->SetBoolField(TEXT("canToggle"), CanToggle);
    Result->SetNumberField(TEXT("resetTime"), ResetTime);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetStringField(TEXT("switchPath"), SwitchPath);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Switch properties configured"), Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("configure_switch_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
