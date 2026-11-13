#include "Domains/MaterialAuthoring/McpMatPrivate.h"
#include "Foundation/BridgeHelpers/Reflection/McpEnumName.h"

namespace McpMaterialAuthoringHandlers
{
namespace
{
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

// Value is the enumerator without its prefix ("UI" for MD_UI), its full name, or the label the Details panel shows
// ("User Interface"), in any case and with or without spaces; a Hidden entry is not offered.
bool ParseMaterialDomain(const FString& Value, EMaterialDomain& Out) { return McpParseEnum(Value, Out, /*bHidden=*/false); }
bool ParseBlendMode(const FString& Value, EBlendMode& Out) { return McpParseEnum(Value, Out, /*bHidden=*/false); }
bool ParseShadingModel(const FString& Value, EMaterialShadingModel& Out) { return McpParseEnum(Value, Out, /*bHidden=*/false); }
FString ValidMaterialDomains() { return ValidShortNames(StaticEnum<EMaterialDomain>()); }
FString ValidBlendModes() { return ValidShortNames(StaticEnum<EBlendMode>()); }
FString ValidShadingModels() { return ValidShortNames(StaticEnum<EMaterialShadingModel>()); }
} // namespace McpMaterialAuthoringHandlers
