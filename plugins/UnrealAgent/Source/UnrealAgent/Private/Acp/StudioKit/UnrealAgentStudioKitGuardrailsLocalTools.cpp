#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

namespace UnrealAgentStudioKit
{
    FString MakeGuardrailsLocalToolSection()
    {
        return MakeGuardrailsCommandSafetySection()
            + MakeGuardrailsLocalPathSection()
            + MakeGuardrailsLocalShellSection()
            + MakeGuardrailsLocalMutationSection();
    }
}
