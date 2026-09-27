#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"

namespace
{
constexpr int32 MaxExportedInteractionProperties = 100;

// The SCS component list plus the authored defaults (NewVariables and the
// generated class's own editable/visible properties, exported as text from the
// CDO) so a door/switch/chest readback shows what configure_* wrote.
void AddBlueprintStateInfo(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Result)
{
    TArray<TSharedPtr<FJsonValue>> Components;
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (!Node)
            {
                continue;
            }
            TSharedPtr<FJsonObject> Component = MakeShared<FJsonObject>();
            Component->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
            UClass* ComponentClass = Node->ComponentClass;
            if (!ComponentClass && Node->ComponentTemplate)
            {
                ComponentClass = Node->ComponentTemplate->GetClass();
            }
            Component->SetStringField(TEXT("class"), ComponentClass ? ComponentClass->GetName() : TEXT("Unknown"));
            Components.Add(MakeShared<FJsonValueObject>(Component));
        }
    }
    Result->SetArrayField(TEXT("components"), Components);

    TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
    int32 Count = 0;
    bool bTruncated = false;
    UClass* GeneratedClass = Blueprint->GeneratedClass;
    UObject* CDO = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
    auto Emit = [&Properties, &Count, &bTruncated](const FString& Name, const FString& Value)
    {
        if (Properties->HasField(Name))
        {
            return;
        }
        if (Count >= MaxExportedInteractionProperties)
        {
            bTruncated = true;
            return;
        }
        Properties->SetStringField(Name, Value);
        ++Count;
    };

    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        FProperty* Property = GeneratedClass ? GeneratedClass->FindPropertyByName(Var.VarName) : nullptr;
        FString Text = Var.DefaultValue;
        if (Property && CDO)
        {
            Text.Reset();
            Property->ExportText_InContainer(0, Text, CDO, nullptr, CDO, PPF_None);
        }
        Emit(Var.VarName.ToString(), Text);
    }
    if (GeneratedClass && CDO)
    {
        for (TFieldIterator<FProperty> It(GeneratedClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        {
            if (!It->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
            {
                continue;
            }
            FString Text;
            It->ExportText_InContainer(0, Text, CDO, nullptr, CDO, PPF_None);
            Emit(It->GetName(), Text);
        }
    }
    Result->SetObjectField(TEXT("properties"), Properties);
    Result->SetNumberField(TEXT("propertyCount"), Count);
    if (bTruncated)
    {
        Result->SetBoolField(TEXT("propertiesTruncated"), true);
    }
}

bool AddBlueprintInfo(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Result,
    const FString& Path,
    const FString& TypeField,
    const FString& PathField)
{
    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(Path, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
        return false;
    }

    Result->SetStringField(TEXT("assetType"), TypeField);
    // The resolved package path, so this read receipt carries the same canonical
    // assetPath handle every interaction mutation receipt now emits.
    Result->SetStringField(TEXT("assetPath"), ResolvedPath);
    Result->SetStringField(PathField, Path);
    if (PathField == TEXT("blueprintPath"))
    {
        Result->SetStringField(TEXT("blueprintName"), Blueprint->GetName());
    }
    AddBlueprintStateInfo(Blueprint, Result);
    return true;
}
}

namespace McpInteractionHandlers
{
bool HandleInteractionInfoAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction != TEXT("get_interaction_info"))
    {
        return false;
    }

    // blueprintPath wins, then actorName, then the kind-named paths.
    static const TCHAR* const PathFields[][2] = {
        {TEXT("blueprintPath"), TEXT("Blueprint")}, {TEXT("doorPath"), TEXT("Door")},
        {TEXT("switchPath"), TEXT("Switch")}, {TEXT("chestPath"), TEXT("Chest")},
        {TEXT("triggerPath"), TEXT("Trigger")}};
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    if (BlueprintPath.IsEmpty() && !ActorName.IsEmpty())
    {
        AActor* FoundActor = McpHandlerUtils::FindActorByName(ActorName);
        if (!FoundActor)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        Result->SetStringField(TEXT("assetType"), TEXT("Actor"));
        Result->SetStringField(TEXT("actorName"), FoundActor->GetName());
        Result->SetStringField(TEXT("actorClass"), FoundActor->GetClass()->GetName());
    }
    else
    {
        const TCHAR* const* Field = nullptr;
        FString Path;
        for (const TCHAR* const* Candidate : PathFields)
        {
            Path = GetJsonStringField(Payload, Candidate[0]);
            if (!Path.IsEmpty())
            {
                Field = Candidate;
                break;
            }
        }
        if (!Field)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                TEXT("At least one path parameter is required (blueprintPath, actorName, doorPath, switchPath, chestPath, or triggerPath)"),
                TEXT("MISSING_PARAMETER"));
            return true;
        }
        if (!AddBlueprintInfo(Subsystem, RequestId, RequestingSocket, Result, Path, Field[1], Field[0]))
        {
            return true;
        }
    }
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction info retrieved"), Result);
    return true;
}
}
