#pragma once

// McpHandlerUtilsPropertyResolve.h — resolve a property name to its FProperty
// and owning container.
//
// Every container-mutation handler (array append/insert/element/remove, map
// get/set/remove/keys, set add/remove/query) opened with the SAME 22-line
// prologue: branch on whether the name is a dotted path, call
// ResolveNestedPropertyPath or FindPropertyByName, and send one of two errors.
// It appeared 15 times across 10 files in two spellings that differed only in
// line wrapping, so a fix to the resolution rule had to be applied fifteen times.
//
// The error is REPORTED, not sent: these handlers are subsystem methods and
// SendAutomationError is a subsystem member, so returning the message keeps this
// a free function with no dependency on the subsystem type.

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"

/**
 * Resolve `PropertyName` against `RootObject`.
 *
 * A name containing '.' is treated as a nested path; anything else is looked up
 * directly on the object's class. On success returns the property and sets
 * `OutContainer` to the memory that owns it. On failure returns nullptr and
 * fills `OutError`/`OutErrorCode` for the caller to pass to SendAutomationError.
 */
static inline FProperty *McpResolvePropertyContainer(UObject *RootObject,
                                                     const FString &PropertyName,
                                                     void *&OutContainer,
                                                     FString &OutError,
                                                     FString &OutErrorCode) {
  OutContainer = nullptr;
  OutError.Empty();
  OutErrorCode = TEXT("PROPERTY_NOT_FOUND");

  if (!RootObject) {
    OutError = TEXT("Property not found.");
    return nullptr;
  }

  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    FProperty *Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                                    OutContainer, ResolveError);
    if (!Property || !OutContainer) {
      OutError = FString::Printf(TEXT("Failed to resolve property: %s"), *ResolveError);
      return nullptr;
    }
    return Property;
  }

  OutContainer = RootObject;
  FProperty *Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
  if (!Property) {
    OutError = TEXT("Property not found.");
    return nullptr;
  }
  return Property;
}
