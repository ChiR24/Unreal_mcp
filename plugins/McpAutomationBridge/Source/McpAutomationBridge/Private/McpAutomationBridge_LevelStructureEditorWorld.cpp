#include "McpAutomationBridge_LevelStructureEditorWorld.h"

#if WITH_EDITOR
#include "Editor.h"

namespace LevelStructureHelpers
{
UWorld* GetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}
}
#endif
