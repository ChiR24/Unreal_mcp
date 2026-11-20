#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// The terms of one judge clause's expect (McpMotionJudge.h): "location.z > 120 and not velocity.z < 0". A term reads
// one value of a sample and compares it; the result is three-valued, Unknown when the sample does not hold the value or
// the comparison has no meaning for it.
namespace McpMotionJudgeTerms
{
enum class ETri : uint8 { False, True, Unknown };

struct FTerm
{
    bool bNot = false;
    FString Path;
    FString Op; // > >= < <= == != in
    FString Value;
    double Low = 0.0;
    double High = 0.0;
};

/** The terms of Expect, joined by "and"; false with Error naming the term when one does not parse. */
bool ParseTerms(const FString& Expect, TArray<FTerm>& Out, FString& Error);

/** location.x/y/z, velocity.x/y/z, rotation.pitch/yaw/roll (Rotation: the last one the samples carried), a sampled
 *  property by the name it was asked by, or a watched actor's as "<actor>.location.z" / "<actor>.<property>"; null when
 *  the sample does not hold it. */
TSharedPtr<FJsonValue> Read(const TSharedPtr<FJsonObject>& Sample, const TSharedPtr<FJsonValue>& Rotation, const FString& Path);

ETri Eval(const FTerm& Term, const TSharedPtr<FJsonValue>& Value);
} // namespace McpMotionJudgeTerms
