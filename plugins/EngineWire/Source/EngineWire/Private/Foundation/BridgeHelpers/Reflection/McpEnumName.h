#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

// An enumerator's short name ("HasDuration", "BLEND_Opaque"); empty when the value names no enumerator.
template <typename TEnum>
FString McpEnumName(TEnum Value)
{
    const UEnum* Enum = StaticEnum<TEnum>();
    return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : FString();
}

// The same for a TEnumAsByte field (most UPROPERTY enums of the engine's older classes).
template <typename TEnum>
FString McpEnumName(TEnumAsByte<TEnum> Value)
{
    return McpEnumName(Value.GetValue());
}

// A caller's word for one of Enum's enumerators: a name as the engine reads one (COND_InitialOnly, HasDuration,
// ENetRole::ROLE_Authority), the name after its prefix (InitialOnly) or the label the Details panel shows ("Initial
// Only"); case, spaces, underscores and hyphens ignored. The MAX entry the header tool adds never matches, and a Hidden
// entry only with bHidden. False, OutValue untouched, when nothing matches.
inline bool McpParseEnumToken(const UEnum* Enum, const FString& Token, int64& OutValue, bool bHidden = true)
{
    const auto Key = [](const FString& Text)
    {
        return Text.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("")).Replace(TEXT("-"), TEXT("")).ToLower();
    };
    const FString Trimmed = Token.TrimStartAndEnd();
    const FString Wanted = Key(Trimmed);
    const int32 Last = Enum ? Enum->NumEnums() - 1 : 0;
    const auto Takes = [Enum, Last, bHidden](int32 Index)
    {
        return Index >= 0 && Index < Last && (bHidden || !Enum->HasMetaData(TEXT("Hidden"), Index));
    };
    int32 Found = Enum && !Wanted.IsEmpty() ? Enum->GetIndexByNameString(Trimmed) : INDEX_NONE;
    for (int32 Index = 0; !Wanted.IsEmpty() && !Takes(Found) && Index < Last; ++Index)
    {
        const FString Name = Enum->GetNameStringByIndex(Index);
        int32 Underscore = INDEX_NONE;
        const bool bNamed = Key(Name) == Wanted || (Name.FindChar(TEXT('_'), Underscore) && Key(Name.Mid(Underscore + 1)) == Wanted) ||
                            Key(Enum->GetDisplayNameTextByIndex(Index).ToString()) == Wanted;
        Found = bNamed ? Index : INDEX_NONE;
    }
    if (!Takes(Found))
    {
        return false;
    }
    OutValue = Enum->GetValueByIndex(Found);
    return true;
}

// McpParseEnumToken for TEnum, into Out.
template <typename TEnum>
bool McpParseEnum(const FString& Token, TEnum& Out, bool bHidden = true)
{
    int64 Value = 0;
    if (!McpParseEnumToken(StaticEnum<TEnum>(), Token, Value, bHidden))
    {
        return false;
    }
    Out = static_cast<TEnum>(Value);
    return true;
}
