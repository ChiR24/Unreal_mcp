// McpAutomationBridge_BlueprintInterfaceHandlers.cpp
// Blueprint Interface Management Handlers
//
// Provides:
// - create_blueprint_interface: Create a BPTYPE_Interface Blueprint
// - add_function: Add a function graph to an interface
// - remove_function: Remove a function graph from an interface
// - list_functions: List all function graphs on an interface
// - implement_interface: Add an interface to a Blueprint
// - remove_interface: Remove an interface from a Blueprint
// - list_interfaces: List interfaces implemented by a Blueprint

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraphSchema_K2.h"
#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpBlueprintInterfaceHandlers, Log, All);

// ============================================================================
// Sub-Handlers (static, editor-only)
// ============================================================================

#if WITH_EDITOR

// ----------------------------------------------------------------------------
// create_blueprint_interface
// ----------------------------------------------------------------------------
static bool HandleCreateBlueprintInterface(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetName = GetJsonStringField(Payload, TEXT("assetName"), TEXT(""));
    FString FolderPath = GetJsonStringField(Payload, TEXT("folderPath"), TEXT("/Game"));

    if (AssetName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetName is required"), nullptr);
        return true;
    }

    FString SanitizedFolder = SanitizeProjectRelativePath(FolderPath);
    if (SanitizedFolder.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid folderPath: contains traversal or invalid characters"), nullptr);
        return true;
    }

    // Ensure folder starts with /Game
    if (!SanitizedFolder.StartsWith(TEXT("/Game")) &&
        !SanitizedFolder.StartsWith(TEXT("/Engine")) &&
        !SanitizedFolder.StartsWith(TEXT("/Script")))
    {
        SanitizedFolder = TEXT("/Game") + SanitizedFolder;
    }

    FString PackagePath = SanitizedFolder / AssetName;

    // Create a Blueprint Interface using UBlueprintFactory
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = UInterface::StaticClass();
    Factory->BlueprintType = BPTYPE_Interface;

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UObject* NewAsset = AssetTools.CreateAsset(AssetName, SanitizedFolder, UBlueprint::StaticClass(), Factory);

    if (!NewAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create Blueprint Interface '%s' at '%s'"), *AssetName, *SanitizedFolder), nullptr);
        return true;
    }

    McpSafeAssetSave(NewAsset);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("assetName"), AssetName);
    ResponseJson->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    ResponseJson->SetStringField(TEXT("folderPath"), SanitizedFolder);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Blueprint Interface '%s' created at '%s'"), *AssetName, *SanitizedFolder), ResponseJson);
    return true;
}

// ----------------------------------------------------------------------------
// add_function
// ----------------------------------------------------------------------------
static bool HandleAddFunction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString InterfacePath = GetJsonStringField(Payload, TEXT("interfacePath"), TEXT(""));
    FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"), TEXT(""));

    if (InterfacePath.IsEmpty() || FunctionName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("interfacePath and functionName are required"), nullptr);
        return true;
    }

    FString SanitizedPath = SanitizeProjectRelativePath(InterfacePath);
    if (SanitizedPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid interfacePath"), nullptr);
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *SanitizedPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to load Blueprint at '%s'"), *SanitizedPath), nullptr);
        return true;
    }

    // Check if function already exists
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetFName() == FName(*FunctionName))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Function '%s' already exists on '%s'"), *FunctionName, *SanitizedPath), nullptr);
            return true;
        }
    }

    // Create a new function graph
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        FName(*FunctionName),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());

    if (!NewGraph)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create function graph '%s'"), *FunctionName), nullptr);
        return true;
    }

    FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewGraph, /*bIsUserCreated=*/true, static_cast<UFunction*>(nullptr));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("interfacePath"), SanitizedPath);
    ResponseJson->SetStringField(TEXT("functionName"), FunctionName);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Function '%s' added to '%s'"), *FunctionName, *SanitizedPath), ResponseJson);
    return true;
}

// ----------------------------------------------------------------------------
// remove_function
// ----------------------------------------------------------------------------
static bool HandleRemoveFunction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString InterfacePath = GetJsonStringField(Payload, TEXT("interfacePath"), TEXT(""));
    FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"), TEXT(""));

    if (InterfacePath.IsEmpty() || FunctionName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("interfacePath and functionName are required"), nullptr);
        return true;
    }

    FString SanitizedPath = SanitizeProjectRelativePath(InterfacePath);
    if (SanitizedPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid interfacePath"), nullptr);
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *SanitizedPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to load Blueprint at '%s'"), *SanitizedPath), nullptr);
        return true;
    }

    // Find the function graph
    UEdGraph* TargetGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetFName() == FName(*FunctionName))
        {
            TargetGraph = Graph;
            break;
        }
    }

    if (!TargetGraph)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Function '%s' not found on '%s'"), *FunctionName, *SanitizedPath), nullptr);
        return true;
    }

    FBlueprintEditorUtils::RemoveGraph(Blueprint, TargetGraph);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("interfacePath"), SanitizedPath);
    ResponseJson->SetStringField(TEXT("functionName"), FunctionName);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Function '%s' removed from '%s'"), *FunctionName, *SanitizedPath), ResponseJson);
    return true;
}

// ----------------------------------------------------------------------------
// list_functions
// ----------------------------------------------------------------------------
static bool HandleListFunctions(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString InterfacePath = GetJsonStringField(Payload, TEXT("interfacePath"), TEXT(""));

    if (InterfacePath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("interfacePath is required"), nullptr);
        return true;
    }

    FString SanitizedPath = SanitizeProjectRelativePath(InterfacePath);
    if (SanitizedPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid interfacePath"), nullptr);
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *SanitizedPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to load Blueprint at '%s'"), *SanitizedPath), nullptr);
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> FunctionsArray;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph)
        {
            TSharedPtr<FJsonObject> FuncObj = MakeShareable(new FJsonObject());
            FuncObj->SetStringField(TEXT("name"), Graph->GetName());
            FunctionsArray.Add(MakeShareable(new FJsonValueObject(FuncObj)));
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("interfacePath"), SanitizedPath);
    ResponseJson->SetArrayField(TEXT("functions"), FunctionsArray);
    ResponseJson->SetNumberField(TEXT("count"), FunctionsArray.Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d function(s) on '%s'"), FunctionsArray.Num(), *SanitizedPath), ResponseJson);
    return true;
}

// ----------------------------------------------------------------------------
// implement_interface
// ----------------------------------------------------------------------------
static bool HandleImplementInterface(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    FString InterfacePath = GetJsonStringField(Payload, TEXT("interfacePath"), TEXT(""));

    if (BlueprintPath.IsEmpty() || InterfacePath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath and interfacePath are required"), nullptr);
        return true;
    }

    FString SanitizedBPPath = SanitizeProjectRelativePath(BlueprintPath);
    FString SanitizedInterfacePath = SanitizeProjectRelativePath(InterfacePath);

    if (SanitizedBPPath.IsEmpty() || SanitizedInterfacePath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid blueprintPath or interfacePath"), nullptr);
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *SanitizedBPPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to load Blueprint at '%s'"), *SanitizedBPPath), nullptr);
        return true;
    }

    // Load the interface Blueprint to get its generated class path
    UBlueprint* InterfaceBP = LoadObject<UBlueprint>(nullptr, *SanitizedInterfacePath);
    if (!InterfaceBP)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to load interface Blueprint at '%s'"), *SanitizedInterfacePath), nullptr);
        return true;
    }

    UClass* InterfaceClass = InterfaceBP->GeneratedClass;
    if (!InterfaceClass)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Interface Blueprint '%s' has no generated class"), *SanitizedInterfacePath), nullptr);
        return true;
    }

    // Check if the interface is already implemented
    for (const FBPInterfaceDescription& Desc : Blueprint->ImplementedInterfaces)
    {
        if (Desc.Interface == InterfaceClass)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Interface '%s' is already implemented on '%s'"), *SanitizedInterfacePath, *SanitizedBPPath), nullptr);
            return true;
        }
    }

    FTopLevelAssetPath InterfaceAssetPath(InterfaceClass);
    FBlueprintEditorUtils::ImplementNewInterface(Blueprint, InterfaceAssetPath);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("blueprintPath"), SanitizedBPPath);
    ResponseJson->SetStringField(TEXT("interfacePath"), SanitizedInterfacePath);
    ResponseJson->SetStringField(TEXT("interfaceClass"), InterfaceClass->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Interface '%s' implemented on '%s'"), *SanitizedInterfacePath, *SanitizedBPPath), ResponseJson);
    return true;
}

// ----------------------------------------------------------------------------
// remove_interface
// ----------------------------------------------------------------------------
static bool HandleRemoveInterface(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    FString InterfacePath = GetJsonStringField(Payload, TEXT("interfacePath"), TEXT(""));

    if (BlueprintPath.IsEmpty() || InterfacePath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath and interfacePath are required"), nullptr);
        return true;
    }

    FString SanitizedBPPath = SanitizeProjectRelativePath(BlueprintPath);
    FString SanitizedInterfacePath = SanitizeProjectRelativePath(InterfacePath);

    if (SanitizedBPPath.IsEmpty() || SanitizedInterfacePath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid blueprintPath or interfacePath"), nullptr);
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *SanitizedBPPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to load Blueprint at '%s'"), *SanitizedBPPath), nullptr);
        return true;
    }

    // Load the interface Blueprint to get its generated class
    UBlueprint* InterfaceBP = LoadObject<UBlueprint>(nullptr, *SanitizedInterfacePath);
    if (!InterfaceBP)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to load interface Blueprint at '%s'"), *SanitizedInterfacePath), nullptr);
        return true;
    }

    UClass* InterfaceClass = InterfaceBP->GeneratedClass;
    if (!InterfaceClass)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Interface Blueprint '%s' has no generated class"), *SanitizedInterfacePath), nullptr);
        return true;
    }

    // Verify the interface is currently implemented
    bool bFound = false;
    for (const FBPInterfaceDescription& Desc : Blueprint->ImplementedInterfaces)
    {
        if (Desc.Interface == InterfaceClass)
        {
            bFound = true;
            break;
        }
    }

    if (!bFound)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Interface '%s' is not implemented on '%s'"), *SanitizedInterfacePath, *SanitizedBPPath), nullptr);
        return true;
    }

    FBlueprintEditorUtils::RemoveInterface(Blueprint, FTopLevelAssetPath(InterfaceClass));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("blueprintPath"), SanitizedBPPath);
    ResponseJson->SetStringField(TEXT("interfacePath"), SanitizedInterfacePath);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Interface '%s' removed from '%s'"), *SanitizedInterfacePath, *SanitizedBPPath), ResponseJson);
    return true;
}

// ----------------------------------------------------------------------------
// list_interfaces
// ----------------------------------------------------------------------------
static bool HandleListInterfaces(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"), TEXT(""));

    if (BlueprintPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath is required"), nullptr);
        return true;
    }

    FString SanitizedPath = SanitizeProjectRelativePath(BlueprintPath);
    if (SanitizedPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Invalid blueprintPath"), nullptr);
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *SanitizedPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to load Blueprint at '%s'"), *SanitizedPath), nullptr);
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> InterfacesArray;
    for (const FBPInterfaceDescription& Desc : Blueprint->ImplementedInterfaces)
    {
        if (Desc.Interface)
        {
            TSharedPtr<FJsonObject> InterfaceObj = MakeShareable(new FJsonObject());
            InterfaceObj->SetStringField(TEXT("className"), Desc.Interface->GetName());
            InterfaceObj->SetStringField(TEXT("classPath"), Desc.Interface->GetPathName());

            // List functions provided by this interface
            TArray<TSharedPtr<FJsonValue>> FuncsArray;
            for (UEdGraph* Graph : Desc.Graphs)
            {
                if (Graph)
                {
                    TSharedPtr<FJsonObject> FuncObj = MakeShareable(new FJsonObject());
                    FuncObj->SetStringField(TEXT("name"), Graph->GetName());
                    FuncsArray.Add(MakeShareable(new FJsonValueObject(FuncObj)));
                }
            }
            InterfaceObj->SetArrayField(TEXT("functions"), FuncsArray);

            InterfacesArray.Add(MakeShareable(new FJsonValueObject(InterfaceObj)));
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("blueprintPath"), SanitizedPath);
    ResponseJson->SetArrayField(TEXT("interfaces"), InterfacesArray);
    ResponseJson->SetNumberField(TEXT("count"), InterfacesArray.Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d interface(s) on '%s'"), InterfacesArray.Num(), *SanitizedPath), ResponseJson);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageBlueprintInterfaceAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpBlueprintInterfaceHandlers, Log,
        TEXT("HandleManageBlueprintInterfaceAction: SubAction=%s, RequestId=%s"), *SubAction, *RequestId);

    bool bHandled = false;

    if (SubAction == TEXT("create_blueprint_interface"))
    {
        bHandled = HandleCreateBlueprintInterface(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("add_function"))
    {
        bHandled = HandleAddFunction(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("remove_function"))
    {
        bHandled = HandleRemoveFunction(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("list_functions"))
    {
        bHandled = HandleListFunctions(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("implement_interface"))
    {
        bHandled = HandleImplementInterface(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("remove_interface"))
    {
        bHandled = HandleRemoveInterface(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("list_interfaces"))
    {
        bHandled = HandleListInterfaces(this, RequestId, Payload, Socket);
    }
    else
    {
        SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Unknown manage_blueprint_interface subAction: %s"), *SubAction), nullptr);
        return true;
    }

    return bHandled;

#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("manage_blueprint_interface requires editor build"), nullptr);
    return true;
#endif
}
