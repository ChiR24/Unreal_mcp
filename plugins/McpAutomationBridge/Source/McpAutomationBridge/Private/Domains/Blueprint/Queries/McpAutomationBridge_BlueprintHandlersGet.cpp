#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"

#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintGet(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if ((ActionMatchesPattern(TEXT("blueprint_get")) ||
       ActionMatchesPattern(TEXT("get_blueprint")) ||
       ActionMatchesPattern(TEXT("get"))) &&
      !Lower.Contains(TEXT("scs"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_get handler: RequestId=%s"), *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("blueprint_get requires a blueprint path."),
                             nullptr, TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    bool bExists = false;
    TSharedPtr<FJsonObject> Entry = nullptr;

    FString Normalized;
    FString Err;
    UBlueprint *BP = LoadBlueprintAsset(Path, Normalized, Err);
    bExists = (BP != nullptr);
    if (bExists) {
      const FString Key =
          !Normalized.TrimStartAndEnd().IsEmpty() ? Normalized : Path;
      Entry = FMcpAutomationBridge_BuildBlueprintSnapshot(BP, Key);
    }

    if (!bExists) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint not found"), nullptr,
                             TEXT("NOT_FOUND"));
      return true;
    }

    // The published `get` contract is {blueprintPath, propertyName} ->
    // {propertyValue}. Resolve the requested property from the snapshot's
    // defaults (CDO value, or the authored default for a variable that has
    // not been compiled in yet) instead of returning the bare snapshot, which
    // the output schema refused as OUTPUT_SCHEMA_VIOLATION.
    FString PropertyName;
    LocalPayload->TryGetStringField(TEXT("propertyName"), PropertyName);
    PropertyName.TrimStartAndEndInline();
    if (!PropertyName.IsEmpty() && Entry.IsValid()) {
      TSharedPtr<FJsonValue> PropertyValue;
      const TSharedPtr<FJsonObject> *Defaults = nullptr;
      if (Entry->TryGetObjectField(TEXT("defaults"), Defaults) && Defaults &&
          (*Defaults).IsValid()) {
        PropertyValue = (*Defaults)->TryGetField(PropertyName);
      }
      // The snapshot holds only the Blueprint's own variables, so an inherited
      // property (AutoPossessAI, MaxWalkSpeed...) read as missing while the
      // message claimed the CDO had been searched. Read it off the CDO.
      UClass *Generated = BP ? BP->GeneratedClass.Get() : nullptr;
      FProperty *CdoProperty =
          Generated ? Generated->FindPropertyByName(*PropertyName) : nullptr;
      if (!PropertyValue.IsValid() && CdoProperty) {
        PropertyValue = MakeShared<FJsonValueString>(
            McpPropertyReflection::GetPropertyValueAsString(
                Generated->GetDefaultObject(), CdoProperty));
      }
      // "Shield.bVisible": an SCS component's template is not a CDO property,
      // so one component default took an inspect call per component to read.
      FString ComponentName, ComponentPath;
      if (!PropertyValue.IsValid() && Generated &&
          PropertyName.Split(TEXT("."), &ComponentName, &ComponentPath)) {
        USCS_Node *Node = BP->SimpleConstructionScript
                              ? BP->SimpleConstructionScript->FindSCSNode(FName(*ComponentName))
                              : nullptr;
        UObject *Template = Node ? Node->ComponentTemplate : nullptr;
        // A component the native parent creates (a Character's movement) is a
        // default subobject of the CDO, not an SCS node, so "CharMoveComp.
        // JumpZVelocity" and "CharacterMovement.JumpZVelocity" both missed. Take
        // it by its object name, else by the property that holds it.
        UObject *CDO = Generated->GetDefaultObject();
        if (!Template) {
          Template = CDO->GetDefaultSubobjectByName(FName(*ComponentName));
        }
        if (FObjectProperty *Holder = Template ? nullptr : FindFProperty<FObjectProperty>(Generated, *ComponentName)) {
          Template = Holder->GetObjectPropertyValue_InContainer(CDO);
        }
        void *Container = nullptr;
        FString PathError, ResolvedPath;
        // The shared resolver edit_scs set_property writes through.
        if (FProperty *Prop = Template ? McpResolvePropertyPath(Template, ComponentPath, Container, ResolvedPath, PathError) : nullptr) {
          FString Text;
          Prop->ExportText_InContainer(0, Text, Container, nullptr, Template, PPF_None);
          PropertyValue = MakeShared<FJsonValueString>(Text);
        }
      }
      if (!PropertyValue.IsValid()) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetStringField(TEXT("blueprintPath"), Path);
        Resp->SetStringField(TEXT("propertyName"), PropertyName);
        Bridge.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Property '%s' not found on blueprint (its variables, CDO properties and, as Component.Property, its components' defaults are searched). Its variables: %s."), *PropertyName, *McpBlueprintVariableList(Generated)),
            Resp, TEXT("PROPERTY_NOT_FOUND"));
        return true;
      }
      // Just the value: the whole summary (variables, components, graphs)
      // used to ride along with every single-property read.
      TSharedPtr<FJsonObject> Lean = MakeShared<FJsonObject>();
      FString AssetPath;
      if (Entry->TryGetStringField(TEXT("assetPath"), AssetPath)) {
        Lean->SetStringField(TEXT("assetPath"), AssetPath);
      }
      Lean->SetField(TEXT("propertyValue"), PropertyValue);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                    FString::Printf(TEXT("Read %s"), *PropertyName), Lean, FString());
      return true;
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Blueprint fetched"), Entry, FString());
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
