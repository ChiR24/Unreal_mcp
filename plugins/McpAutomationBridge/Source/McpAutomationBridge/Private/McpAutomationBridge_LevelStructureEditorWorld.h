#pragma once

class UWorld;

namespace LevelStructureHelpers
{
#if WITH_EDITOR
UWorld* GetEditorWorld();
#endif
}
