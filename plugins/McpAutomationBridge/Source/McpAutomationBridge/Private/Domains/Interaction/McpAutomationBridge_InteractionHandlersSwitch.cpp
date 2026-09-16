#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersMutationEvidence.h"

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
        // Hierarchy via AddChildNode (see the door handler).
        RootNode->AddChildNode(MeshNode);
        RootNode->AddChildNode(TriggerNode);
        FBlueprintEditorUtils::MarkBlueprintAsModified(SwitchBP);
        const bool bSwitchSaved = McpSafeAssetSave(SwitchBP);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("switchPath"), SwitchBP->GetPathName());
        Result->SetStringField(TEXT("blueprintPath"), SwitchBP->GetPathName());
        Result->SetStringField(TEXT("switchType"), SwitchType);
        TArray<FString> SwitchChanges;
        SwitchChanges.Add(TEXT("created switch blueprint"));
        if (bSwitchSaved) { SwitchChanges.Add(TEXT("saved")); }
        AddMutationEvidence(Result, SwitchBP, SwitchChanges);
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
    if (SwitchPath.IsEmpty())
    {
        // Without this the empty path reached LoadBlueprintAsset, which answered
        // "BLUEPRINT_NOT_FOUND: Empty request" -- naming neither the missing parameter nor the fact that
        // no lookup was ever attempted.
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter 'switchPath'"), TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(SwitchPath, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }

    // Mirror the door guard: this branch used to mutate whatever blueprint it loaded, so an unrelated asset
    // silently received switch variables. create_switch_actor builds a SwitchMesh component, so requiring
    // that name discriminates rather than guesses.
    bool bHasSwitchMesh = false;
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->GetVariableName().ToString() == TEXT("SwitchMesh"))
            {
                bHasSwitchMesh = true;
                break;
            }
        }
    }
    if (!bHasSwitchMesh)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s is not a switch blueprint: expected the SCS node SwitchMesh. Run create_switch_actor first, or target the switch asset."), *ResolvedPath),
            TEXT("INVALID_OBJECT_TYPE"));
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

    // Class members added above do not exist on GeneratedClass until it is regenerated, so a CDO write made
    // before this compile looked up nothing, did nothing, and still reported "configured": true with the
    // requested values echoed. create_combat_asset compiles first for exactly this reason. The apply result
    // is now checked instead of being discarded into a local error string.
    McpSafeCompileBlueprint(Blueprint);

    int32 PropertiesNotApplied = 0;
    if (Blueprint->GeneratedClass)
    {
        if (UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject())
        {
            auto ApplyToCdo = [CDO, &PropertiesNotApplied](const TCHAR* PropertyName, const TSharedPtr<FJsonValue>& Value)
            {
                FProperty* Prop = CDO->GetClass()->FindPropertyByName(PropertyName);
                FString ApplyError;
                if (!Prop || !ApplyJsonValueToProperty(CDO, Prop, Value, ApplyError))
                {
                    ++PropertiesNotApplied;
                }
            };
            ApplyToCdo(TEXT("SwitchType"), MakeShared<FJsonValueString>(SwitchType));
            ApplyToCdo(TEXT("bCanToggle"), MakeShared<FJsonValueBoolean>(CanToggle));
            ApplyToCdo(TEXT("ResetTime"), MakeShared<FJsonValueNumber>(ResetTime));
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("switchType"), SwitchType);
    Result->SetBoolField(TEXT("canToggle"), CanToggle);
    Result->SetNumberField(TEXT("resetTime"), ResetTime);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetBoolField(TEXT("propertiesApplied"), PropertiesNotApplied == 0);
    Result->SetStringField(TEXT("switchPath"), SwitchPath);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    const bool bSwitchConfigSaved = McpSafeAssetSave(Blueprint);
    TArray<FString> SwitchChanges;
    SwitchChanges.Add(TEXT("configured switch properties"));
    if (bSwitchConfigSaved) { SwitchChanges.Add(TEXT("saved")); }
    AddMutationEvidence(Result, Blueprint, SwitchChanges);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Switch properties configured"), Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("configure_switch_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
