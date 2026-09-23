#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "UObject/UObjectHash.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"

// A component template edit only rewrote the class default: placed actors copy
// their old component values across the recompile, so the change never reached
// an actor already in a level (a widened trigger stayed narrow in play). Do what
// the Blueprint editor does -- an instance still holding the old default takes
// the new one; an instance someone overrode keeps its value.
//
// SCS components are DUPLICATED from their template, not instanced from it, so
// the template has no archetype instances to walk. Like the engine's fallback in
// FComponentEditorUtils::PropagateDefaultValueChange, go through the owning
// class's instances and find each one's component by name.
namespace McpScsPropagate
{
inline bool ExportNested(UObject *Obj, const FString &Path, FString &Out)
{
  void *Container = nullptr;
  FString Error;
  FProperty *Prop = ResolveNestedPropertyPath(Obj, Path, Container, Error);
  if (!Prop || !Container)
    return false;
  Out.Reset();
  Prop->ExportText_InContainer(0, Out, Container, nullptr, Obj, PPF_None);
  return true;
}

// Old values captured before the template changes, pushed out afterwards.
struct FDefaults
{
  UObject *Template = nullptr;
  UBlueprint *Blueprint = nullptr;
  FName ComponentName;
  TArray<TPair<FString, FString>> Old;

  void Capture(const FString &Path)
  {
    FString Text;
    if (ExportNested(Template, Path, Text))
      Old.Emplace(Path, Text);
  }

  // Number of placed components that took at least one new default.
  int32 Propagate() const
  {
    if (Old.Num() == 0 || !Blueprint || !Blueprint->GeneratedClass)
      return 0;
    TArray<UObject *> Actors;
    Blueprint->GeneratedClass->GetDefaultObject()->GetArchetypeInstances(Actors);
    int32 Updated = 0;
    for (UObject *Actor : Actors)
    {
      UObject *Component = IsValid(Actor)
          ? static_cast<UObject *>(FindObjectWithOuter(Actor, Template->GetClass(), ComponentName))
          : nullptr;
      // An inherited native component is found by its object name, not the
      // property name the caller used: ACharacter's CapsuleComponent is
      // "CollisionCylinder" on every instance.
      if (!Component && IsValid(Actor))
        Component = static_cast<UObject *>(FindObjectWithOuter(Actor, Template->GetClass(), Template->GetFName()));
      if (!IsValid(Component))
        continue;
      bool bChanged = false;
      for (const TPair<FString, FString> &Entry : Old)
      {
        FString NewText, Current;
        void *Container = nullptr;
        FString Error;
        FProperty *Prop = ResolveNestedPropertyPath(Component, Entry.Key, Container, Error);
        if (!Prop || !Container || !ExportNested(Template, Entry.Key, NewText) || NewText == Entry.Value ||
            !ExportNested(Component, Entry.Key, Current) || Current != Entry.Value)
          continue;
        Component->Modify();
        Prop->ImportText_Direct(*NewText, Prop->ContainerPtrToValuePtr<void>(Container), Component, PPF_None);
        bChanged = true;
      }
      if (!bChanged)
        continue;
      if (USceneComponent *Scene = Cast<USceneComponent>(Component))
        Scene->UpdateComponentToWorld();
      Component->PostEditChange();
      ++Updated;
    }
    return Updated;
  }
};
}
#endif
