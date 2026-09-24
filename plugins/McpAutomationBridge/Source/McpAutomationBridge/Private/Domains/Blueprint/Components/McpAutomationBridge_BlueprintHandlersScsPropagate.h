#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectHash.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersComponentLookup.h"
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

  // Number of placed components that took at least one new default; their paths
  // go to OutPaths when given. A value that did not hold is never counted: it
  // goes to OutFailed as "component path: property", read back after the write.
  int32 Propagate(TArray<FString> *OutPaths = nullptr, TArray<FString> *OutFailed = nullptr) const
  {
    if (Old.Num() == 0 || !Blueprint || !Blueprint->GeneratedClass)
      return 0;
    TArray<UObject *> Actors;
    Blueprint->GeneratedClass->GetDefaultObject()->GetArchetypeInstances(Actors);
    int32 Updated = 0;
    for (UObject *Object : Actors)
    {
      // Placed actors only. The Content Browser's thumbnail scene spawns an
      // instance of the class too, and counting it made instancesUpdated claim a
      // change the level never received.
      AActor *Actor = Cast<AActor>(Object);
      const UWorld *World = IsValid(Actor) ? Actor->GetWorld() : nullptr;
      if (!World || World->WorldType != EWorldType::Editor)
        continue;
      UObject *Component = static_cast<UObject *>(FindObjectWithOuter(Actor, Template->GetClass(), ComponentName));
      // An inherited native component is found by its object name, not the
      // property name the caller used: ACharacter's CapsuleComponent is
      // "CollisionCylinder" on every instance.
      if (!Component)
        Component = static_cast<UObject *>(FindObjectWithOuter(Actor, Template->GetClass(), Template->GetFName()));
      if (!IsValid(Component))
        continue;
      // Written the way control_actor edit_component writes a live component,
      // the path whose values are known to hold: Modify, a typed copy of the
      // template's value, then the shared render refresh. The previous
      // no-Modify, text-import, full-reregister write reported every bool
      // (bVisible, CastShadow) as updated while the component kept its old value.
      TArray<TPair<FString, FString>> Written;
      for (const TPair<FString, FString> &Entry : Old)
      {
        FString NewText, Current;
        void *Container = nullptr;
        void *TemplateContainer = nullptr;
        FString Error;
        FProperty *Prop = ResolveNestedPropertyPath(Component, Entry.Key, Container, Error);
        FProperty *TemplateProp = ResolveNestedPropertyPath(Template, Entry.Key, TemplateContainer, Error);
        if (!Prop || !Container || Prop != TemplateProp || !TemplateContainer ||
            !ExportNested(Template, Entry.Key, NewText) || NewText == Entry.Value ||
            !ExportNested(Component, Entry.Key, Current) || Current != Entry.Value)
          continue;
        if (Written.Num() == 0)
          Component->Modify();
        // An instanced subobject must not be shared with the template, so those
        // still go through text (which resolves to a reference, as before).
        if (Prop->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference))
          Prop->ImportText_Direct(*NewText, Prop->ContainerPtrToValuePtr<void>(Container), Component, PPF_None);
        else
          Prop->CopyCompleteValue_InContainer(Container, TemplateContainer);
        Written.Emplace(Entry.Key, NewText);
      }
      if (Written.Num() == 0)
        continue;
      UActorComponent *Live = Cast<UActorComponent>(Component);
      McpRefreshComponentAfterEdit(Live);
      if (Live)
        Live->MarkPackageDirty();
      bool bAllHeld = true;
      for (const TPair<FString, FString> &Entry : Written)
      {
        FString After;
        if (ExportNested(Component, Entry.Key, After) && After == Entry.Value)
          continue;
        bAllHeld = false;
        if (OutFailed)
          OutFailed->Add(FString::Printf(TEXT("%s: %s"), *Component->GetPathName(), *Entry.Key));
      }
      if (!bAllHeld)
        continue;
      if (OutPaths)
        OutPaths->Add(Component->GetPathName());
      ++Updated;
    }
    return Updated;
  }
};

// A later operation in the same batch (any add_component) recompiles the
// Blueprint and re-creates the placed actors, and a placed value that still
// differs from the new template then reads as a deliberate override from that
// point on (a template hidden in op 3 stayed visible on the level instance).
// Every batch operation's defaults are kept here and pushed once more after the
// batch's final compile; an instance that already took the new value, or that
// someone overrode, is left alone by the same old-value test, and one that
// still will not take it is named in the batch's warnings.
inline TArray<TPair<TWeakObjectPtr<UObject>, FDefaults>> &Pending()
{
  static TArray<TPair<TWeakObjectPtr<UObject>, FDefaults>> List;
  return List;
}

inline int32 RepropagatePending(TArray<FString> *OutFailed = nullptr)
{
  int32 Updated = 0;
  for (const TPair<TWeakObjectPtr<UObject>, FDefaults> &Entry : Pending())
  {
    if (Entry.Key.IsValid() && IsValid(Entry.Value.Blueprint))
      Updated += Entry.Value.Propagate(nullptr, OutFailed);
  }
  Pending().Reset();
  return Updated;
}

inline TArray<TSharedPtr<FJsonValue>> ToJsonStrings(const TArray<FString> &In)
{
  TArray<TSharedPtr<FJsonValue>> Out;
  for (const FString &Text : In)
    Out.Add(MakeShared<FJsonValueString>(Text));
  return Out;
}

// Push one operation's new defaults to the placed instances and record the ones
// that took them. One that did not is retried, and named if it still did not,
// after the batch's final compile (RepropagatePending).
inline void PropagateAndReport(const FDefaults &Defaults, const TSharedPtr<FJsonObject> &OpSummary)
{
  TArray<FString> UpdatedPaths;
  if (const int32 Updated = Defaults.Propagate(&UpdatedPaths))
  {
    OpSummary->SetNumberField(TEXT("instancesUpdated"), Updated);
    OpSummary->SetArrayField(TEXT("updatedInstances"), ToJsonStrings(UpdatedPaths));
  }
}
}
#endif
