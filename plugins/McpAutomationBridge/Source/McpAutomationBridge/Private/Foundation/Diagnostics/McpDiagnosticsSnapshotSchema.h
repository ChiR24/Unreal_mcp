#pragma once

#include "CoreMinimal.h"
// JsonWriter.h pulls in PrettyJsonPrintPolicy only, so the condensed policy this
// file writes with has to be included explicitly or it parses as an undeclared
// function template.
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Todo 9 (BB-005) lane 1 - bounded, crash-tolerant diagnostics snapshot schema.
//
// This header is the redaction/bounded-field corpus for the plugin-only
// diagnostics store. Every JSON key the store may write is listed here; a key
// that is NOT listed cannot be serialized, so payloads, code, paths, capability
// tokens, scoped principals, raw idempotency keys, and raw session ids are
// structurally excluded from the on-disk record - not by convention but by the
// absence of a writer for them. Values are clamped through allowlists
// (canonical actions, origins, terminal classes, disconnect reasons) or
// bounded lengths; unresolved input becomes a fixed sentinel, never the input.
//
// Durability contract: process-death recovery only, NOT power-loss durability.
// No fsync is ever issued on the write path (the editor game thread must not
// block on a disk flush).

namespace McpDiagnosticsSchema
{
	inline constexpr int32 MaxSnapshotBytes = 64 * 1024;
	inline constexpr int32 SchemaVersionValue = 1;
	inline constexpr int32 MaxIdLength = 64;
	inline constexpr int32 MaxActionLength = 128;

	inline const TCHAR* NonCanonicalActionValue() { return TEXT("non_canonical"); }
	inline const TCHAR* UnknownValue() { return TEXT("unknown"); }

	// Canonical parent-tool allowlist (mirrors the 23 TS canonical parents).
	inline const TArray<FString>& CanonicalActionValues()
	{
		static const TArray<FString> Values = {
			TEXT("animation_physics"),
			TEXT("build_environment"),
			TEXT("control_actor"),
			TEXT("control_editor"),
			TEXT("inspect"),
			TEXT("manage_ai"),
			TEXT("manage_asset"),
			TEXT("manage_audio"),
			TEXT("manage_blueprint"),
			TEXT("manage_character"),
			TEXT("manage_combat"),
			TEXT("manage_effect"),
			TEXT("manage_gas"),
			TEXT("manage_interaction"),
			TEXT("manage_inventory"),
			TEXT("manage_level"),
			TEXT("manage_level_structure"),
			TEXT("manage_geometry"),
			TEXT("manage_networking"),
			TEXT("manage_pcg"),
			TEXT("manage_sequence"),
			TEXT("manage_tools"),
			TEXT("system_control"),
		};
		return Values;
	}

	inline const TArray<FString>& OriginValues()
	{
		static const TArray<FString> Values = {
			TEXT("WebSocket"),
			TEXT("NativeHTTP"),
		};
		return Values;
	}

	// Terminal class: success/failure/unknown plus the shared refusal codes.
	inline const TArray<FString>& TerminalClassValues()
	{
		static const TArray<FString> Values = {
			TEXT("success"),
			TEXT("failure"),
			TEXT("unknown"),
			TEXT("SCOPE_NOT_GRANTED"),
			TEXT("CONSENT_REQUIRED"),
			TEXT("PATH_NOT_PERMITTED"),
			TEXT("PROJECT_NOT_PERMITTED"),
			TEXT("QUOTA_EXCEEDED"),
			TEXT("COMMAND_BLOCKED"),
		};
		return Values;
	}

	inline const TArray<FString>& DisconnectReasonValues()
	{
		static const TArray<FString> Values = {
			TEXT("closed"),
			TEXT("error"),
		};
		return Values;
	}

	inline FString CoerceCanonicalAction(const FString& Candidate)
	{
		const FString Trimmed = Candidate.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.Len() > MaxActionLength)
		{
			return NonCanonicalActionValue();
		}
		const int32 Dot = Trimmed.Find(TEXT("."));
		const FString Parent = (Dot == INDEX_NONE) ? Trimmed : Trimmed.Left(Dot);
		return CanonicalActionValues().Contains(Parent)
			? Trimmed
			: FString(NonCanonicalActionValue());
	}

	inline FString CoerceOrigin(const FString& Candidate)
	{
		const FString Trimmed = Candidate.TrimStartAndEnd();
		return OriginValues().Contains(Trimmed) ? Trimmed : FString(UnknownValue());
	}

	inline FString CoerceTerminalClass(const FString& Candidate)
	{
		const FString Trimmed = Candidate.TrimStartAndEnd();
		return TerminalClassValues().Contains(Trimmed) ? Trimmed : FString(UnknownValue());
	}

	inline FString CoerceDisconnectReason(const FString& Candidate)
	{
		const FString Trimmed = Candidate.TrimStartAndEnd();
		return DisconnectReasonValues().Contains(Trimmed) ? Trimmed : FString(UnknownValue());
	}

	/**
	 * Bounded, redacted in-memory record. Every field is either a number, a
	 * clamped allowlist string, or a length-capped identifier. There is no
	 * field for payload, code, path, token, principal, idempotency key, or raw
	 * session id - the record cannot represent them.
	 */
	struct FMcpDiagnosticsSnapshotState
	{
		FString InstanceId;
		int32 Pid = 0;
		FString StartTimeUtc;

		int32 Requests = 0;
		int32 Failures = 0;
		int32 Refusals = 0;
		int32 QueueWaitMs = 0;

		bool bHasRequest = false;
		FString RequestId;
		FString CorrelationId;
		FString CanonicalAction;
		FString Origin;
		int32 QueueDepth = 0;
		double EnqueueAt = 0.0;
		double DispatchAt = 0.0;
		double TerminalAt = 0.0;
		FString TerminalClass;

		bool bHasHandshake = false;
		bool HandshakeOk = false;
		double HandshakeAt = 0.0;

		bool bHasDisconnect = false;
		FString DisconnectReason;
		double DisconnectAt = 0.0;

		bool bHasSession = false;
		int32 SessionsCreated = 0;
		int32 SessionsClosed = 0;
		int32 SessionsActive = 0;
		FString LastIdentitySha256;
		double SessionAt = 0.0;
	};

	inline FString IsoFromSeconds(double Seconds)
	{
		if (Seconds <= 0.0)
		{
			return FString();
		}
		return FDateTime::FromUnixTimestamp(Seconds).ToIso8601();
	}

	inline void SetTimeOrNull(TSharedRef<FJsonObject> Obj, const TCHAR* Key, double Seconds)
	{
		Obj->SetField(Key, Seconds > 0.0
			? TSharedPtr<FJsonValue>(MakeShared<FJsonValueString>(IsoFromSeconds(Seconds)))
			: TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
	}

	inline void SetStringOrNull(TSharedRef<FJsonObject> Obj, const TCHAR* Key, const FString& Value)
	{
		Obj->SetField(Key, Value.IsEmpty()
			? TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>())
			: TSharedPtr<FJsonValue>(MakeShared<FJsonValueString>(Value)));
	}

	/**
	 * Builds the bounded on-disk JSON. bMinimal drops every optional section
	 * (handshake/disconnect/session) so an over-cap record can be reserialized
	 * rather than sliced; JSON is never sliced.
	 */
	inline TSharedRef<FJsonObject> BuildSnapshotJson(const FMcpDiagnosticsSnapshotState& State, bool bMinimal = false)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schemaVersion"), SchemaVersionValue);

		TSharedRef<FJsonObject> Instance = MakeShared<FJsonObject>();
		Instance->SetStringField(TEXT("instanceId"), State.InstanceId.Left(MaxIdLength));
		Instance->SetNumberField(TEXT("pid"), State.Pid);
		Instance->SetStringField(TEXT("startTimeUtc"), State.StartTimeUtc);
		Root->SetObjectField(TEXT("instance"), Instance);

		TSharedRef<FJsonObject> Counters = MakeShared<FJsonObject>();
		Counters->SetNumberField(TEXT("requests"), State.Requests);
		Counters->SetNumberField(TEXT("failures"), State.Failures);
		Counters->SetNumberField(TEXT("refusals"), State.Refusals);
		Counters->SetNumberField(TEXT("queueWaitMs"), State.QueueWaitMs);
		Root->SetObjectField(TEXT("counters"), Counters);

		TSharedRef<FJsonObject> LastRequest = MakeShared<FJsonObject>();
		SetStringOrNull(LastRequest, TEXT("requestId"), State.RequestId.Left(MaxIdLength));
		SetStringOrNull(LastRequest, TEXT("correlationId"), State.CorrelationId.Left(MaxIdLength));
		SetStringOrNull(LastRequest, TEXT("canonicalAction"), State.CanonicalAction);
		SetStringOrNull(LastRequest, TEXT("origin"), State.Origin);
		LastRequest->SetNumberField(TEXT("queueDepth"), State.QueueDepth);
		SetTimeOrNull(LastRequest, TEXT("enqueueAt"), State.EnqueueAt);
		SetTimeOrNull(LastRequest, TEXT("dispatchAt"), State.DispatchAt);
		SetTimeOrNull(LastRequest, TEXT("terminalAt"), State.TerminalAt);
		SetStringOrNull(LastRequest, TEXT("terminalClass"), State.TerminalClass);
		Root->SetObjectField(TEXT("lastRequest"), LastRequest);

		if (bMinimal)
		{
			return Root;
		}

		if (State.bHasHandshake)
		{
			TSharedRef<FJsonObject> Handshake = MakeShared<FJsonObject>();
			SetTimeOrNull(Handshake, TEXT("at"), State.HandshakeAt);
			Handshake->SetBoolField(TEXT("ok"), State.HandshakeOk);
			Root->SetObjectField(TEXT("lastHandshake"), Handshake);
		}
		else
		{
			Root->SetField(TEXT("lastHandshake"), MakeShared<FJsonValueNull>());
		}

		if (State.bHasDisconnect)
		{
			TSharedRef<FJsonObject> Disconnect = MakeShared<FJsonObject>();
			SetTimeOrNull(Disconnect, TEXT("at"), State.DisconnectAt);
			SetStringOrNull(Disconnect, TEXT("reason"), State.DisconnectReason);
			Root->SetObjectField(TEXT("lastDisconnect"), Disconnect);
		}
		else
		{
			Root->SetField(TEXT("lastDisconnect"), MakeShared<FJsonValueNull>());
		}

		if (State.bHasSession)
		{
			TSharedRef<FJsonObject> Session = MakeShared<FJsonObject>();
			Session->SetNumberField(TEXT("created"), State.SessionsCreated);
			Session->SetNumberField(TEXT("closed"), State.SessionsClosed);
			Session->SetNumberField(TEXT("active"), State.SessionsActive);
			SetStringOrNull(Session, TEXT("lastIdentitySha256"), State.LastIdentitySha256);
			SetTimeOrNull(Session, TEXT("at"), State.SessionAt);
			Root->SetObjectField(TEXT("session"), Session);
		}
		else
		{
			Root->SetField(TEXT("session"), MakeShared<FJsonValueNull>());
		}

		return Root;
	}

	// Condensed, not the TJsonWriterFactory<> default. That default is
	// TPrettyJsonPrintPolicy, and indentation is the wrong trade for this file
	// twice over: the snapshot is a machine-read crash artifact nobody opens by
	// hand, and it is size-capped, so the newlines and tabs spent on layout are
	// bytes taken from the record itself and push MaxSnapshotBytes into dropping
	// real sections. It also put a space after every colon, which is why the
	// suite's `"requestId":"..."` checks could never match what was on disk.
	using FCondensedWriter = TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;
	using FCondensedWriterFactory = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

	// Serializes to a bounded JSON string; drops optional sections if over cap.
	inline FString SerializeState(const FMcpDiagnosticsSnapshotState& State, bool bMinimal)
	{
		FString Content;
		{
			const TSharedRef<FCondensedWriter> Writer = FCondensedWriterFactory::Create(&Content);
			FJsonSerializer::Serialize(BuildSnapshotJson(State, bMinimal), Writer);
			Writer->Close();
		}
		if (FTCHARToUTF8(Content).Length() <= MaxSnapshotBytes)
		{
			return Content;
		}
		FString Reduced;
		{
			const TSharedRef<FCondensedWriter> Writer = FCondensedWriterFactory::Create(&Reduced);
			FJsonSerializer::Serialize(BuildSnapshotJson(State, true), Writer);
			Writer->Close();
		}
		return Reduced;
	}
} // namespace McpDiagnosticsSchema
