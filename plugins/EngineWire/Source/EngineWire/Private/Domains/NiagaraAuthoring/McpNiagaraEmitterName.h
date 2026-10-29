#pragma once

#include "CoreMinimal.h"
#include "NiagaraEmitter.h"
#include "Runtime/Launch/Resources/Version.h"

/**
 * Give a newly created emitter asset its unique emitter name without renaming the asset.
 *
 * SetUniqueEmitterName renames the object to MakeUniqueObjectName(...) when the name it is given differs from the
 * name it compares against. From UE 5.4 that is, for an asset, GetFName().GetPlainNameString(), which drops a
 * trailing "_<number>": passing the requested name renamed every emitter whose name ends in a number (name
 * "NE_Spark_0" left the package NE_Spark_0 holding the emitter NE_Spark_1 plus a redirector named NE_Spark_0, and
 * "NE_Spark_7" became NE_Spark_0, the first free number from the plain root). The engine's own 5.4+ asset paths
 * (PostRename, PostDuplicate, the emitter factory) pass the plain name, so this does too. UE 5.0-5.3 compare
 * GetName(), so there the full name is the one that keeps the asset's name.
 */
inline void McpNameNewEmitterAsset(UNiagaraEmitter& Emitter)
{
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 4
    Emitter.SetUniqueEmitterName(Emitter.GetFName().GetPlainNameString());
#else
    Emitter.SetUniqueEmitterName(Emitter.GetName());
#endif
}
