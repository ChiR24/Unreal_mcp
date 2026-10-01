// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace McpPlacement {
/**
 * Volume-only actors (post-process, trigger, audio, kill-Z) legitimately enclose
 * everything inside them, so reporting those as overlaps would bury the real
 * signal under noise. They are no host to mount on either.
 */
bool IsBoundsOnlyActor(const AActor *Actor);

/**
 * The actor Actor hangs on from the side, or null: a window band flush on a hall,
 * an awning on a facade. The placement check only looked for a surface UNDER an
 * actor, so everything mounted on a wall read "floating" by the height of the
 * wall it hangs on (20 window bands and 96 awnings of one level, 139 and 521
 * units up) and the real findings drowned in it.
 *
 * Mounted means the bounds touch or overlap another actor's vertical face: they
 * meet within Touch (the graze distance the overlap check already ignores), the
 * contact is across X or Y rather than a floor or a roof edge (shallower than it
 * is tall), and shallow for the actor (no deeper than Touch twice, or a quarter
 * of the actor's own size there), so a pole floating inside a big actor's box is
 * not "mounted" on it.
 */
AActor *FindMount(UWorld *World, AActor *Actor, const FBox &ActorBox, double Touch);
} // namespace McpPlacement
