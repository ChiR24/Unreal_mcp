#pragma once

#include "Containers/ScriptArray.h"
#include "CoreMinimal.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "UObject/UnrealType.h"

static inline USCS_Node *FindScsNodeByName(USimpleConstructionScript *SCS,
                                           const FString &Name) {
  if (!SCS || Name.IsEmpty())
    return nullptr;

  UClass *SCSClass = SCS->GetClass();
  if (!SCSClass)
    return nullptr;

  FArrayProperty *ArrayProperty =
      FindFProperty<FArrayProperty>(SCSClass, TEXT("AllNodes"));
  if (!ArrayProperty)
    return nullptr;

  FObjectProperty *ObjectProperty =
      CastField<FObjectProperty>(ArrayProperty->Inner);
  if (!ObjectProperty)
    return nullptr;

  FScriptArrayHelper Helper(
      ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(SCS));
  for (int32 Index = 0; Index < Helper.Num(); ++Index) {
    void *ElementPtr = Helper.GetRawPtr(Index);
    if (!ElementPtr)
      continue;

    UObject *ElementObject =
        ObjectProperty->GetObjectPropertyValue(ElementPtr);
    if (!ElementObject)
      continue;

    FProperty *VariableProperty =
        ElementObject->GetClass()->FindPropertyByName(TEXT("VariableName"));
    if (FNameProperty *NameProperty =
            CastField<FNameProperty>(VariableProperty)) {
      const FName VariableName =
          NameProperty->GetPropertyValue_InContainer(ElementObject);
      if (!VariableName.IsNone() &&
          VariableName.ToString().Equals(Name, ESearchCase::IgnoreCase)) {
        return reinterpret_cast<USCS_Node *>(ElementObject);
      }
    }

    if (ElementObject->GetName().Equals(Name, ESearchCase::IgnoreCase)) {
      return reinterpret_cast<USCS_Node *>(ElementObject);
    }
  }

  return nullptr;
}
