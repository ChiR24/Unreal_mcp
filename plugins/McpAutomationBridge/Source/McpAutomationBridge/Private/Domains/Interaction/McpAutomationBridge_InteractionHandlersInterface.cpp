#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersMutationEvidence.h"

namespace McpInteractionHandlers
{
bool HandleInteractableInterfaceAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction != TEXT("create_interactable_interface"))
    {
        return false;
    }

    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    const FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interfaces"));
    if (Name.IsEmpty())
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
        return true;
    }

#if WITH_EDITOR
    // A duplicate create used to run the factory, the function-graph authoring
    // and the save before anything noticed the asset already existed, which
    // stalled the queue into a -32001 timeout instead of refusing. Refusing
    // here means nothing below runs, so no package revision moves.
    const FString InterfacePath = MakeLegacyPackageName(Folder, Name, TEXT("/Game/Interfaces"));
    if (UEditorAssetLibrary::DoesAssetExist(InterfacePath))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Interactable interface already exists at %s. Choose a different name or folder."), *InterfacePath),
            TEXT("ALREADY_EXISTS"));
        return true;
    }

    UPackage* Package = CreatePackage(*InterfacePath);
    if (!Package)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
        return true;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Factory->BlueprintType = BPTYPE_Interface;
#endif
    Factory->ParentClass = UInterface::StaticClass();
    UBlueprint* InterfaceBP = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn));

    if (!InterfaceBP)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create interface blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
        return true;
    }

    InterfaceBP->BlueprintType = BPTYPE_Interface;
    const TArray<FString> FunctionNames = {TEXT("Interact"), TEXT("CanInteract"), TEXT("GetInteractionPrompt")};
    TArray<TSharedPtr<FJsonValue>> FunctionsAdded;
    for (const FString& FunctionName : FunctionNames)
    {
        UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
            InterfaceBP, FName(*FunctionName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        if (NewGraph)
        {
            FBlueprintEditorUtils::AddFunctionGraph<UFunction>(InterfaceBP, NewGraph, false, static_cast<UFunction*>(nullptr));
            FunctionsAdded.Add(MakeShared<FJsonValueString>(FunctionName));
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InterfaceBP);
    FAssetRegistryModule::AssetCreated(InterfaceBP);
    const bool bInterfaceSaved = McpSafeAssetSave(InterfaceBP);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("interfacePath"), InterfaceBP->GetPathName());
    Result->SetStringField(TEXT("interfaceName"), Name);
    Result->SetBoolField(TEXT("created"), true);
    Result->SetArrayField(TEXT("functionsAdded"), FunctionsAdded);
    TArray<FString> InterfaceChanges;
    InterfaceChanges.Add(TEXT("created interactable interface"));
    if (bInterfaceSaved) { InterfaceChanges.Add(TEXT("saved")); }
    AddMutationEvidence(Result, InterfaceBP, InterfaceChanges);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interactable interface created"), Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("create_interactable_interface is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
