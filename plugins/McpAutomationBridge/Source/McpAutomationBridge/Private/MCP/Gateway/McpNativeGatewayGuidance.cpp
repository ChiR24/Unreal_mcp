// McpNativeGatewayGuidance.cpp — see header.

#include "MCP/Gateway/McpNativeGatewayGuidance.h"

namespace
{
int32 CommonPrefixLength(const FString& Left, const FString& Right)
{
	const int32 Limit = FMath::Min(Left.Len(), Right.Len());
	int32 Shared = 0;
	while (Shared < Limit && Left[Shared] == Right[Shared]) ++Shared;
	return Shared;
}
}

int32 GatewayLevenshtein(const FString& A, const FString& B)
{
	const int32 M = A.Len();
	const int32 N = B.Len();
	if (M == 0) return N;
	if (N == 0) return M;
	TArray<int32> Prev;
	Prev.SetNumUninitialized(N + 1);
	for (int32 j = 0; j <= N; ++j) Prev[j] = j;
	TArray<int32> Curr;
	Curr.SetNumUninitialized(N + 1);
	for (int32 i = 1; i <= M; ++i)
	{
		Curr[0] = i;
		for (int32 j = 1; j <= N; ++j)
		{
			const int32 Cost = (A[i - 1] == B[j - 1]) ? 0 : 1;
			Curr[j] = FMath::Min3(Prev[j] + 1, Curr[j - 1] + 1, Prev[j - 1] + Cost);
		}
		Swap(Prev, Curr);
	}
	return Prev[N];
}

TArray<FString> GatewayClosestMatches(const FString& Target, const TArray<FString>& Candidates, int32 Limit)
{
	if (Limit <= 0) return {};
	const FString T = Target.TrimStartAndEnd().ToLower();
	if (T.IsEmpty())
	{
		TArray<FString> Out;
		const int32 Count = FMath::Min(Candidates.Num(), Limit);
		for (int32 i = 0; i < Count; ++i) Out.Add(Candidates[i]);
		return Out;
	}
	struct FScored { FString Name; int32 Score; int32 Prefix; };
	TArray<FScored> Scored;
	Scored.Reserve(Candidates.Num());
	for (const FString& C : Candidates)
	{
		const FString Lower = C.ToLower();
		int32 Score = GatewayLevenshtein(Lower, T);
		if (Lower.Contains(T, ESearchCase::CaseSensitive) || T.Contains(Lower, ESearchCase::CaseSensitive)) Score -= 4;
		Scored.Add({ C, Score, CommonPrefixLength(Lower, T) });
	}
	// Edit distance alone ties candidates a caller would never confuse, and the
	// shared prefix a typo keeps is the better discriminator. The trailing
	// ordinal comparison makes the order total, so the result never depends on
	// TArray::Sort being an unstable introsort or on catalog order.
	Scored.Sort([](const FScored& L, const FScored& R)
	{
		if (L.Score != R.Score) return L.Score < R.Score;
		if (L.Prefix != R.Prefix) return L.Prefix > R.Prefix;
		return L.Name.Compare(R.Name, ESearchCase::CaseSensitive) < 0;
	});
	TArray<FString> Out;
	for (int32 i = 0; i < Scored.Num() && i < Limit; ++i) Out.Add(Scored[i].Name);
	return Out;
}

TSharedPtr<FJsonObject> GatewayBuildNextCall(const FString& Operation, const FString& Tool, const FString& Action, const FString& Param)
{
	auto Next = MakeShared<FJsonObject>();
	Next->SetStringField(TEXT("operation"), Operation);
	if (!Tool.IsEmpty()) Next->SetStringField(TEXT("tool"), Tool);
	if (!Action.IsEmpty()) Next->SetStringField(TEXT("action"), Action);
	if (!Param.IsEmpty()) Next->SetStringField(TEXT("param"), Param);
	return Next;
}
