#include "McpAutomationBridge_InteractionHandlersPrivate.h"

namespace McpInteractionHandlers
{
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
        SCS->AddNode(PivotNode);
        PivotNode->SetParent(RootNode);
        SCS->AddNode(MeshNode);
        MeshNode->SetParent(PivotNode);
        SCS->AddNode(CollisionNode);
        CollisionNode->SetParent(RootNode);
        FBlueprintEditorUtils::MarkBlueprintAsModified(DoorBP);
        McpSafeAssetSave(DoorBP);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("openAngle"), OpenAngle);
        Result->SetNumberField(TEXT("openTime"), OpenTime);
        Result->SetBoolField(TEXT("autoClose"), AutoClose);
        Result->SetNumberField(TEXT("autoCloseDelay"), AutoCloseDelay);
        Result->SetBoolField(TEXT("requiresKey"), RequiresKey);
        McpHandlerUtils::AddVerification(Result, DoorBP);
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
    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(DoorPath, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
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

    if (Blueprint->GeneratedClass)
    {
        UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
        if (CDO)
        {
            if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("OpenAngle")))
            {
                FString ApplyError;
                ApplyJsonValueToProperty(CDO, Prop, MakeShared<FJsonValueNumber>(OpenAngle), ApplyError);
            }
            if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("OpenTime")))
            {
                FString ApplyError;
                ApplyJsonValueToProperty(CDO, Prop, MakeShared<FJsonValueNumber>(OpenTime), ApplyError);
            }
            if (FProperty* Prop = CDO->GetClass()->FindPropertyByName(TEXT("bIsLocked")))
            {
                FString ApplyError;
                ApplyJsonValueToProperty(CDO, Prop, MakeShared<FJsonValueBoolean>(Locked), ApplyError);
            }
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("openAngle"), OpenAngle);
    Result->SetNumberField(TEXT("openTime"), OpenTime);
    Result->SetBoolField(TEXT("locked"), Locked);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetStringField(TEXT("doorPath"), DoorPath);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Door properties configured"), Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("configure_door_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
