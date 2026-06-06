#include "McpPropertyReflectionPrivate.h"

namespace McpPropertyReflection
{
TArray<FString> GetEnumValueNames(UEnum* Enum)
{
    TArray<FString> Names;
    if (!Enum) return Names;

    for (int32 i = 0; i < Enum->NumEnums(); ++i)
    {
        if (!Enum->HasMetaData(TEXT("Hidden"), i) && !Enum->HasMetaData(TEXT("Deprecated"), i))
        {
            Names.Add(Enum->GetNameStringByIndex(i));
        }
    }

    return Names;
}

FString EnumValueToName(UEnum* Enum, int64 Value)
{
    return Enum ? Enum->GetNameStringByValue(Value) : FString();
}

bool EnumNameToValue(UEnum* Enum, const FString& Name, int64& OutValue)
{
    if (!Enum) return false;

    OutValue = Enum->GetValueByNameString(Name);
    if (OutValue != INDEX_NONE) return true;

    const FString FullName = Enum->GenerateFullEnumName(*Name);
    OutValue = Enum->GetValueByName(FName(*FullName));
    return OutValue != INDEX_NONE;
}
}
