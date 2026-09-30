#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersMutationEvidence.h"

#include "EditorAssetLibrary.h"

namespace McpInteractionHandlers
{
namespace
{
FEdGraphPinType PinTypeFor(EInteractionVarType Type)
{
    FEdGraphPinType PinType;
    switch (Type)
    {
    case EInteractionVarType::Bool: PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean; break;
    case EInteractionVarType::Float:
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        break;
    case EInteractionVarType::Name: PinType.PinCategory = UEdGraphSchema_K2::PC_Name; break;
    case EInteractionVarType::SoftObject: PinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject; break;
    }
    return PinType;
}
}

int32 ApplyInteractionVars(UBlueprint* Blueprint, std::initializer_list<FInteractionVar> Vars)
{
    for (const FInteractionVar& Var : Vars)
    {
        const FName VarName(Var.Name);
        if (!Blueprint->NewVariables.ContainsByPredicate(
                [&VarName](const FBPVariableDescription& Existing) { return Existing.VarName == VarName; }))
        {
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, PinTypeFor(Var.Type));
        }
    }
    McpSafeCompileBlueprint(Blueprint);
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    int32 NotApplied = 0;
    for (const FInteractionVar& Var : Vars)
    {
        if (!Var.Value.IsValid())
        {
            continue;
        }
        FProperty* Prop = CDO ? CDO->GetClass()->FindPropertyByName(Var.Name) : nullptr;
        FString ApplyError;
        if (!Prop || !ApplyJsonValueToProperty(CDO, Prop, Var.Value, ApplyError))
        {
            ++NotApplied;
        }
    }
    return NotApplied;
}

void ConfigureInteractionShape(UObject* Template, float Size)
{
    UShapeComponent* Shape = Cast<UShapeComponent>(Template);
    if (!Shape || Size <= 0.0f)
    {
        return;
    }
    if (USphereComponent* Sphere = Cast<USphereComponent>(Shape))
    {
        Sphere->SetSphereRadius(Size);
    }
    else if (UBoxComponent* Box = Cast<UBoxComponent>(Shape))
    {
        Box->SetBoxExtent(FVector(Size));
    }
    else if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Shape))
    {
        Capsule->SetCapsuleSize(Size, Size * 2.0f);
    }
    Shape->SetCollisionProfileName(TEXT("OverlapAll"));
    Shape->SetGenerateOverlapEvents(true);
}

UBlueprint* CreateInteractableBlueprint(
    UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* DefaultFolder, const TCHAR* Noun, std::initializer_list<FInteractionNode> Nodes)
{
    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty())
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
        return nullptr;
    }
    FString PackageName;
    FString PathError;
    if (!ValidateAssetCreationPath(GetJsonStringField(Payload, TEXT("folder"), DefaultFolder), Name, PackageName, PathError))
    {
        Subsystem->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
        return nullptr;
    }
    // A package that is not loaded is recreated empty by CreatePackage, so the save below would overwrite the asset.
    if (McpAssetExists(PackageName))
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("A %s asset already exists at %s. Choose a different name or folder."), Noun, *PackageName),
            TEXT("ASSET_ALREADY_EXISTS"));
        return nullptr;
    }
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
        return nullptr;
    }
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AActor::StaticClass();
    UBlueprint* Blueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(
        UBlueprint::StaticClass(), Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone, nullptr, GWarn));
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Failed to create %s blueprint"), Noun), TEXT("BLUEPRINT_CREATE_FAILED"));
        return nullptr;
    }

    // AddChildNode, not AddNode + SetParent: AddNode registers a root and SetParent only records a parent
    // name, which left orphan roots and FixupRootNodeParentReferences warnings at compile time.
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    TMap<FString, USCS_Node*> Created;
    USCS_Node* Root = nullptr;
    for (const FInteractionNode& Spec : Nodes)
    {
        USCS_Node* Node = SCS->CreateNode(Spec.Class, Spec.Name);
        ConfigureInteractionShape(Node->ComponentTemplate, Spec.TriggerSize);
        if (!Root)
        {
            SCS->AddNode(Node);
            Root = Node;
        }
        else
        {
            USCS_Node** Parent = Spec.Parent ? Created.Find(Spec.Parent) : nullptr;
            (Parent ? *Parent : Root)->AddChildNode(Node);
        }
        Created.Add(Spec.Name, Node);
    }
    return Blueprint;
}

UBlueprint* LoadInteractableBlueprint(
    UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* PathField, const TCHAR* Noun, std::initializer_list<const TCHAR*> RequiredNodes)
{
    const FString Path = GetJsonStringField(Payload, PathField);
    if (Path.IsEmpty())
    {
        // An empty path used to reach LoadBlueprintAsset, which answered "BLUEPRINT_NOT_FOUND: Empty request".
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Missing required parameter '%s'"), PathField), TEXT("MISSING_PARAMETER"));
        return nullptr;
    }
    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(Path, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(Socket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
        return nullptr;
    }
    TArray<FString> Expected;
    bool bAllFound = true;
    for (const TCHAR* Required : RequiredNodes)
    {
        Expected.Add(Required);
        bAllFound = bAllFound && Blueprint->SimpleConstructionScript &&
            Blueprint->SimpleConstructionScript->GetAllNodes().ContainsByPredicate(
                [Required](const USCS_Node* Node) { return Node && Node->GetVariableName().ToString() == Required; });
    }
    if (!bAllFound)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("%s is not a %s blueprint: expected SCS nodes %s. Run create_%s_actor first, or target the %s asset."),
                *ResolvedPath, Noun, *FString::Join(Expected, TEXT(" and ")), Noun, Noun),
            TEXT("INVALID_OBJECT_TYPE"));
        return nullptr;
    }
    return Blueprint;
}

void SendInteractableResult(
    UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, UBlueprint* Blueprint,
    TSharedPtr<FJsonObject> Result, TArray<FString> Changes, const FString& Message)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (McpSafeAssetSave(Blueprint))
    {
        Changes.Add(TEXT("saved"));
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    AddMutationEvidence(Result, Blueprint, Changes);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, Result);
}
}
