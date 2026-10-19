#include "Domains/MaterialAuthoring/McpMatPrivate.h"
#include "Materials/MaterialInstanceConstant.h"

namespace McpMaterialAuthoringHandlers
{
namespace
{
// set_blend_mode / set_material_domain / set_shading_model: one enum-valued UMaterial
// property each, differing only in the payload field, the parser and the setter. A
// material instance overrides the blend mode and the shading model of its parent, as
// its Material Property Overrides do; the domain is the parent's alone.
struct FMaterialEnumSetter
{
  const TCHAR* SubAction;
  const TCHAR* Field;
  const TCHAR* Noun;
  bool (*Apply)(UMaterial*, const FString&);
  FString (*Valid)();
  bool (*Override)(UMaterialInstanceConstant*, const FString&);
};

bool ApplyBlendMode(UMaterial* Material, const FString& Value)
{
  EBlendMode Parsed{};
  if (!ParseBlendMode(Value, Parsed)) return false;
  if (Material) Material->BlendMode = Parsed;
  return true;
}

bool ApplyMaterialDomain(UMaterial* Material, const FString& Value)
{
  EMaterialDomain Parsed{};
  if (!ParseMaterialDomain(Value, Parsed)) return false;
  if (Material) Material->MaterialDomain = Parsed;
  return true;
}

bool ApplyShadingModel(UMaterial* Material, const FString& Value)
{
  EMaterialShadingModel Parsed{};
  if (!ParseShadingModel(Value, Parsed)) return false;
  if (Material) Material->SetShadingModel(Parsed);
  return true;
}

bool OverrideBlendMode(UMaterialInstanceConstant* Instance, const FString& Value)
{
  EBlendMode Parsed{};
  if (!ParseBlendMode(Value, Parsed)) return false;
  Instance->BasePropertyOverrides.bOverride_BlendMode = true;
  Instance->BasePropertyOverrides.BlendMode = Parsed;
  return true;
}

bool OverrideShadingModel(UMaterialInstanceConstant* Instance, const FString& Value)
{
  EMaterialShadingModel Parsed{};
  if (!ParseShadingModel(Value, Parsed)) return false;
  Instance->BasePropertyOverrides.bOverride_ShadingModel = true;
  Instance->BasePropertyOverrides.ShadingModel = Parsed;
  return true;
}

const FMaterialEnumSetter Setters[] = {
  {TEXT("set_blend_mode"), TEXT("blendMode"), TEXT("Blend mode"), &ApplyBlendMode, &ValidBlendModes, &OverrideBlendMode},
  {TEXT("set_material_domain"), TEXT("materialDomain"), TEXT("Material domain"), &ApplyMaterialDomain, &ValidMaterialDomains, nullptr},
  {TEXT("set_shading_model"), TEXT("shadingModel"), TEXT("Shading model"), &ApplyShadingModel, &ValidShadingModels, &OverrideShadingModel},
};

// What a property setter edits: the material at assetPath, or a material instance. Anything else is
// refused here: a missing asset, a material function, and engine content, which every project on
// this engine shares (a set used to write and save it). A material instance was ASSET_NOT_FOUND.
UMaterialInterface* LoadPropertyTarget(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                                       TSharedPtr<FMcpBridgeWebSocket> Socket, FString& OutPath)
{
  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
    return nullptr;
  }
  OutPath = SanitizeProjectRelativePath(AssetPath);
  if (OutPath.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId, McpPathRefusalMessage(TEXT("path"), AssetPath), TEXT("INVALID_PATH"));
    return nullptr;
  }
  UObject* Loaded = StaticLoadObject(UObject::StaticClass(), nullptr, *OutPath);
  UMaterialInterface* Target = Cast<UMaterial>(Loaded) ? Cast<UMaterialInterface>(Loaded) : Cast<UMaterialInstanceConstant>(Loaded);
  if (!Target) {
    Bridge->SendAutomationError(Socket, RequestId,
        Loaded ? FString::Printf(TEXT("%s is a %s, not a material or material instance."), *AssetPath, *Loaded->GetClass()->GetName())
               : FString(TEXT("Could not load Material.")),
        Loaded ? TEXT("UNSUPPORTED_ASSET_TYPE") : TEXT("ASSET_NOT_FOUND"));
    return nullptr;
  }
  if (Target->GetOutermost()->GetName().StartsWith(TEXT("/Engine/"))) {
    Bridge->SendAutomationError(Socket, RequestId,
        TEXT("Engine content is not changed: duplicate the material into /Game and edit the copy."), TEXT("ENGINE_CONTENT"));
    return nullptr;
  }
  return Target;
}

// Recompile, save unless save is false, and answer with what was written and whether it was saved.
TSharedPtr<FJsonObject> FinishPropertyEdit(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, UMaterialInterface* Target)
{
  McpFinishMaterialEdit(RequestId, Target);
  const bool bSaved = GetJsonBoolField(Payload, TEXT("save"), true) && McpSafeAssetSave(Target);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("saved"), bSaved);
  if (Target->IsA<UMaterialInstanceConstant>()) {
    Result->SetBoolField(TEXT("instanceOverride"), true);
  }
  McpHandlerUtils::AddVerification(Result, Target);
  return Result;
}
}

// A null Material only checks the fields, so a create can refuse before it makes anything.
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

bool HandleSetMaterialEnumProperty(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  const FMaterialEnumSetter* Setter = nullptr;
  for (const FMaterialEnumSetter& Candidate : Setters) {
    if (SubAction == Candidate.SubAction) { Setter = &Candidate; }
  }
  if (!Setter) {
    return false;
  }

  FString Value;
  if (!Payload->TryGetStringField(Setter->Field, Value)) {
    Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Missing '%s'."), Setter->Field), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // Checked before anything is touched, so a bad value leaves the asset unchanged.
  if (!Setter->Apply(nullptr, Value)) {
    Bridge->SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Invalid %s '%s'. Valid values: %s"), Setter->Field, *Value, *Setter->Valid()),
                        TEXT("INVALID_ENUM"));
    return true;
  }
  FString Path;
  UMaterialInterface* Target = LoadPropertyTarget(Bridge, RequestId, Payload, Socket, Path);
  if (!Target) {
    return true;
  }
  UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Target);
  if (Instance && !Setter->Override) {
    Bridge->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("A material instance cannot override the %s; set it on the parent material%s."), Setter->Field,
                        Instance->GetMaterial() ? *FString::Printf(TEXT(" %s"), *Instance->GetMaterial()->GetPathName()) : TEXT("")),
        TEXT("MATERIAL_INSTANCE"));
    return true;
  }

  Target->Modify();
  if (Instance) {
    Setter->Override(Instance, Value);
  } else {
    Setter->Apply(Cast<UMaterial>(Target), Value);
  }
  TSharedPtr<FJsonObject> Result = FinishPropertyEdit(RequestId, Payload, Target);
  Result->SetStringField(Setter->Field, Value);
  Bridge->SendAutomationResponse(Socket, RequestId, true,
      Instance ? FString::Printf(TEXT("%s overridden to %s on the instance."), Setter->Noun, *Value)
               : FString::Printf(TEXT("%s set to %s."), Setter->Noun, *Value),
      Result);
  return true;
}

bool HandleSetTwoSided(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction != TEXT("set_two_sided")) {
    return false;
  }
  FString Path;
  UMaterialInterface* Target = LoadPropertyTarget(Bridge, RequestId, Payload, Socket, Path);
  if (!Target) {
    return true;
  }

  // The contract declares `value`; `twoSided` stays as the spelling build_material_graph steps may carry.
  bool bTwoSided = true;
  if (!Payload->TryGetBoolField(TEXT("value"), bTwoSided)) { Payload->TryGetBoolField(TEXT("twoSided"), bTwoSided); }
  Target->Modify();
  if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Target)) {
    Instance->BasePropertyOverrides.bOverride_TwoSided = true;
    Instance->BasePropertyOverrides.TwoSided = bTwoSided ? 1 : 0;
  } else {
    Cast<UMaterial>(Target)->TwoSided = bTwoSided ? 1 : 0;
  }
  TSharedPtr<FJsonObject> Result = FinishPropertyEdit(RequestId, Payload, Target);
  Result->SetStringField(TEXT("assetPath"), Path);
  Result->SetBoolField(TEXT("twoSided"), Target->IsTwoSided());
  Bridge->SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Two-sided set to %s."), bTwoSided ? TEXT("true") : TEXT("false")), Result);
  return true;
}
}
