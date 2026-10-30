#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;

namespace McpAnimationHandlers {
struct FActionContext;

// setup_physics_simulation on a placed prop (an actor with no skeletal mesh): its body simulates, as one undo step,
// weighing the payload's massKg when given (else what its size and material give it). Fills the reply, massKg
// included, and returns true; returns false, touching nothing, when the actor has no body that can simulate.
bool SimulatePlacedProp(FActionContext &Context, AActor *Actor, const TSharedPtr<FJsonObject> &Payload);

// setup_physics_simulation: a massKg out of range, or aimed at a skeletal mesh (whose bodies are weighed in its
// Physics Asset), is refused before anything runs; everything else goes to HandleAnimationSetupPhysicsSimulationAction.
bool HandleSetupPhysicsSimulation(FActionContext &Context, const TSharedPtr<FJsonObject> &Payload);
} // namespace McpAnimationHandlers
