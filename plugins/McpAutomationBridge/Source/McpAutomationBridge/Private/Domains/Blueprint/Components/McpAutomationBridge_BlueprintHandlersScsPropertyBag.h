#pragma once

#include "CoreMinimal.h"

#include "Components/PrimitiveComponent.h"
#include "Dom/JsonObject.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsPropagate.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApply.h"

// The `properties` bag of one edit_scs operation, applied to a component template.
//
// A key that named no property, or a value its property refused, used to be
// skipped without a word while the operation still reported success, so a
// prefab could come out with half its settings missing and nothing said so.
// Every such key is now returned as "name: reason".
//
// Collision keys go through the component's setters (McpApplyCollisionSetterKey),
// and every other key through the shared McpResolvePropertyPath, so a bare name
// that lives in a struct (CastShadow is flat, CollisionEnabled is not) resolves.
namespace McpScsPropertyBag
{
/** Applies Props to Template. Returns the keys that did not land, as "name: reason". */
inline TArray<FString> Apply(UActorComponent *Template, const TSharedPtr<FJsonObject> &Props,
                             McpScsPropagate::FDefaults &Defaults, bool &bAnyApplied)
{
  TArray<FString> Rejected;
  // Iterate Values directly: UE 5.8 keys the map by UE::FSharedString, 5.7 by
  // FString; *Pair.Key is const TCHAR* on both.
  for (const auto &Pair : Props->Values)
  {
    if (!Pair.Value.IsValid())
      continue;
    const FString Name(*Pair.Key);
    FString Error;
    if (McpIsCollisionSetterKey(Template, Name))
    {
      Defaults.CaptureCollision();
      if (McpApplyCollisionSetterKey(Template, Name, Pair.Value, Error))
        bAnyApplied = true;
      else
        Rejected.Add(FString::Printf(TEXT("%s: %s"), *Name, *Error));
      continue;
    }
    void *Container = nullptr;
    FString ResolvedPath;
    FProperty *Property = McpResolvePropertyPath(Template, Name, Container, ResolvedPath, Error);
    if (!Property || !Container)
    {
      Rejected.Add(FString::Printf(TEXT("%s: %s"), *Name,
                                   Error.IsEmpty() ? TEXT("no such property on this component") : *Error));
      continue;
    }
    Defaults.Capture(ResolvedPath);
    FString Failure;
    if (ApplyJsonValueToProperty(Container, Property, Pair.Value, Failure))
    {
      bAnyApplied = true;
      continue;
    }
    Rejected.Add(FString::Printf(TEXT("%s: %s"), *Name, Failure.IsEmpty() ? TEXT("value refused") : *Failure));
  }
  return Rejected;
}
}
