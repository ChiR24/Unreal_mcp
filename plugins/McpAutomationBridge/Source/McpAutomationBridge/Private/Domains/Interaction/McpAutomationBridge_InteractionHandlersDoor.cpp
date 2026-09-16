#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersMutationEvidence.h"

namespace McpInteractionHandlers
{
namespace
{
// create_door_actor accepted openAngle/openTime/autoClose/autoCloseDelay/
// requiresKey, echoed all five back in its response, and stored NONE of them:
// the blueprint it produced had no variables at all, so the door could not open
// to any angle, auto-close, or lock. configure_door_properties in this same file
// already knew the recipe -- add the member variables, compile so GeneratedClass
// actually carries them, then write the CDO -- the create path just never ran it.
int32 ApplyDoorDefaults(UBlueprint* Blueprint, double OpenAngle, double OpenTime,
                        bool bAutoClose, double AutoCloseDelay, bool bRequiresKey,
                        bool bLocked)
{
    if (!Blueprint) { return -1; }
    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    AddBlueprintVariableIfMissing(Blueprint, TEXT("OpenAngle"), FloatType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("OpenTime"), FloatType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("AutoCloseDelay"), FloatType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("bAutoClose"), BoolType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("bRequiresKey"), BoolType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("bIsLocked"), BoolType);
    // Members do not exist on GeneratedClass until it is regenerated, so the CDO
    // write must come AFTER this compile or it silently resolves nothing.
    McpSafeCompileBlueprint(Blueprint);
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    if (!CDO) { return -1; }
    int32 NotApplied = 0;
    auto Apply = [CDO, &NotApplied](const TCHAR* PropertyName, const TSharedPtr<FJsonValue>& Value)
    {
        FProperty* Prop = CDO->GetClass()->FindPropertyByName(PropertyName);
        FString ApplyError;
        if (!Prop || !ApplyJsonValueToProperty(CDO, Prop, Value, ApplyError)) { ++NotApplied; }
    };
    Apply(TEXT("OpenAngle"), MakeShared<FJsonValueNumber>(OpenAngle));
    Apply(TEXT("OpenTime"), MakeShared<FJsonValueNumber>(OpenTime));
    Apply(TEXT("AutoCloseDelay"), MakeShared<FJsonValueNumber>(AutoCloseDelay));
    Apply(TEXT("bAutoClose"), MakeShared<FJsonValueBoolean>(bAutoClose));
    Apply(TEXT("bRequiresKey"), MakeShared<FJsonValueBoolean>(bRequiresKey));
    Apply(TEXT("bIsLocked"), MakeShared<FJsonValueBoolean>(bLocked));
    return NotApplied;
}
}

bool HandleDoorAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("create_door_actor"))
    {
        const FString Name = GetJsonStringField(Payload, TEXT("name"));
        const FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interactables"));
        const double OpenAngle = GetJsonNumberField(Payload, TEXT("openAngle"), 90.0);
        const double OpenTime = GetJsonNumberField(Payload, TEXT("openTime"), 0.5);
        const bool AutoClose = GetJsonBoolField(Payload, TEXT("autoClose"), false);
        const double AutoCloseDelay = GetJsonNumberField(Payload, TEXT("autoCloseDelay"), 3.0);
        const bool RequiresKey = GetJsonBoolField(Payload, TEXT("requiresKey"), false);
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

        const FString SanitizedName = SanitizeAssetName(Name);
        const FString ObjectPath = PackageName + TEXT(".") + SanitizedName;
        if (UBlueprint* ExistingDoorBP = LoadObject<UBlueprint>(nullptr, *ObjectPath))
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetNumberField(TEXT("openAngle"), OpenAngle);
            Result->SetNumberField(TEXT("openTime"), OpenTime);
            Result->SetBoolField(TEXT("autoClose"), AutoClose);
            Result->SetNumberField(TEXT("autoCloseDelay"), AutoCloseDelay);
            Result->SetBoolField(TEXT("requiresKey"), RequiresKey);
            Result->SetBoolField(TEXT("alreadyExisted"), true);
            McpHandlerUtils::AddVerification(Result, ExistingDoorBP);
            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Door actor already exists"), Result);
            return true;
        }

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
            return true;
        }
        if (FindObject<UBlueprint>(Package, *SanitizedName))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Door blueprint already exists in package but could not be loaded"), TEXT("ASSET_ALREADY_EXISTS"));
            return true;
        }

        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        Factory->ParentClass = AActor::StaticClass();
        UBlueprint* DoorBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *SanitizedName, RF_Public | RF_Standalone, nullptr, GWarn));
        if (!DoorBP)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create door blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
            return true;
        }

        USimpleConstructionScript* SCS = DoorBP->SimpleConstructionScript;
        USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
        USCS_Node* PivotNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("DoorPivot"));
        USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("DoorMesh"));
        USCS_Node* CollisionNode = SCS->CreateNode(UBoxComponent::StaticClass(), TEXT("InteractionTrigger"));
        if (UBoxComponent* CollisionTemplate = Cast<UBoxComponent>(CollisionNode->ComponentTemplate))
        {
            CollisionTemplate->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
            CollisionTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
            CollisionTemplate->SetGenerateOverlapEvents(true);
        }
        SCS->AddNode(RootNode);
        // Build the hierarchy with AddChildNode, not AddNode + SetParent:
        //   AddNode() registers the node as a ROOT and SetParent() only writes a
        //   textual parent reference, leaving the node orphaned in RootNodes with
        //   a dangling parent name. At compile time that produced
        //   "FixupRootNodeParentReferences: Couldn't find inherited parent component
        //   'Root' for 'DoorPivot'..." warnings and a broken attach hierarchy.
        //   AddChildNode() moves the node under its parent (ChildNodes + AllNodes).
        RootNode->AddChildNode(PivotNode);
        PivotNode->AddChildNode(MeshNode);
        RootNode->AddChildNode(CollisionNode);
        const bool Locked = GetJsonBoolField(Payload, TEXT("locked"), false);
        const int32 DoorNotApplied = ApplyDoorDefaults(
            DoorBP, OpenAngle, OpenTime, AutoClose, AutoCloseDelay, RequiresKey, Locked);
        FBlueprintEditorUtils::MarkBlueprintAsModified(DoorBP);
        const bool bDoorSaved = McpSafeAssetSave(DoorBP);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("openAngle"), OpenAngle);
        Result->SetNumberField(TEXT("openTime"), OpenTime);
        Result->SetBoolField(TEXT("autoClose"), AutoClose);
        Result->SetNumberField(TEXT("autoCloseDelay"), AutoCloseDelay);
        Result->SetBoolField(TEXT("requiresKey"), RequiresKey);
        Result->SetBoolField(TEXT("propertiesApplied"), DoorNotApplied == 0);
        McpHandlerUtils::AddVerification(Result, DoorBP);
        TArray<FString> DoorChanges;
        DoorChanges.Add(TEXT("created door blueprint"));
        DoorChanges.Add(TEXT("added door components"));
        if (bDoorSaved) { DoorChanges.Add(TEXT("saved")); }
        AddMutationEvidence(Result, DoorBP, DoorChanges);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Door actor created"), Result);
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("create_door_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
        return true;
    }

    if (SubAction != TEXT("configure_door_properties"))
    {
        return false;
    }

    const FString DoorPath = GetJsonStringField(Payload, TEXT("doorPath"));
    const double OpenAngle = GetJsonNumberField(Payload, TEXT("openAngle"), 90.0);
    const double OpenTime = GetJsonNumberField(Payload, TEXT("openTime"), 0.5);
    const bool Locked = GetJsonBoolField(Payload, TEXT("locked"), false);
#if WITH_EDITOR
    if (DoorPath.IsEmpty())
    {
        // Without this the empty path reached LoadBlueprintAsset, which answered
        // "BLUEPRINT_NOT_FOUND: Empty request" -- naming neither the missing parameter nor the fact that
        // no lookup was ever attempted.
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter 'doorPath'"), TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(DoorPath, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }

    // This branch used to mutate whatever blueprint it loaded, so aiming it at
    // a chest authored door variables onto the chest, compiled it, and stalled
    // into a -32001 timeout. create_door_actor builds DoorPivot + DoorMesh
    // (see the create branch above) while a chest builds ChestBase/LidPivot/
    // LidMesh, so requiring both names discriminates rather than guesses.
    bool bHasDoorPivot = false;
    bool bHasDoorMesh = false;
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (!Node)
            {
                continue;
            }
            const FString NodeName = Node->GetVariableName().ToString();
            if (NodeName == TEXT("DoorPivot"))
            {
                bHasDoorPivot = true;
            }
            else if (NodeName == TEXT("DoorMesh"))
            {
                bHasDoorMesh = true;
            }
        }
    }
    if (!bHasDoorPivot || !bHasDoorMesh)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s is not a door blueprint: expected SCS nodes DoorPivot and DoorMesh. Run create_door_actor first, or target the door asset."), *ResolvedPath),
            TEXT("INVALID_OBJECT_TYPE"));
        return true;
    }

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    AddBlueprintVariableIfMissing(Blueprint, TEXT("OpenAngle"), FloatType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("OpenTime"), FloatType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("bIsLocked"), BoolType);
    AddBlueprintVariableIfMissing(Blueprint, TEXT("bIsOpen"), BoolType);

    // Class members added above do not exist on GeneratedClass until it is regenerated, so a CDO write made
    // before this compile looked up nothing, did nothing, and still reported "configured": true with the
    // requested values echoed. create_combat_asset compiles first for exactly this reason. The apply result
    // is now checked instead of being discarded into a local error string.
    McpSafeCompileBlueprint(Blueprint);

    int32 PropertiesNotApplied = 0;
    if (Blueprint->GeneratedClass)
    {
        UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
        if (CDO)
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
            ApplyToCdo(TEXT("OpenAngle"), MakeShared<FJsonValueNumber>(OpenAngle));
            ApplyToCdo(TEXT("OpenTime"), MakeShared<FJsonValueNumber>(OpenTime));
            ApplyToCdo(TEXT("bIsLocked"), MakeShared<FJsonValueBoolean>(Locked));
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("openAngle"), OpenAngle);
    Result->SetNumberField(TEXT("openTime"), OpenTime);
    Result->SetBoolField(TEXT("locked"), Locked);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetBoolField(TEXT("propertiesApplied"), PropertiesNotApplied == 0);
    Result->SetStringField(TEXT("doorPath"), DoorPath);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    const bool bDoorConfigSaved = McpSafeAssetSave(Blueprint);
    TArray<FString> DoorChanges;
    DoorChanges.Add(TEXT("configured door properties"));
    if (bDoorConfigSaved) { DoorChanges.Add(TEXT("saved")); }
    AddMutationEvidence(Result, Blueprint, DoorChanges);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Door properties configured"), Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("configure_door_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
