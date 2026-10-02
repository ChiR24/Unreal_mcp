#pragma once

#include "CoreMinimal.h"

class FJsonValue;
class UObject;

namespace McpPropertyCdoPropagation
{
// The live copies of a class default or component template (placed actors, their components) whose value at Path
// still equals the template's, read before the template is written: they never overrode the default, so a write to
// it reaches them as the editor's details panel makes it. The Blueprint compile alone kept their old value, because
// by then the old default object already held the new one and the copies looked overridden.
TArray<UObject*> CollectFollowers(UObject* Template, const FString& Path);

// Writes Value at Path on each follower collected before the template changed; returns how many took it.
int32 ApplyToFollowers(const TArray<UObject*>& Followers, const FString& Path, const TSharedPtr<FJsonValue>& Value);
}
