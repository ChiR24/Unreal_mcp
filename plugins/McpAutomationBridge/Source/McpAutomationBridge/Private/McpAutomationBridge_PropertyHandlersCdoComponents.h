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
