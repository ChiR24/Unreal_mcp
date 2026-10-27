// implement_interface: a Blueprint takes (or, with remove, drops) an interface, a Blueprint Interface asset or a native
// UInterface class. Blueprints could only reach each other through casts and dispatchers: no call added an interface.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Interface.h"

namespace McpBlueprintHandlers {
namespace {
// A Blueprint Interface asset's generated class, or a native interface class by path or name; null otherwise.
UClass *ResolveInterfaceClass(const FString &InterfacePath, FString &OutError) {
  UClass *Interface = nullptr;
  if (UBlueprint *InterfaceBlueprint = LoadObject<UBlueprint>(nullptr, *InterfacePath, nullptr, LOAD_NoWarn | LOAD_Quiet)) {
    Interface = InterfaceBlueprint->BlueprintType == BPTYPE_Interface ? InterfaceBlueprint->GeneratedClass.Get() : nullptr;
  } else {
    Interface = McpFindTypeQuiet(InterfacePath);
  }
  if (!Interface || !Interface->HasAnyClassFlags(CLASS_Interface)) {
    OutError = FString::Printf(TEXT("%s is not a Blueprint Interface or an interface class"), *InterfacePath);
    return nullptr;
  }
  return Interface;
}

bool Implements(const UBlueprint *Blueprint, const UClass *Interface) {
  return Blueprint->ImplementedInterfaces.ContainsByPredicate(
      [Interface](const FBPInterfaceDescription &Description) { return Description.Interface.Get() == Interface; });
}
} // namespace

bool HandleBlueprintImplementInterface(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (!ActionMatchesPattern(TEXT("implement_interface"))) {
    return false;
  }
  const FString Path = ResolveBlueprintRequestedPath();
  FString InterfacePath;
  LocalPayload->TryGetStringField(TEXT("interfacePath"), InterfacePath);
  bool bRemove = false;
  LocalPayload->TryGetBoolField(TEXT("remove"), bRemove);
  if (Path.IsEmpty() || InterfacePath.TrimStartAndEnd().IsEmpty()) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprintPath and interfacePath are required"),
                                  nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString Normalized, Error;
  UBlueprint *Blueprint = LoadBlueprintAsset(Path, Normalized, Error);
  if (!Blueprint) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  Error.IsEmpty() ? TEXT("Failed to load blueprint") : Error, nullptr,
                                  TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }
  UClass *Interface = ResolveInterfaceClass(InterfacePath.TrimStartAndEnd(), Error);
  if (!Interface) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Error, nullptr, TEXT("INTERFACE_NOT_FOUND"));
    return true;
  }

  // Already in the wanted state is success, so a repeat call changes nothing.
  const bool bChange = bRemove == Implements(Blueprint, Interface);
  if (bChange && bRemove) {
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
    FBlueprintEditorUtils::RemoveInterface(Blueprint, Interface->GetClassPathName());
#else
    FBlueprintEditorUtils::RemoveInterface(Blueprint, Interface->GetFName());
#endif
  } else if (bChange) {
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
    const bool bAdded = FBlueprintEditorUtils::ImplementNewInterface(Blueprint, Interface->GetClassPathName());
#else
    const bool bAdded = FBlueprintEditorUtils::ImplementNewInterface(Blueprint, Interface->GetFName());
#endif
    if (!bAdded) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    FString::Printf(TEXT("The editor refused to add %s"), *Interface->GetName()),
                                    nullptr, TEXT("IMPLEMENT_FAILED"));
      return true;
    }
  }
  const bool bCompiled = !bChange || McpSafeCompileBlueprint(Blueprint);
  const bool bSaved = !bChange || McpSafeAssetSave(Blueprint);

  // A function with outputs becomes a graph to fill (edit_graph graphName); the others are events the event graph
  // implements, as the editor's Implemented Interfaces panel lays them out.
  TArray<FString> FunctionGraphs, Events;
  if (!bRemove) {
    for (TFieldIterator<UFunction> It(Interface); It; ++It) {
      if (It->HasAnyFunctionFlags(FUNC_BlueprintEvent) && It->GetName() != TEXT("ExecuteUbergraph")) {
        (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(*It) ? Events : FunctionGraphs).Add(It->GetName());
      }
    }
  }
  TArray<FString> Interfaces;
  for (const FBPInterfaceDescription &Description : Blueprint->ImplementedInterfaces) {
    if (Description.Interface) Interfaces.Add(Description.Interface->GetPathName());
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("blueprintPath"), Normalized.IsEmpty() ? Path : Normalized);
  Resp->SetStringField(TEXT("interface"), Interface->GetPathName());
  Resp->SetBoolField(TEXT("changed"), bChange);
  Resp->SetArrayField(TEXT("interfaces"), McpHandlerUtils::ToJsonStringArray(Interfaces));
  if (!bRemove) {
    Resp->SetArrayField(TEXT("functionGraphs"), McpHandlerUtils::ToJsonStringArray(FunctionGraphs));
    Resp->SetArrayField(TEXT("events"), McpHandlerUtils::ToJsonStringArray(Events));
  }
  if (bChange) {
    McpHandlerUtils::AddVerification(Resp, Blueprint);
  }
  if (!bCompiled || !bSaved) {
    Resp->SetStringField(TEXT("warning"), !bCompiled ? TEXT("The Blueprint did not compile cleanly; compile it to read the errors.")
                                                     : TEXT("The Blueprint changed but could not be saved."));
  }
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                bRemove ? (bChange ? TEXT("Interface removed") : TEXT("Interface was not implemented"))
                                        : (bChange ? TEXT("Interface implemented") : TEXT("Interface already implemented")),
                                Resp);
  return true;
}
} // namespace McpBlueprintHandlers
