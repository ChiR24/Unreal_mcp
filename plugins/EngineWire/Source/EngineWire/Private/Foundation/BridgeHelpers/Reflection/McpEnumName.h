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
