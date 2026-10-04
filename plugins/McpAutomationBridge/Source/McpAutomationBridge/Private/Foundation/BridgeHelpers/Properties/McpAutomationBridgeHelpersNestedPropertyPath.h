#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

// Up to 8 property names of Scope sharing a word (3+ letters, split at capitals) with Wanted, so a
// miss names what the caller probably meant ("FadeAmount" finds OnAudioFadeChangeEvent); empty
// when nothing shares one. A miss used to say only "not found in scope". A shared word counts by
// how rare it is in Scope, so "LifeTime" lists InitialLifeSpan before the many ...Time properties.
static inline FString McpSimilarPropertyNames(const UStruct *Scope, const FString &Wanted) {
  TArray<FString> Words;
  FString Word;
  for (const TCHAR Char : Wanted + TEXT(" ")) {
    const bool bBreak = FChar::IsUpper(Char) || !FChar::IsAlnum(Char);
    if (bBreak && Word.Len() >= 3) {
      Words.Add(Word);
    }
    if (bBreak) {
      Word.Reset();
    }
    if (FChar::IsAlnum(Char)) {
      Word.AppendChar(Char);
    }
  }
  TArray<FString> Names;
  for (TFieldIterator<FProperty> It(Scope); It; ++It) {
    Names.Add(It->GetName());
  }
  TArray<int32> Holders;
  for (const FString &Each : Words) {
    Holders.Add(Names.FilterByPredicate([&Each](const FString &Name) { return Name.Contains(Each); }).Num());
  }
  TArray<TPair<double, FString>> Scored;
  for (const FString &Name : Names) {
    double Score = 0.0;
    for (int32 Index = 0; Index < Words.Num(); ++Index) {
      Score += Name.Contains(Words[Index]) ? 1.0 / Holders[Index] : 0.0;
    }
    if (Score > 0.0) {
      Scored.Emplace(Score, Name);
    }
  }
  Scored.StableSort([](const TPair<double, FString> &A, const TPair<double, FString> &B) { return A.Key > B.Key; });
  TArray<FString> Similar;
  for (int32 Index = 0; Index < FMath::Min(8, Scored.Num()); ++Index) {
    Similar.Add(Scored[Index].Value);
  }
  return FString::Join(Similar, TEXT(", "));
}

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
  UObject *CurrentObject = RootObject; // null while the walk is inside a struct
  FProperty *CurrentProperty = nullptr;

  for (int32 Index = 0; Index < PathSegments.Num(); ++Index) {
    const FString &Segment = PathSegments[Index];
    const bool bIsLastSegment = Index == PathSegments.Num() - 1;

    CurrentProperty =
        FindFProperty<FProperty>(CurrentTypeScope, FName(*Segment));
    // An actor's instance components (placed in the level, or added by AddInstanceComponent) have no property
    // behind them, so a middle segment may name one exactly: the component name a create call reported.
    AActor *Actor = !CurrentProperty && !bIsLastSegment ? Cast<AActor>(CurrentObject) : nullptr;
    if (Actor) {
      TArray<UActorComponent *> Components;
      Actor->GetComponents(Components);
      if (UActorComponent *const *Named = Components.FindByPredicate([&Segment](const UActorComponent *Component) {
            return Component && Component->GetName().Equals(Segment, ESearchCase::IgnoreCase);
          })) {
        CurrentObject = *Named;
        CurrentContainer = *Named;
        CurrentTypeScope = (*Named)->GetClass();
        continue;
      }
    }
    if (!CurrentProperty) {
      const FString Similar = McpSimilarPropertyNames(CurrentTypeScope, Segment);
      OutError = FString::Printf(
          TEXT("Property '%s' not found in scope '%s' (segment %d of %d)%s"),
          *Segment, *CurrentTypeScope->GetName(), Index + 1,
          PathSegments.Num(), Similar.IsEmpty() ? TEXT("") : *(TEXT("; similar: ") + Similar));
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
      CurrentObject = NextObject;
      CurrentTypeScope = NextObject->GetClass();
    } else if (FStructProperty *StructProperty =
                   CastField<FStructProperty>(CurrentProperty)) {
      CurrentContainer =
          StructProperty->ContainerPtrToValuePtr<void>(CurrentContainer);
      CurrentObject = nullptr;
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
