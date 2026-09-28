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

// Appends the dotted path of every struct member named Name below Scope, at any
// depth. Only by-value structs are walked: they cannot contain themselves, and
// following object references would reach unrelated objects.
static inline void McpCollectNestedMemberPaths(const UStruct *Scope, const FName Name,
                                               const FString &Prefix, TArray<FString> &Out) {
  for (TFieldIterator<FStructProperty> It(Scope); It; ++It) {
    if (!It->Struct) {
      continue;
    }
    const FString Path = Prefix + It->GetName() + TEXT(".");
    if (const FProperty *Member = FindFProperty<FProperty>(It->Struct, Name)) {
      Out.Add(Path + Member->GetName());
    }
    McpCollectNestedMemberPaths(It->Struct, Name, Path, Out);
  }
}

/**
 * The one resolver the property get and set actions share (component, SCS
 * template and object properties). A dotted path walks structs and object
 * references to any depth (BodyInstance.CollisionEnabled,
 * LightmassSettings.bShadowIndirectOnly). A bare name that is not a property of
 * the object itself is looked for among its struct members at any depth and
 * resolves when exactly one carries it; several matches are an error naming
 * them. OutResolvedPath is the full path that resolved.
 */
static inline FProperty *McpResolvePropertyPath(UObject *RootObject, const FString &PropertyPath,
                                                void *&OutContainerPtr, FString &OutResolvedPath,
                                                FString &OutError) {
  OutResolvedPath = PropertyPath;
  FProperty *Property = ResolveNestedPropertyPath(RootObject, PropertyPath, OutContainerPtr, OutError);
  if (Property || !RootObject || PropertyPath.IsEmpty() || PropertyPath.Contains(TEXT("."))) {
    return Property;
  }
  TArray<FString> Candidates;
  McpCollectNestedMemberPaths(RootObject->GetClass(), FName(*PropertyPath), FString(), Candidates);
  if (Candidates.Num() == 1) {
    OutResolvedPath = Candidates[0];
    return ResolveNestedPropertyPath(RootObject, Candidates[0], OutContainerPtr, OutError);
  }
  if (Candidates.Num() > 1) {
    OutError = FString::Printf(TEXT("'%s' matches several nested properties; name the full path, one of: %s"),
                               *PropertyPath, *FString::Join(Candidates, TEXT(", ")));
  }
  return nullptr;
}
