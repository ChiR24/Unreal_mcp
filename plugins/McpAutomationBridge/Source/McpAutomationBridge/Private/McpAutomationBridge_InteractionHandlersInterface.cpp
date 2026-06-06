#include "McpAutomationBridge_InteractionHandlersPrivate.h"

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
    UPackage* Package = CreatePackage(*MakeLegacyPackageName(Folder, Name, TEXT("/Game/Interfaces")));
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
    McpSafeAssetSave(InterfaceBP);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("interfacePath"), InterfaceBP->GetPathName());
    Result->SetStringField(TEXT("interfaceName"), Name);
    Result->SetBoolField(TEXT("created"), true);
    Result->SetArrayField(TEXT("functionsAdded"), FunctionsAdded);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interactable interface created"), Result);
#else
    Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("create_interactable_interface is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
