#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

// Blueprint snapshot builder shared with blueprint_get (Domains/Blueprint). It is
// declared locally because the Blueprint domain's aggregate declarations header
// depends on its own include order and would drag graph-editing types in here.
namespace McpBlueprintHandlers {
TSharedPtr<FJsonObject> FMcpAutomationBridge_BuildBlueprintSnapshot(UBlueprint *Blueprint,
                                                                    const FString &NormalizedPath);
}

namespace McpEnvironmentHandlers {

namespace {
TSharedPtr<FJsonObject> McpMakeComponentEntry(const FString &Name, const UClass *Class,
                                              const FString &Parent, const TCHAR *Source)
{
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("name"), Name);
    Entry->SetStringField(TEXT("class"), Class ? Class->GetName() : TEXT(""));
    Entry->SetStringField(TEXT("classPath"), Class ? Class->GetPathName() : TEXT(""));
    Entry->SetStringField(TEXT("parent"), Parent);
    Entry->SetStringField(TEXT("source"), Source);
    return Entry;
}

UBlueprint *McpResolveBlueprintFromPayload(const TSharedPtr<FJsonObject> &Payload, FString &OutRequested,
                                           FString &OutNormalized, FString &OutError)
{
    OutRequested = McpGetFirstStringField(Payload, {TEXT("blueprintPath"), TEXT("assetPath"), TEXT("objectPath"),
                                                    TEXT("path"), TEXT("name"), TEXT("actorName")});
    return OutRequested.IsEmpty() ? nullptr : LoadBlueprintAsset(OutRequested, OutNormalized, OutError);
}
} // namespace

// SCS components (this Blueprint first, then inherited Blueprint parents) plus
// the native components instantiated on the generated class default object.
TArray<TSharedPtr<FJsonValue>> McpCollectBlueprintComponents(UBlueprint *Blueprint)
{
    TArray<TSharedPtr<FJsonValue>> Components;
    TSet<FString> Seen;
    for (UBlueprint *Current = Blueprint; Current != nullptr;)
    {
        if (USimpleConstructionScript *Scs = Current->SimpleConstructionScript)
        {
            const TArray<USCS_Node *> &Nodes = Scs->GetAllNodes();
            TMap<const USCS_Node *, FString> ParentNames;
            for (USCS_Node *Node : Nodes)
            {
                if (!Node)
                {
                    continue;
                }
                for (USCS_Node *Child : Node->GetChildNodes())
                {
                    if (Child)
                    {
                        ParentNames.Add(Child, Node->GetVariableName().ToString());
                    }
                }
            }
            for (USCS_Node *Node : Nodes)
            {
                const FString VarName = Node ? Node->GetVariableName().ToString() : FString();
                if (!Node || Seen.Contains(VarName))
                {
                    continue;
                }
                Seen.Add(VarName);
                const FString *TreeParent = ParentNames.Find(Node);
                const FString Parent = TreeParent ? *TreeParent
                    : (Node->ParentComponentOrVariableName.IsNone() ? FString()
                                                                    : Node->ParentComponentOrVariableName.ToString());
                const UClass *Class = Node->ComponentTemplate ? Node->ComponentTemplate->GetClass() : nullptr;
                if (!Class)
                {
                    Class = Node->ComponentClass;
                }
                Components.Add(MakeShared<FJsonValueObject>(McpMakeComponentEntry(
                    VarName, Class, Parent, Current == Blueprint ? TEXT("SCS") : TEXT("SCS_Inherited"))));
            }
        }
        UClass *ParentClass = Current->ParentClass;
        Current = ParentClass ? Cast<UBlueprint>(ParentClass->ClassGeneratedBy) : nullptr;
    }
    AActor *DefaultActor = (Blueprint && Blueprint->GeneratedClass)
        ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (DefaultActor)
    {
        TInlineComponentArray<UActorComponent *> NativeComponents;
        DefaultActor->GetComponents(NativeComponents);
        for (UActorComponent *Component : NativeComponents)
        {
            if (!Component || Seen.Contains(Component->GetName()))
            {
                continue;
            }
            Seen.Add(Component->GetName());
            const USceneComponent *Scene = Cast<USceneComponent>(Component);
            const FString Parent = (Scene && Scene->GetAttachParent()) ? Scene->GetAttachParent()->GetName() : FString();
            Components.Add(MakeShared<FJsonValueObject>(
                McpMakeComponentEntry(Component->GetName(), Component->GetClass(), Parent, TEXT("Native"))));
        }
    }
    return Components;
}

TArray<TSharedPtr<FJsonValue>> McpCollectBlueprintVariables(UBlueprint *Blueprint)
{
    TArray<TSharedPtr<FJsonValue>> Variables;
    if (!Blueprint)
    {
        return Variables;
    }
    for (const FBPVariableDescription &Variable : Blueprint->NewVariables)
    {
        TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
        Entry->SetStringField(TEXT("name"), Variable.VarName.ToString());
        Entry->SetStringField(TEXT("type"), UEdGraphSchema_K2::TypeToText(Variable.VarType).ToString());
        Entry->SetStringField(TEXT("category"), Variable.Category.ToString());
        Entry->SetStringField(TEXT("defaultValue"), Variable.DefaultValue);
        Variables.Add(MakeShared<FJsonValueObject>(Entry));
    }
    return Variables;
}

// get_blueprint_details: accepts blueprintPath / assetPath / objectPath, returns
// the blueprint_get snapshot (variables, functions, events, defaults) plus the
// class hierarchy and the SCS + native component list.
bool HandleInspectBlueprintDetailsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString Requested, Normalized, LoadError;
    UBlueprint *Blueprint = McpResolveBlueprintFromPayload(Payload, Requested, Normalized, LoadError);
    if (Requested.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
                                   TEXT("blueprintPath, assetPath, or objectPath required for get_blueprint_details"),
                                   TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!Blueprint)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
                                   FString::Printf(TEXT("Blueprint not found: %s (%s)"), *Requested, *LoadError),
                                   TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }
    const FString Key = Normalized.IsEmpty() ? Requested : Normalized;
    TSharedPtr<FJsonObject> Resp = McpBlueprintHandlers::FMcpAutomationBridge_BuildBlueprintSnapshot(Blueprint, Key);
    if (!Resp.IsValid())
    {
        Resp = McpHandlerUtils::CreateResultObject();
    }
    UClass *Parent = Blueprint->ParentClass;
    Resp->SetStringField(TEXT("blueprintPath"), Key);
    Resp->SetStringField(TEXT("objectPath"), Blueprint->GetPathName());
    Resp->SetStringField(TEXT("objectName"), Blueprint->GetName());
    Resp->SetStringField(TEXT("className"), Blueprint->GetClass()->GetName());
    Resp->SetStringField(TEXT("parentClass"), Parent ? Parent->GetName() : TEXT("None"));
    Resp->SetStringField(TEXT("parentClassPath"), Parent ? Parent->GetPathName() : TEXT(""));
    Resp->SetStringField(TEXT("generatedClass"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : TEXT(""));
    Resp->SetStringField(TEXT("blueprintType"), StaticEnum<EBlueprintType>()->GetNameStringByValue(
        static_cast<int64>(Blueprint->BlueprintType.GetValue())));
    if (!Resp->HasField(TEXT("variables")))
    {
        Resp->SetArrayField(TEXT("variables"), McpCollectBlueprintVariables(Blueprint));
    }
    const TArray<TSharedPtr<FJsonValue>> Components = McpCollectBlueprintComponents(Blueprint);
    Resp->SetArrayField(TEXT("components"), Components);
    Resp->SetNumberField(TEXT("componentCount"), Components.Num());
    Resp->SetBoolField(TEXT("isActor"), false);
    Resp->SetBoolField(TEXT("success"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Blueprint details retrieved"), Resp, FString());
    return true;
}

// get_components with a Blueprint target: SCS components + CDO native components.
bool HandleInspectBlueprintComponentsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &BlueprintPath, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString Normalized, LoadError;
    UBlueprint *Blueprint = LoadBlueprintAsset(BlueprintPath, Normalized, LoadError);
    if (!Blueprint)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
                                   FString::Printf(TEXT("Blueprint not found: %s (%s)"), *BlueprintPath, *LoadError),
                                   TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }
    UClass *Parent = Blueprint->ParentClass;
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("blueprintPath"), Normalized.IsEmpty() ? BlueprintPath : Normalized);
    Resp->SetStringField(TEXT("className"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetName() : Blueprint->GetName());
    Resp->SetStringField(TEXT("parentClass"), Parent ? Parent->GetName() : TEXT("None"));
    const TArray<TSharedPtr<FJsonValue>> Components = McpCollectBlueprintComponents(Blueprint);
    Resp->SetArrayField(TEXT("components"), Components);
    Resp->SetNumberField(TEXT("componentCount"), Components.Num());
    Resp->SetNumberField(TEXT("count"), Components.Num());
    Resp->SetBoolField(TEXT("success"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Blueprint components retrieved"), Resp, FString());
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
