#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

static inline FProperty *ResolveNestedPropertyPath(UObject *RootObject,
                                                   const FString &PropertyPath,
                                                   void *&OutContainerPtr,
                                                   FString &OutError) {
  OutError.Empty();
  OutContainerPtr = nullptr;

  if (!RootObject) {
    OutError = TEXT("Root object is null");
    return nullptr;
  }

  if (PropertyPath.IsEmpty()) {
    OutError = TEXT("Property path is empty");
    return nullptr;
  }

  TArray<FString> PathSegments;
  PropertyPath.ParseIntoArray(PathSegments, TEXT("."), true);
  if (PathSegments.Num() == 0) {
    OutError = TEXT("Invalid property path format");
    return nullptr;
  }

  UStruct *CurrentTypeScope = RootObject->GetClass();
  void *CurrentContainer = RootObject;
  FProperty *CurrentProperty = nullptr;

  for (int32 Index = 0; Index < PathSegments.Num(); ++Index) {
    const FString &Segment = PathSegments[Index];
    const bool bIsLastSegment = Index == PathSegments.Num() - 1;

    CurrentProperty =
        FindFProperty<FProperty>(CurrentTypeScope, FName(*Segment));
    if (!CurrentProperty) {
      OutError = FString::Printf(
          TEXT("Property '%s' not found in scope '%s' (segment %d of %d)"),
          *Segment, *CurrentTypeScope->GetName(), Index + 1,
          PathSegments.Num());
      return nullptr;
    }

    if (bIsLastSegment) {
      OutContainerPtr = CurrentContainer;
      return CurrentProperty;
    }

    if (FObjectProperty *ObjectProperty =
            CastField<FObjectProperty>(CurrentProperty)) {
      UObject *NextObject =
          ObjectProperty->GetObjectPropertyValue_InContainer(CurrentContainer);
      if (!NextObject) {
        OutError = FString::Printf(
            TEXT("Object property '%s' is null (segment %d of %d)"), *Segment,
            Index + 1, PathSegments.Num());
        return nullptr;
      }
      CurrentContainer = NextObject;
      CurrentTypeScope = NextObject->GetClass();
    } else if (FStructProperty *StructProperty =
                   CastField<FStructProperty>(CurrentProperty)) {
      CurrentContainer =
          StructProperty->ContainerPtrToValuePtr<void>(CurrentContainer);
      CurrentTypeScope = StructProperty->Struct;
    } else {
      OutError = FString::Printf(
          TEXT("Cannot traverse into property '%s' of type '%s'"), *Segment,
          *CurrentProperty->GetClass()->GetName());
      return nullptr;
    }
  }

  OutError = TEXT("Unexpected end of property path resolution");
  return nullptr;
}
