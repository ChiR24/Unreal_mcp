#pragma once

#include "MaterialShared.h"
#include "Materials/Material.h"

// PostEditChange on an edited material (or material function) as the material editor's Apply runs it, inside an update
// context: its end recompiles every loaded instance of the material that compiles shaders of its own (a static switch or
// a base property override, usage flags included), which a bare PostEditChange left drawing the material's old graph.
inline void McpRecompileMaterialHost(UObject *Host)
{
  FMaterialUpdateContext Context;
  if (UMaterial *Material = Cast<UMaterial>(Host)) {
    Context.AddMaterial(Material);
  }
  Host->PostEditChange();
}
