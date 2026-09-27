#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
namespace
{
// set_blend_mode / set_material_domain / set_shading_model: one enum-valued UMaterial
// property each, differing only in the payload field, the parser and the setter.
struct FMaterialEnumSetter
{
  const TCHAR* SubAction;
  const TCHAR* Field;
  const TCHAR* Noun;
  bool (*Apply)(UMaterial*, const FString&);
  FString (*Valid)();
};

bool ApplyBlendMode(UMaterial* Material, const FString& Value)
{
  EBlendMode Parsed{};
  if (!ParseBlendMode(Value, Parsed)) return false;
  Material->BlendMode = Parsed;
  return true;
}

bool ApplyMaterialDomain(UMaterial* Material, const FString& Value)
{
  EMaterialDomain Parsed{};
  if (!ParseMaterialDomain(Value, Parsed)) return false;
  Material->MaterialDomain = Parsed;
  return true;
}

bool ApplyShadingModel(UMaterial* Material, const FString& Value)
{
  EMaterialShadingModel Parsed{};
  if (!ParseShadingModel(Value, Parsed)) return false;
  Material->SetShadingModel(Parsed);
  return true;
}

const FMaterialEnumSetter Setters[] = {
  {TEXT("set_blend_mode"), TEXT("blendMode"), TEXT("Blend mode"), &ApplyBlendMode, &ValidBlendModes},
  {TEXT("set_material_domain"), TEXT("materialDomain"), TEXT("Material domain"), &ApplyMaterialDomain, &ValidMaterialDomains},
  {TEXT("set_shading_model"), TEXT("shadingModel"), TEXT("Shading model"), &ApplyShadingModel, &ValidShadingModels},
};
}

bool ApplyMaterialEnumFields(UMaterial* Material, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
  for (const FMaterialEnumSetter& Setter : Setters) {
    FString Value;
    if (Payload->TryGetStringField(Setter.Field, Value) && !Setter.Apply(Material, Value)) {
      OutError = FString::Printf(TEXT("Invalid %s '%s'. Valid values: %s"), Setter.Field, *Value, *Setter.Valid());
      return false;
    }
  }
  return true;
}

bool HandleSetMaterialEnumProperty(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  const FMaterialEnumSetter* Setter = nullptr;
  for (const FMaterialEnumSetter& Candidate : Setters) {
    if (SubAction == Candidate.SubAction) { Setter = &Candidate; }
  }
  if (!Setter) {
    return false;
  }

  FString AssetPath, Value;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!Payload->TryGetStringField(Setter->Field, Value)) {
    Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Missing '%s'."), Setter->Field),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Validate path security BEFORE loading asset
  const FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
  if (ValidatedPath.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                        TEXT("INVALID_PATH"));
    return true;
  }

  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunction(ValidatedPath, Material, Function);
  if (!Material && !Function) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
    return true;
  }
  if (!Material) {
    Bridge->SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("%s is only supported on UMaterial assets, not Material Functions."), *SubAction),
                        TEXT("UNSUPPORTED_ASSET_TYPE"));
    return true;
  }
  if (!Setter->Apply(Material, Value)) {
    Bridge->SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Invalid %s '%s'. Valid values: %s"), Setter->Field, *Value, *Setter->Valid()),
                        TEXT("INVALID_ENUM"));
    return true;
  }

  Material->PostEditChange();
  Material->MarkPackageDirty();
  if (GetJsonBoolField(Payload, TEXT("save"), true)) {
    McpSafeAssetSave(Material);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, Material);
  Bridge->SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("%s set to %s."), Setter->Noun, *Value), Result);
  return true;
}
}
