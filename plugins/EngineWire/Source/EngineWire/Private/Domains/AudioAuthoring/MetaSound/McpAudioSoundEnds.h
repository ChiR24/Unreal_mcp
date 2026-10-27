#pragma once

#include "CoreMinimal.h"

class USoundBase;

namespace McpAudioAuthoring
{
// Why a sound never stops by itself, or empty when it does: a looping wave or cue and a MetaSound without the
// one-shot interface loop, and a one-shot MetaSound whose On Finished output nothing fires plays on just the same
// (its graph is read on 5.5+).
FString DescribeEndlessSound(const USoundBase* Sound);
}
