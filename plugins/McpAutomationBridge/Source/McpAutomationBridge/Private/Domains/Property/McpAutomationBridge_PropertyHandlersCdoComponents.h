#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Engine/InheritableComponentHandler.h"

class UActorComponent;
class UBlueprint;

namespace McpPropertyCdoComponents
{
TSharedPtr<FJsonObject> BuildComponentSummary(
    UActorComponent* Template,
    const FString& DisplayName,
    const FString& Source,
    bool bDetailed,
    const TArray<FName>& PropertyFilter);

TMap<FString, FString> BuildScsSourceMap(UBlueprint* Blueprint);

/**
 * Every name FindCdoComponent() would accept, in a stable order: native object
 * names, the UPROPERTY aliases that point at them (ACharacter's `Mesh`), and
 * SCS variable names from this Blueprint and its parents. Used to turn a bare
 * COMPONENT_NOT_FOUND into a one-round-trip fix.
 */
TArray<FString> CollectResolvableComponentNames(UBlueprint* Blueprint, UObject* CDO);

UActorComponent* FindCdoComponent(
    UBlueprint* Blueprint,
    UObject* CDO,
    const FString& ComponentName,
    bool bCreateInheritedOverride,
    UInheritableComponentHandler** OutCreatedInheritedOverrideHandler = nullptr,
    FComponentKey* OutCreatedInheritedOverrideKey = nullptr,
    bool* bOutFoundComponent = nullptr);
}
#endif
