#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

static inline FProperty *McpFindPropertyRecursive(UClass *StartClass,
                                                  FName PropName) {
  for (UClass *Class = StartClass; Class != nullptr;
       Class = Class->GetSuperClass()) {
    if (FProperty *Property = Class->FindPropertyByName(PropName)) {
      return Property;
    }
  }
  return nullptr;
}
