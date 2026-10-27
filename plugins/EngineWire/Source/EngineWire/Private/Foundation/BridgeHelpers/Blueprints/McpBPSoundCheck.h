#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class UBlueprint;

// Sound nodes the compiler has no word for, one warning diagnostic per node while Diagnostics holds fewer than
// MaxDiagnostics. A sound played at a location (PlaySoundAtLocation, SpawnSoundAtLocation, SpawnSoundAttached) with no
// attenuation, none on the node and none on the sound, is heard at full volume at any distance: a splash meant for one
// puddle played across the whole level. PlaySound2D and PlaySoundAtLocation hand back nothing to stop, so a sound that
// never ends by itself (a looping one, a one-shot MetaSound that never fires On Finished) played by one runs until the
// level unloads, one more copy per call. Only a sound chosen on the node itself is judged; one wired in is left alone.
void McpAddSoundNodeWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics);
