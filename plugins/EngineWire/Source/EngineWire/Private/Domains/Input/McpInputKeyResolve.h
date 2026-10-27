#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UInputAction;
class UInputMappingContext;
struct FEnhancedActionKeyMapping;

namespace McpInputHandlers
{
FKey InputKeyFromName(const FString& KeyName);
// The concrete Base subclass Name names, short ("Pressed", "DeadZone") or with its Prefix ("InputTriggerPressed").
UClass* ResolveInputClass(const FString& Name, const TCHAR* Prefix, UClass* Base);
// Context's mapping of Action to Key, or null.
FEnhancedActionKeyMapping* FindInputMapping(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key);
}
