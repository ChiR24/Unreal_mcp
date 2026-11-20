#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// S12's first slice: sample_motion's judge. Each clause states what must hold over the run's samples:
// {expect, when: always | ever | atEnd | at, atSeconds?, after?: {expect}, withinSeconds?}. expect compares what a sample
// holds (McpMotionJudgeTerms.h): location.x/y/z, velocity.x/y/z, rotation.pitch/yaw/roll (the last one taken) or a
// sampled property by the name it was asked by. after opens the window at the first sample where its expect holds;
// withinSeconds bounds it. The verdict per clause is ok, failed (at which sample, with what it read) or unknown (it needs
// samples the run never took, or names a value no sample holds). A pure function of the samples, so it judges recorded
// runs too.
namespace McpMotionJudge
{
/** The clauses are a list of 1-16 objects as above; false (OutError naming the field by pointer) when not. */
bool Validate(const TSharedPtr<FJsonObject>& Payload, FString& OutError, FString& OutPointer);

/** Validate, then the clauses into OutClauses (left empty when the payload has no judge); OutError leads with the pointer. */
bool Take(const TSharedPtr<FJsonObject>& Payload, TArray<TSharedPtr<FJsonValue>>& OutClauses, FString& OutError);

/** One verdict object per clause: {clause, verdict, atSeconds?, read?, why?}. EndedBecause is the run's own
 *  (duration = it ran its full length). */
TArray<TSharedPtr<FJsonValue>> Judge(const TArray<TSharedPtr<FJsonValue>>& Clauses, const TArray<TSharedPtr<FJsonValue>>& Samples,
                                     const FString& EndedBecause);

/** judge (the verdicts) and judgeVerdict (ok when every clause is ok, failed when any failed, else unknown) onto Data;
 *  nothing without clauses. */
void AddTo(const TArray<TSharedPtr<FJsonValue>>& Clauses, const TArray<TSharedPtr<FJsonValue>>& Samples,
           const FString& EndedBecause, FJsonObject& Data);
} // namespace McpMotionJudge
