#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_ConstructObjectFromClass.h"

namespace McpBlueprintGraphHandlers
{
bool TryCreateConstructObjectNode(
    FActionContext& Context,
    UClass* NodeClass,
    float X,
    float Y)
{
    // Covers the whole UK2Node_ConstructObjectFromClass family:
    // SpawnActorFromClass, ConstructObjectFromClass, CreateWidget,
    // AddComponentByClass, ... — every one of which crashes on the generic path.
    if (!NodeClass->IsChildOf(UK2Node_ConstructObjectFromClass::StaticClass()))
    {
        return false;
    }

    // Resolve an optional construct/spawn class up front, before mutating the
    // graph. A classless node is valid (and no longer crashes), but if the
    // caller explicitly named a class we must not silently drop it — reject with
    // CLASS_NOT_FOUND, matching the Cast branch above.
    //
    // Prefer `targetClass` (the spelling the rest of create_node uses) and fall
    // back to the legacy spawnClass/actorClass/class spellings for backward
    // compatibility. Resolve via the shared ResolveTargetClassFromString so
    // /Game/... Blueprint asset paths load correctly — ResolveUClass only does
    // native/name lookups and returns CLASS_NOT_FOUND for /Game/ paths.
    FString RequestedClassName;
    UClass* DesiredClass = nullptr;
    const bool bHasRequestedClass =
        Context.Payload->TryGetStringField(TEXT("targetClass"), RequestedClassName) ||
        Context.Payload->TryGetStringField(TEXT("spawnClass"), RequestedClassName) ||
        Context.Payload->TryGetStringField(TEXT("actorClass"), RequestedClassName) ||
        Context.Payload->TryGetStringField(TEXT("class"), RequestedClassName);
    if (bHasRequestedClass)
    {
        DesiredClass = ResolveTargetClassFromString(RequestedClassName);
        if (!DesiredClass)
        {
            Context.SendError(
                FString::Printf(
                    TEXT("Class '%s' not found"),
                    *RequestedClassName),
                TEXT("CLASS_NOT_FOUND"));
            return true;
        }
    }

    UEdGraphNode* NewNode =
        NewObject<UEdGraphNode>(Context.TargetGraph, NodeClass);
    if (!NewNode)
    {
        Context.SendError(
            TEXT("Failed to instantiate node."),
            TEXT("CREATE_FAILED"));
        return true;
    }

    Context.TargetGraph->AddNode(NewNode, false, false);
    NewNode->Modify();
    NewNode->CreateNewGuid();

    // ROOT-CAUSE FIX: allocate pins BEFORE PostPlacedNewNode(). Nodes in this
    // family read checked pins inside PostPlacedNewNode() — e.g.
    // UK2Node_SpawnActorFromClass::GetScaleMethodPin() => FindPinChecked() — which
    // check()-crashes the editor when the pin doesn't exist yet. The editor's
    // palette spawn survives because the cached template node already carries pins;
    // the generic create_node path (PostPlacedNewNode then AllocateDefaultPins)
    // does not, so it asserts. Allocating first makes the checked accessor safe.
    NewNode->AllocateDefaultPins();
    NewNode->PostPlacedNewNode();

    // Apply the requested construct/spawn class (already resolved above) so the
    // expose-on-spawn pins are generated. Mirror how the engine itself seeds the
    // class (ExpandNode): set DefaultObject, clear the textual default, then
    // rebuild pins. Pins already exist here, so ReconstructNode is safe.
    if (DesiredClass)
    {
        if (UK2Node_ConstructObjectFromClass* ConstructNode =
                Cast<UK2Node_ConstructObjectFromClass>(NewNode))
        {
            if (UEdGraphPin* ClassPin = ConstructNode->GetClassPin())
            {
                ClassPin->DefaultObject = DesiredClass;
                ClassPin->DefaultValue.Reset();
                NewNode->ReconstructNode();
            }
        }
    }

    NewNode->NodePosX = X;
    NewNode->NodePosY = Y;
    if (const UEdGraphSchema* Schema = Context.TargetGraph->GetSchema())
    {
        Schema->ForceVisualizationCacheClear();
    }
    Context.TargetGraph->NotifyGraphChanged();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
    Result->SetStringField(TEXT("nodeClass"), NodeClass->GetName());
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Node created."), Result);
    return true;
}
}
#endif
