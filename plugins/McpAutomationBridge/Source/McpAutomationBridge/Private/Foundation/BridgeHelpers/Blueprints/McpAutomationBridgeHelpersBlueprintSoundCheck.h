#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class UBlueprint;

// A sound played at a location (PlaySoundAtLocation, SpawnSoundAtLocation, SpawnSoundAttached) with no attenuation, none
// on the node and none on the sound, is heard at full volume at any distance: a splash meant for one puddle played across
// the whole level. The compiler has no word for it, so a compile adds a warning diagnostic per such node while
// Diagnostics holds fewer than MaxDiagnostics. Only a sound chosen on the node itself is judged; one wired in is left alone.
void McpAddUnattenuatedSoundWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics);
