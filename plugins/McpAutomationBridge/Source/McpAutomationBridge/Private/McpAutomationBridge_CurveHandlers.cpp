// =============================================================================
// McpAutomationBridge_CurveHandlers.cpp
// =============================================================================
// manage_curve tool handlers (Ch5 UCurveFloat authoring).
//
// Dispatcher: HandleManageCurveAction reads payload `subAction` and routes to
// per-action helpers declared as static functions below. This mirrors the
// HandleManageDataAction pattern used in the Ch2/Ch3 DataTable/DataAsset bridge.
//
// Per-action helpers added one-per-commit (Ch5 Tasks 2-5).
// Unknown sub-actions return NOT_IMPLEMENTED so callers get a stable error
// shape even before every action is wired up.
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "MCP/Helpers/McpGenericAssetFactory.h"

#if WITH_EDITOR
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"
#include "EditorAssetLibrary.h"
#include "UObject/Package.h"
#endif

// ---------------------------------------------------------------------------
// Per-action handler helpers (definitions added in follow-on tasks)
// ---------------------------------------------------------------------------
namespace McpCurveHandlers
{

#if WITH_EDITOR

	static void SendSuccess(UMcpAutomationBridgeSubsystem* Self,
		TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId,
		const FString& Message, const TSharedPtr<FJsonObject>& Data)
	{
		Self->SendAutomationResponse(Socket, RequestId, true, Message, Data);
	}

	static void SendError(UMcpAutomationBridgeSubsystem* Self,
		TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId,
		const FString& Category, const FString& Message)
	{
		Self->SendAutomationError(Socket, RequestId, Message, Category);
	}

	// -----------------------------------------------------------------------
	// create_curve_float — instantiate a UCurveFloat at path/name.
	// -----------------------------------------------------------------------
	static bool HandleCreateCurveFloat(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr, NameStr;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr) ||
			!Payload->TryGetStringField(TEXT("name"), NameStr))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field(s): path, name"));
			return true;
		}

		const FString FullPath = PathStr / NameStr;
		if (UEditorAssetLibrary::DoesAssetExist(FullPath))
		{
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("assetPath"), FullPath);
			Data->SetBoolField(TEXT("alreadyExists"), true);
			SendSuccess(Self, Socket, RequestId,
				FString::Printf(TEXT("Curve already exists at '%s'"), *FullPath), Data);
			return true;
		}

		FString OutError;
		bool bSaved = false;
		UObject* NewCurve = McpGenericAssetFactory::CreateAssetOfClass(
			UCurveFloat::StaticClass(), PathStr, NameStr, nullptr, OutError, bSaved);
		if (!NewCurve)
		{
			SendError(Self, Socket, RequestId, TEXT("ENGINE_API_ERROR"),
				OutError.IsEmpty() ? TEXT("CreateAsset returned nullptr") : OutError);
			return true;
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), NewCurve->GetPathName());
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved && !OutError.IsEmpty())
		{
			Data->SetStringField(TEXT("saveWarning"), OutError);
		}
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Created UCurveFloat at '%s'"), *NewCurve->GetPathName()),
			Data);
		return true;
	}

	// Parse the caller-facing interpMode string into the engine's separate
	// interp + tangent-mode enums. Default (unknown/unset) is (Cubic, Auto).
	static ERichCurveInterpMode ParseInterpMode(const FString& S, ERichCurveTangentMode& OutTangent)
	{
		OutTangent = RCTM_Auto;
		if (S.Equals(TEXT("Linear"), ESearchCase::IgnoreCase)) { return RCIM_Linear; }
		if (S.Equals(TEXT("Constant"), ESearchCase::IgnoreCase)) { return RCIM_Constant; }
		if (S.Equals(TEXT("CubicBreak"), ESearchCase::IgnoreCase))
		{
			OutTangent = RCTM_Break;
			return RCIM_Cubic;
		}
		// Default "Auto" (or empty): Cubic + Auto tangents.
		OutTangent = RCTM_Auto;
		return RCIM_Cubic;
	}

	// -----------------------------------------------------------------------
	// set_curve_keys — replace all keys on the target UCurveFloat.
	// -----------------------------------------------------------------------
	static bool HandleSetCurveKeys(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr;
		const TArray<TSharedPtr<FJsonValue>>* KeysArr = nullptr;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr) ||
			!Payload->TryGetArrayField(TEXT("keys"), KeysArr))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field(s): path, keys"));
			return true;
		}

		UCurveFloat* Curve = LoadObject<UCurveFloat>(nullptr, *PathStr);
		if (!Curve)
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("Curve not found: %s"), *PathStr));
			return true;
		}

		Curve->FloatCurve.Reset();
		for (const TSharedPtr<FJsonValue>& V : *KeysArr)
		{
			if (!V.IsValid() || V->Type != EJson::Object) { continue; }
			const TSharedPtr<FJsonObject>& Obj = V->AsObject();
			if (!Obj.IsValid()) { continue; }

			double TimeVal = 0.0;
			double ValueVal = 0.0;
			if (!Obj->TryGetNumberField(TEXT("time"), TimeVal) ||
				!Obj->TryGetNumberField(TEXT("value"), ValueVal))
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
					TEXT("Each key must have numeric 'time' and 'value'"));
				return true;
			}

			FString InterpStr;
			Obj->TryGetStringField(TEXT("interpMode"), InterpStr);
			ERichCurveTangentMode TangentMode;
			const ERichCurveInterpMode InterpMode = ParseInterpMode(InterpStr, TangentMode);

			// FRichCurve::AddKey signature in UE 5.7:
			//   virtual FKeyHandle AddKey(float InTime, float InValue,
			//       const bool bUnwindRotation = false,
			//       FKeyHandle KeyHandle = FKeyHandle()) final override;
			// Third param defaults to false which is what we want.
			const FKeyHandle Handle = Curve->FloatCurve.AddKey(
				static_cast<float>(TimeVal), static_cast<float>(ValueVal));
			Curve->FloatCurve.SetKeyInterpMode(Handle, InterpMode);
			Curve->FloatCurve.SetKeyTangentMode(Handle, TangentMode);
		}

		Curve->MarkPackageDirty();
		const bool bSaved = McpSafeAssetSave(Curve);

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), Curve->GetPathName());
		Data->SetNumberField(TEXT("keyCount"), Curve->FloatCurve.GetNumKeys());
		Data->SetBoolField(TEXT("saved"), bSaved);
		if (!bSaved)
		{
			Data->SetStringField(TEXT("saveWarning"),
				TEXT("Asset changes in memory but save failed"));
		}
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Set %d key(s) on curve '%s'"),
				Curve->FloatCurve.GetNumKeys(), *Curve->GetName()),
			Data);
		return true;
	}

	// Inverse mapping of ParseInterpMode: (interp, tangent) -> caller-facing string.
	static FString InterpModeToString(ERichCurveInterpMode M, ERichCurveTangentMode T)
	{
		if (M == RCIM_Linear) { return TEXT("Linear"); }
		if (M == RCIM_Constant) { return TEXT("Constant"); }
		if (M == RCIM_Cubic && T == RCTM_Break) { return TEXT("CubicBreak"); }
		// RCIM_Cubic + (Auto/User/SmartAuto/None) collapses to "Auto" in the
		// caller-facing enum — finer tangent control is a follow-up.
		return TEXT("Auto");
	}

	// -----------------------------------------------------------------------
	// get_curve_keys — emit every key as {time, value, interpMode}.
	// -----------------------------------------------------------------------
	static bool HandleGetCurveKeys(UMcpAutomationBridgeSubsystem* Self,
		const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
		TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString PathStr;
		if (!Payload->TryGetStringField(TEXT("path"), PathStr))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing required field: path"));
			return true;
		}

		UCurveFloat* Curve = LoadObject<UCurveFloat>(nullptr, *PathStr);
		if (!Curve)
		{
			SendError(Self, Socket, RequestId, TEXT("NOT_FOUND"),
				FString::Printf(TEXT("Curve not found: %s"), *PathStr));
			return true;
		}

		TArray<TSharedPtr<FJsonValue>> Keys;
		// GetKeyHandleIterator returns TArray<FKeyHandle>::TConstIterator in UE 5.7.
		for (auto It = Curve->FloatCurve.GetKeyHandleIterator(); It; ++It)
		{
			const FKeyHandle Handle = *It;
			// GetKey(FKeyHandle) has mutable + const overloads in UE 5.7;
			// the mutable overload returns FRichCurveKey&.
			const FRichCurveKey& K = Curve->FloatCurve.GetKey(Handle);

			TSharedPtr<FJsonObject> KObj = MakeShared<FJsonObject>();
			KObj->SetNumberField(TEXT("time"), K.Time);
			KObj->SetNumberField(TEXT("value"), K.Value);
			KObj->SetStringField(TEXT("interpMode"),
				InterpModeToString(K.InterpMode.GetValue(), K.TangentMode.GetValue()));
			Keys.Add(MakeShared<FJsonValueObject>(KObj));
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), Curve->GetPathName());
		Data->SetArrayField(TEXT("keys"), Keys);
		Data->SetNumberField(TEXT("keyCount"), Keys.Num());
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Fetched %d key(s) from curve '%s'"),
				Keys.Num(), *Curve->GetName()),
			Data);
		return true;
	}

#endif // WITH_EDITOR

} // namespace McpCurveHandlers

// ---------------------------------------------------------------------------
// Dispatcher
// ---------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleManageCurveAction(
	const FString& RequestId, const FString& Action,
	const TSharedPtr<FJsonObject>& Payload,
	TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
	if (Action != TEXT("manage_curve"))
	{
		return false;
	}

	FString SubAction;
	if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("subAction"), SubAction))
	{
		SendAutomationError(RequestingSocket, RequestId,
			TEXT("Missing required parameter: subAction"),
			TEXT("INVALID_PARAMS"));
		return true;
	}

#if !WITH_EDITOR
	SendAutomationError(RequestingSocket, RequestId,
		TEXT("manage_curve requires an editor build"),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#else
	if (SubAction == TEXT("create_curve_float"))
	{
		return McpCurveHandlers::HandleCreateCurveFloat(this, RequestId, Payload, RequestingSocket);
	}
	if (SubAction == TEXT("set_curve_keys"))
	{
		return McpCurveHandlers::HandleSetCurveKeys(this, RequestId, Payload, RequestingSocket);
	}
	if (SubAction == TEXT("get_curve_keys"))
	{
		return McpCurveHandlers::HandleGetCurveKeys(this, RequestId, Payload, RequestingSocket);
	}

	SendAutomationError(RequestingSocket, RequestId,
		FString::Printf(TEXT("manage_curve sub-action not yet implemented: %s"), *SubAction),
		TEXT("NOT_IMPLEMENTED"));
	return true;
#endif
}
