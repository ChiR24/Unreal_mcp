#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FMcpSemanticError;

// A write's read-back, judged by the gateway (McpExecuteVerify.cpp). A record whose reply carries what it wrote as read
// back afterwards declares behavior.readBack.compare (reply field -> "$params.<input>"); after a successful call the
// receipt says verified: true when every compared field matches what was sent, or verified: false with mismatches
// naming each field, what was sent and what was read back. options.verify: false skips it.
namespace McpExecuteVerify
{
enum class EMatch : uint8
{
    Same,
    Differs,
    Unknown, // shapes that cannot be compared (a string sent, an object read back): no verdict either way
};

/** A boolean; false (OutError set, INVALID_OPTIONS) when not. */
bool Validate(const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError);

/** Numbers within a small tolerance, strings and booleans exactly, lists item by item, an object field by field over
 *  what was sent (extra fields read back do not count), and a list of numbers against an object of X/Y/Z(/W),
 *  R/G/B(/A) or Pitch/Yaw/Roll in that order. */
EMatch Compare(const TSharedPtr<FJsonValue>& Sent, const TSharedPtr<FJsonValue>& ReadBack);

/** Stamps Receipt's receipt block with verified (and mismatches) from Data and Params as Behavior's readBack names
 *  them; nothing when the record declares no read-back, options.verify is false, or no field could be compared. */
void Stamp(const TSharedPtr<FJsonObject>& Behavior, const TSharedPtr<FJsonObject>& Options,
           const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonObject>& Data, const TSharedPtr<FJsonObject>& Receipt);
} // namespace McpExecuteVerify
