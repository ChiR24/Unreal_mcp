#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
namespace
{
// Value is the enumerator without its prefix ("Surface" for MD_Surface).
template <typename TEnum>
bool ParseEnumShortName(const FString& Value, const TCHAR* Prefix, TEnum& Out)
{
  const UEnum* Enum = StaticEnum<TEnum>();
  const int32 Index = Enum ? Enum->GetIndexByNameString(FString(Prefix) + Value) : INDEX_NONE;
  if (Index == INDEX_NONE || Index >= Enum->NumEnums() - 1 || Enum->HasMetaData(TEXT("Hidden"), Index)) {
    return false;
  }
  Out = static_cast<TEnum>(Enum->GetValueByIndex(Index));
  return true;
}

// Every enumerator's short name, for "valid values" messages.
FString ValidShortNames(const UEnum* Enum)
{
  TArray<FString> Names;
  for (int32 Index = 0; Enum && Index < Enum->NumEnums() - 1; ++Index) {
    if (Enum->HasMetaData(TEXT("Hidden"), Index)) continue;
    Names.Add(MaterialEnumShortName(Enum, Enum->GetValueByIndex(Index)));
  }
  return FString::Join(Names, TEXT(", "));
}
} // namespace

FString MaterialEnumShortName(const UEnum* Enum, int64 Value)
{
  FString Name = Enum ? Enum->GetNameStringByValue(Value) : FString();
  int32 Underscore = INDEX_NONE;
  return Name.FindChar(TEXT('_'), Underscore) ? Name.Mid(Underscore + 1) : (Name.IsEmpty() ? TEXT("Unknown") : Name);
}

bool ParseMaterialDomain(const FString& Value, EMaterialDomain& Out) { return ParseEnumShortName(Value, TEXT("MD_"), Out); }
bool ParseBlendMode(const FString& Value, EBlendMode& Out) { return ParseEnumShortName(Value, TEXT("BLEND_"), Out); }
bool ParseShadingModel(const FString& Value, EMaterialShadingModel& Out) { return ParseEnumShortName(Value, TEXT("MSM_"), Out); }
FString ValidMaterialDomains() { return ValidShortNames(StaticEnum<EMaterialDomain>()); }
FString ValidBlendModes() { return ValidShortNames(StaticEnum<EBlendMode>()); }
FString ValidShadingModels() { return ValidShortNames(StaticEnum<EMaterialShadingModel>()); }
} // namespace McpMaterialAuthoringHandlers
