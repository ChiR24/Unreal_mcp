#pragma once

#include "CoreMinimal.h"
#include "NiagaraEmitter.h"

/**
 * Give a newly created emitter asset its unique emitter name without renaming the asset.
 *
 * For an asset, SetUniqueEmitterName compares the name it is given with GetFName().GetPlainNameString(), which
 * drops a trailing "_<number>", and renames the object to MakeUniqueObjectName(...) when they differ. Passing the
 * requested name therefore renamed every emitter whose name ends in a number: name "NE_Spark_0" left the package
 * NE_Spark_0 holding the emitter NE_Spark_1 plus a redirector named NE_Spark_0, and "NE_Spark_7" became NE_Spark_0
 * (the first free number from the plain root). The engine's own asset paths (PostRename, PostDuplicate, the emitter
 * factory) pass the plain name, so this does too.
 */
inline void McpNameNewEmitterAsset(UNiagaraEmitter& Emitter)
{
    Emitter.SetUniqueEmitterName(Emitter.GetFName().GetPlainNameString());
}
