#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersMutationEvidence.h"

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
        // Hierarchy via AddChildNode (see the door handler): AddNode + SetParent
        // leaves orphan root nodes with dangling parent names and produces
        // FixupRootNodeParentReferences warnings at compile time.
        RootNode->AddChildNode(BaseMeshNode);
        RootNode->AddChildNode(LidPivotNode);
        LidPivotNode->AddChildNode(LidMeshNode);
        RootNode->AddChildNode(TriggerNode);
        // create_chest_actor echoed `locked` straight back into its response and
        // stored it nowhere, so every chest came out unlocked no matter what was
        // asked for. configure_chest_properties below already does this properly;
        // the create path just never ran it. Compile first -- the member does not
        // exist on GeneratedClass until then, so an earlier CDO write finds nothing.
        bool bChestLockApplied = false;
        {
            FEdGraphPinType ChestBoolType;
            ChestBoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
            AddBlueprintVariableIfMissing(ChestBP, TEXT("bIsLocked"), ChestBoolType);
            McpSafeCompileBlueprint(ChestBP);
            if (UClass* GenClass = ChestBP->GeneratedClass)
            {
                UObject* ChestCDO = GenClass->GetDefaultObject();
                FProperty* LockProp = ChestCDO ? GenClass->FindPropertyByName(TEXT("bIsLocked")) : nullptr;
                FString ChestApplyError;
                bChestLockApplied = LockProp && ApplyJsonValueToProperty(
                    ChestCDO, LockProp, MakeShared<FJsonValueBoolean>(Locked), ChestApplyError);
            }
        }
        FBlueprintEditorUtils::MarkBlueprintAsModified(ChestBP);
        const bool bChestSaved = McpSafeAssetSave(ChestBP);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("chestPath"), ChestBP->GetPathName());
        Result->SetStringField(TEXT("blueprintPath"), ChestBP->GetPathName());
        Result->SetBoolField(TEXT("locked"), Locked);
        Result->SetBoolField(TEXT("propertiesApplied"), bChestLockApplied);
        TArray<FString> ChestChanges;
        ChestChanges.Add(TEXT("created chest blueprint"));
        ChestChanges.Add(TEXT("added chest components"));
        // Only claimed when the save actually reported success.
        if (bChestSaved) { ChestChanges.Add(TEXT("saved")); }
        AddMutationEvidence(Result, ChestBP, ChestChanges);
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
    if (ChestPath.IsEmpty())
    {
        // Without this the empty path reached LoadBlueprintAsset, which answered
        // "BLUEPRINT_NOT_FOUND: Empty request" -- naming neither the missing parameter nor the fact that
        // no lookup was ever attempted.
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter 'chestPath'"), TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(ChestPath, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }

    // Mirror the door guard: this branch used to mutate whatever blueprint it loaded, so an unrelated asset
    // silently received chest variables. create_chest_actor builds ChestBase/LidPivot/LidMesh, so requiring
    // two of those names discriminates rather than guesses.
    bool bHasChestBase = false;
    bool bHasLidMesh = false;
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (!Node)
            {
                continue;
            }
            const FString NodeName = Node->GetVariableName().ToString();
            if (NodeName == TEXT("ChestBase"))
            {
                bHasChestBase = true;
            }
            else if (NodeName == TEXT("LidMesh"))
            {
                bHasLidMesh = true;
            }
        }
    }
    if (!bHasChestBase || !bHasLidMesh)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s is not a chest blueprint: expected SCS nodes ChestBase and LidMesh. Run create_chest_actor first, or target the chest asset."), *ResolvedPath),
            TEXT("INVALID_OBJECT_TYPE"));
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
            ApplyToCdo(TEXT("LidOpenAngle"), MakeShared<FJsonValueNumber>(OpenAngle));
            ApplyToCdo(TEXT("OpenTime"), MakeShared<FJsonValueNumber>(OpenTime));
            ApplyToCdo(TEXT("bIsLocked"), MakeShared<FJsonValueBoolean>(Locked));
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("locked"), Locked);
    Result->SetNumberField(TEXT("openAngle"), OpenAngle);
    Result->SetNumberField(TEXT("openTime"), OpenTime);
    if (!LootTablePath.IsEmpty())
    {
        Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    }
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetBoolField(TEXT("propertiesApplied"), PropertiesNotApplied == 0);
    Result->SetStringField(TEXT("chestPath"), ChestPath);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    const bool bChestConfigSaved = McpSafeAssetSave(Blueprint);
    TArray<FString> ChestChanges;
    ChestChanges.Add(TEXT("configured chest properties"));
    if (!LootTablePath.IsEmpty())
    {
        // Only a path string was supplied. Nothing assigns it to the LootTable variable, so the changes
        // ledger must not claim an assignment that never happened.
        ChestChanges.Add(TEXT("lootTablePath supplied (not assigned)"));
    }
    if (bChestConfigSaved) { ChestChanges.Add(TEXT("saved")); }
    AddMutationEvidence(Result, Blueprint, ChestChanges);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Chest properties configured"), Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("configure_chest_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
