#pragma once

#include "CoreMinimal.h"

class AActor;

namespace McpAnimationHandlers {
struct FActionContext;

// setup_physics_simulation on a placed prop (an actor with no skeletal mesh): its body simulates, as one undo step.
// Fills the reply and returns true; returns false, touching nothing, when the actor has no body that can simulate.
bool SimulatePlacedProp(FActionContext &Context, AActor *Actor);
} // namespace McpAnimationHandlers
