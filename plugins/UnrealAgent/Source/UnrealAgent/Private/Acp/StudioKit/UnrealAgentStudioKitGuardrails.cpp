#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

namespace UnrealAgentStudioKit
{
FString MakeGuardrailsCoreSection()
{
    return MakeGuardrailsPreflightStateSection()
        + MakeGuardrailsMutationAdmissionSection();
}
}
