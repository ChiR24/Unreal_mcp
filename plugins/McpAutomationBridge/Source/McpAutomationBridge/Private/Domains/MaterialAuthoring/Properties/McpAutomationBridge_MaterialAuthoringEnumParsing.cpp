#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
namespace
{
template <typename TEnum, size_t N>
bool ParseEnumName(const FString& Value, const TPair<const TCHAR*, TEnum> (&Table)[N], TEnum& Out)
{
  for (const TPair<const TCHAR*, TEnum>& Entry : Table) {
    if (Value == Entry.Key) {
      Out = Entry.Value;
      return true;
    }
  }
  return false;
}
} // namespace

bool ParseMaterialDomain(const FString& Value, EMaterialDomain& Out)
{
  static const TPair<const TCHAR*, EMaterialDomain> Table[] = {
      {TEXT("Surface"), MD_Surface},       {TEXT("DeferredDecal"), MD_DeferredDecal},
      {TEXT("LightFunction"), MD_LightFunction}, {TEXT("Volume"), MD_Volume},
      {TEXT("PostProcess"), MD_PostProcess}, {TEXT("UI"), MD_UI},
  };
  return ParseEnumName(Value, Table, Out);
}

bool ParseBlendMode(const FString& Value, EBlendMode& Out)
{
  static const TPair<const TCHAR*, EBlendMode> Table[] = {
      {TEXT("Opaque"), BLEND_Opaque},         {TEXT("Masked"), BLEND_Masked},
      {TEXT("Translucent"), BLEND_Translucent}, {TEXT("Additive"), BLEND_Additive},
      {TEXT("Modulate"), BLEND_Modulate},     {TEXT("AlphaComposite"), BLEND_AlphaComposite},
      {TEXT("AlphaHoldout"), BLEND_AlphaHoldout},
  };
  return ParseEnumName(Value, Table, Out);
}

bool ParseShadingModel(const FString& Value, EMaterialShadingModel& Out)
{
  static const TPair<const TCHAR*, EMaterialShadingModel> Table[] = {
      {TEXT("Unlit"), MSM_Unlit},
      {TEXT("DefaultLit"), MSM_DefaultLit},
      {TEXT("Subsurface"), MSM_Subsurface},
      {TEXT("SubsurfaceProfile"), MSM_SubsurfaceProfile},
      {TEXT("PreintegratedSkin"), MSM_PreintegratedSkin},
      {TEXT("ClearCoat"), MSM_ClearCoat},
      {TEXT("Hair"), MSM_Hair},
      {TEXT("Cloth"), MSM_Cloth},
      {TEXT("Eye"), MSM_Eye},
      {TEXT("TwoSidedFoliage"), MSM_TwoSidedFoliage},
      {TEXT("ThinTranslucent"), MSM_ThinTranslucent},
  };
  return ParseEnumName(Value, Table, Out);
}
} // namespace McpMaterialAuthoringHandlers
#endif
