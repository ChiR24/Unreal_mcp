#include "MCP/Gateway/McpNativeGatewayFolding.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
// McpNativeGatewayFolding.cpp — see header for the contract.
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "Dom/JsonValue.h"

namespace
{
FString FoldingReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(Field, Value);
	}
	return Value;
}

FString FoldingActionSegment(const FString& CapabilityId)
{
	int32 LastDot = INDEX_NONE;
	if (CapabilityId.FindLastChar(TEXT('.'), LastDot) && LastDot != INDEX_NONE)
	{
		return CapabilityId.RightChop(LastDot + 1);
	}
	return CapabilityId;
}

bool ParseLegacyPairs(const TSharedPtr<FJsonObject>& Record, FMcpCapabilityRecord& Out, FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* LegacyIds = nullptr;
	if (!Record->TryGetArrayField(TEXT("legacyIds"), LegacyIds) || !LegacyIds)
	{
		return true;
	}
	for (const TSharedPtr<FJsonValue>& Value : *LegacyIds)
	{
		const TSharedPtr<FJsonObject>* Pair = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Pair) || !Pair)
		{
			continue;
		}
		FMcpLegacyPair Parsed;
		Parsed.Tool = FoldingReadString(*Pair, TEXT("tool"));
		Parsed.Action = FoldingReadString(*Pair, TEXT("action"));
		if (Parsed.Tool.IsEmpty() || Parsed.Action.IsEmpty())
		{
			OutError = FString::Printf(TEXT("record '%s' has a legacy pair without tool/action"), *Out.Id);
			return false;
		}
		const TSharedPtr<FJsonObject>* Folded = nullptr;
		if ((*Pair)->TryGetObjectField(TEXT("folded"), Folded) && Folded)
		{
			Parsed.FoldedPins = *Folded;
		}
		Out.LegacyPairs.Add(MoveTemp(Parsed));
	}
	// The first pair is the advertised primary; folding it would leave the
	// record with no name of its own.
	if (Out.LegacyPairs.Num() > 0 && Out.LegacyPairs[0].IsFolded())
	{
		OutError = FString::Printf(TEXT("record '%s' folds its advertised primary pair"), *Out.Id);
		return false;
	}
	return true;
}

bool ParseDispatchBy(const TSharedPtr<FJsonObject>& Record, FMcpCapabilityRecord& Out, FString& OutError)
{
	const TSharedPtr<FJsonObject>* Routing = nullptr;
	const TSharedPtr<FJsonObject>* DispatchBy = nullptr;
	if (!Record->TryGetObjectField(TEXT("routing"), Routing) || !Routing
		|| !(*Routing)->TryGetObjectField(TEXT("dispatchBy"), DispatchBy) || !DispatchBy)
	{
		return true;
	}
	Out.DispatchBySelector = FoldingReadString(*DispatchBy, TEXT("param"));
	const TSharedPtr<FJsonObject>* Actions = nullptr;
	if (Out.DispatchBySelector.IsEmpty()
		|| !(*DispatchBy)->TryGetObjectField(TEXT("actions"), Actions) || !Actions)
	{
		OutError = FString::Printf(TEXT("record '%s' has an incomplete routing.dispatchBy"), *Out.Id);
		return false;
	}
	for (const auto& Entry : (*Actions)->Values)
	{
		FString Action;
		// A mapped action must be one the record itself declares, so a fold can
		// never dispatch something the handlers do not implement.
		if (!Entry.Value.IsValid() || !McpHandlerUtils::TryGetJsonValueString(Entry.Value, Action) || Action.IsEmpty()
			|| McpFindLegacyPair(Out, Action) == nullptr)
		{
			OutError = FString::Printf(
				TEXT("record '%s' maps '%s' to an action it does not declare"), *Out.Id, *Entry.Key);
			return false;
		}
		// `Values` keys are FString before 5.8 and UE::FSharedString from 5.8, so
		// dereference (both expose operator* -> const TCHAR*) instead of relying
		// on a conversion that no longer exists.
		Out.DispatchByActions.Add(FString(*Entry.Key), Action);
	}
	return true;
}
}

bool McpParseRecordFolding(
	const TSharedPtr<FJsonObject>& Record, FMcpCapabilityRecord& Out, FString& OutError)
{
	Out.LegacyPairs.Reset();
	Out.DispatchBySelector.Reset();
	Out.DispatchByActions.Reset();
	return ParseLegacyPairs(Record, Out, OutError) && ParseDispatchBy(Record, Out, OutError);
}

const FMcpLegacyPair* McpFindLegacyPair(const FMcpCapabilityRecord& Record, const FString& Action)
{
	if (Action.IsEmpty())
	{
		return nullptr;
	}
	for (const FMcpLegacyPair& Pair : Record.LegacyPairs)
	{
		if (Pair.Action.Equals(Action, ESearchCase::CaseSensitive))
		{
			return &Pair;
		}
	}
	return nullptr;
}

FString McpRequestedLegacyAction(const TSharedPtr<FJsonObject>& GatewayParams, const FString& RecordId)
{
	const FString Action = FoldingReadString(GatewayParams, TEXT("action"));
	if (!Action.IsEmpty())
	{
		return Action;
	}
	const FString Capability = FoldingReadString(GatewayParams, TEXT("capability"));
	if (Capability.IsEmpty() || Capability.Equals(RecordId, ESearchCase::CaseSensitive))
	{
		return FString();
	}
	return FoldingActionSegment(Capability);
}

/**
 * Inject the pins a folded old name implies, never overriding what the caller
 * sent. Returns false when the caller supplied the pinned selector with a
 * value that DISAGREES with the pin: the action they named wins dispatch, so
 * a conflict is a contradictory request (a legacy caller never sent the
 * selector pre-fold) and must be refused rather than dispatched.
 */
bool McpApplyFoldedPins(
	const FMcpCapabilityRecord& Record, const FString& RequestedAction,
	const TSharedPtr<FJsonObject>& Params)
{
	const FMcpLegacyPair* Pair = McpFindLegacyPair(Record, RequestedAction);
	if (!Pair || !Pair->IsFolded() || !Params.IsValid())
	{
		return true;
	}
	for (const auto& Pin : Pair->FoldedPins->Values)
	{
		if (Params->HasField(Pin.Key))
		{
			FString ExistingText;
			FString PinText;
			// FJsonValue exposes no TryGetString: compare the string forms directly.
			const TSharedPtr<FJsonValue>* Existing = Params->Values.Find(Pin.Key);
			if (Existing && Existing->IsValid() && (*Existing)->Type == EJson::String
				&& Pin.Value.IsValid() && Pin.Value->Type == EJson::String
				&& (*Existing)->AsString().Equals(Pin.Value->AsString(), ESearchCase::CaseSensitive))
			{
				continue;
			}
			return false;
		}
		Params->SetField(Pin.Key, Pin.Value);
	}
	return true;
}

/**
 * The bridge action to dispatch once params are validated: an old name
 * dispatches itself, the primary maps its selector through dispatchBy. An
 * unmapped selector value FAILS CLOSED (empty return): validation enforces
 * the selector's enum, which the map keys equal, so this only guards a
 * malformed record from silently dispatching the primary.
 */
FString McpResolveDispatchAction(
	const FMcpCapabilityRecord& Record, const FString& RequestedAction,
	const TSharedPtr<FJsonObject>& Params, const FString& PrimaryAction)
{
	const FMcpLegacyPair* Pair = McpFindLegacyPair(Record, RequestedAction);
	if (Pair && Pair->IsFolded())
	{
		return Pair->Action;
	}
	if (Record.DispatchBySelector.IsEmpty() || !Params.IsValid())
	{
		return PrimaryAction;
	}
	FString Value;
	if (!Params->TryGetStringField(Record.DispatchBySelector, Value))
	{
		return PrimaryAction;
	}
	const FString* Mapped = Record.DispatchByActions.Find(Value);
	return Mapped ? *Mapped : FString();
}

/**
 * A consent grant naming a FOLDED {tool}.{action} pair authorizes that pair's
 * operation specifically — the human acknowledged the action the name
 * describes. Returns false (and reports the granted pair name) when such a
 * grant is used to dispatch a DIFFERENT action of the same family; a grant
 * naming the canonical id, an alias, or a non-folded name authorizes the
 * whole family and always passes. Mirror of matchedFoldedGrant in
 * gateway-execute.ts.
 */
bool McpFoldedGrantMatchesDispatch(
	const FMcpCapabilityRecord& Record, const FString& GrantedCapability,
	const FString& DispatchTarget, FString& OutGrantedPairName)
{
	OutGrantedPairName.Reset();
	if (GrantedCapability.IsEmpty())
	{
		return true;
	}
	for (const FMcpLegacyPair& Pair : Record.LegacyPairs)
	{
		const FString PairName = Pair.Tool + TEXT(".") + Pair.Action;
		if (!Pair.IsFolded() || !PairName.Equals(GrantedCapability, ESearchCase::CaseSensitive))
		{
			continue;
		}
		OutGrantedPairName = PairName;
		return Pair.Action.Equals(DispatchTarget, ESearchCase::CaseSensitive);
	}
	return true;
}
