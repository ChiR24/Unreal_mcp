#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
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
// Collision is the case callers hit most: it lives inside BodyInstance behind
// setters, so the flat names (CollisionProfileName, CollisionEnabled) matched no
// property at all, and writing the nested BodyInstance fields directly bypassed
// the profile bookkeeping. Both spellings now go through the component's own
// setters.
namespace McpScsPropertyBag
{
inline bool IsCollisionKey(const FString &Name, const TCHAR *Field)
{
  return Name == Field || Name == FString(TEXT("BodyInstance.")) + Field;
}

/** Applies Props to Template. Returns the keys that did not land, as "name: reason". */
inline TArray<FString> Apply(UActorComponent *Template, const TSharedPtr<FJsonObject> &Props,
                             McpScsPropagate::FDefaults &Defaults, bool &bAnyApplied)
{
  TArray<FString> Rejected;
  UPrimitiveComponent *Primitive = Cast<UPrimitiveComponent>(Template);
  // Iterate Values directly: UE 5.8 keys the map by UE::FSharedString, 5.7 by
  // FString; *Pair.Key is const TCHAR* on both.
  for (const auto &Pair : Props->Values)
  {
    if (!Pair.Value.IsValid())
      continue;
    const FString Name(*Pair.Key);
    const bool bProfile = IsCollisionKey(Name, TEXT("CollisionProfileName"));
    if (Primitive && (bProfile || IsCollisionKey(Name, TEXT("CollisionEnabled"))))
    {
      if (Pair.Value->Type != EJson::String)
      {
        Rejected.Add(FString::Printf(TEXT("%s: expects a string (a profile or ECollisionEnabled name)"), *Name));
        continue;
      }
      const FString Text = Pair.Value->AsString();
      Defaults.Capture(TEXT("BodyInstance.CollisionProfileName"));
      Defaults.Capture(TEXT("BodyInstance.CollisionEnabled"));
      if (bProfile)
      {
        Primitive->SetCollisionProfileName(FName(*Text));
        bAnyApplied = true;
        continue;
      }
      const int64 Value = StaticEnum<ECollisionEnabled::Type>()->GetValueByNameString(Text);
      if (Value == INDEX_NONE)
      {
        Rejected.Add(FString::Printf(TEXT("%s: '%s' is not an ECollisionEnabled value (NoCollision, QueryOnly, "
                                          "PhysicsOnly, QueryAndPhysics)"), *Name, *Text));
        continue;
      }
      Primitive->SetCollisionEnabled(static_cast<ECollisionEnabled::Type>(Value));
      bAnyApplied = true;
      continue;
    }
    void *Container = nullptr;
    FString Error;
    FProperty *Property = ResolveNestedPropertyPath(Template, Name, Container, Error);
    if (!Property || !Container)
    {
      Rejected.Add(FString::Printf(TEXT("%s: %s"), *Name,
                                   Error.IsEmpty() ? TEXT("no such property on this component") : *Error));
      continue;
    }
    Defaults.Capture(Name);
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
#endif
