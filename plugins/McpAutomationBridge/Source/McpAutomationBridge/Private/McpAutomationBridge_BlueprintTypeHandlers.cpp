// McpAutomationBridge_BlueprintTypeHandlers.cpp

#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BlueprintTypeHandlers.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "MCP/McpPinTypeParser.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Factories/EnumFactory.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UObject/Package.h"
#endif

namespace McpBlueprintTypeHandlers
{
namespace
{
	void SendError(UMcpAutomationBridgeSubsystem* Self,
	               TSharedPtr<FMcpBridgeWebSocket> Socket,
	               const FString& RequestId,
	               const FString& Code,
	               const FString& Message)
	{
		Self->SendAutomationError(Socket, RequestId, Message, Code);
	}

	void SendSuccess(UMcpAutomationBridgeSubsystem* Self,
	                 TSharedPtr<FMcpBridgeWebSocket> Socket,
	                 const FString& RequestId,
	                 const FString& Message,
	                 const TSharedPtr<FJsonObject>& Data)
	{
		Self->SendAutomationResponse(Socket, RequestId, true, Message, Data);
	}

#if WITH_EDITOR
	// Convert /Game/X/Y to (PackagePath="/Game/X", AssetName="Y")
	bool SplitAssetPath(const FString& AssetPath, FString& OutPackagePath, FString& OutAssetName, FString& OutError)
	{
		if (!AssetPath.StartsWith(TEXT("/")))
		{
			OutError = FString::Printf(TEXT("Asset path must start with /Game/ or /: '%s'"), *AssetPath);
			return false;
		}
		int32 LastSlash = INDEX_NONE;
		if (!AssetPath.FindLastChar('/', LastSlash) || LastSlash <= 0)
		{
			OutError = FString::Printf(TEXT("Cannot extract asset name from path: '%s'"), *AssetPath);
			return false;
		}
		OutPackagePath = AssetPath.Left(LastSlash);
		OutAssetName = AssetPath.Mid(LastSlash + 1);
		if (OutAssetName.IsEmpty())
		{
			OutError = TEXT("Asset name is empty after splitting path");
			return false;
		}
		return true;
	}

	bool IsValidIdentifier(const FString& S)
	{
		if (S.IsEmpty()) return false;
		const TCHAR First = S[0];
		if (!(FChar::IsAlpha(First) || First == '_')) return false;
		for (int32 i = 1; i < S.Len(); ++i)
		{
			const TCHAR C = S[i];
			if (!(FChar::IsAlnum(C) || C == '_')) return false;
		}
		return true;
	}

	int32 CountReferencers(const FString& AssetPath)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
		TArray<FName> Refs;
		AR.GetReferencers(FName(*AssetPath), Refs);
		return Refs.Num();
	}

	bool HandleCreateEnum(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString AssetPath, Err;
		if (!McpHandlerUtils::TryGetRequiredString(Payload, TEXT("assetPath"), AssetPath, Err))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"), Err);
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* Enumerators = nullptr;
		if (!Payload->TryGetArrayField(TEXT("enumerators"), Enumerators) || Enumerators->Num() == 0)
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing or empty 'enumerators' array"));
			return true;
		}

		const bool bOverwrite = Payload->HasTypedField<EJson::Boolean>(TEXT("overwrite")) &&
		                        Payload->GetBoolField(TEXT("overwrite"));

		FString PackagePath, AssetName;
		if (!SplitAssetPath(AssetPath, PackagePath, AssetName, Err))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"), Err);
			return true;
		}

		TArray<FString> Warnings;
		if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			if (!bOverwrite)
			{
				SendError(Self, Socket, RequestId, TEXT("ASSET_ALREADY_EXISTS"),
					FString::Printf(TEXT("Asset already exists at '%s'; pass overwrite=true to replace."), *AssetPath));
				return true;
			}
			const int32 Refs = CountReferencers(AssetPath);
			if (Refs > 0)
			{
				Warnings.Add(FString::Printf(
					TEXT("Replaced asset had %d referencer(s); pin types in dependent assets may now be unresolved."),
					Refs));
			}
			UObject* Existing = UEditorAssetLibrary::LoadAsset(AssetPath);
			if (!Existing || !UEditorAssetLibrary::DeleteLoadedAsset(Existing))
			{
				SendError(Self, Socket, RequestId, TEXT("OPERATION_FAILED"),
					FString::Printf(TEXT("Failed to delete existing asset at '%s'"), *AssetPath));
				return true;
			}
		}

		// Validate enumerator names + check duplicates
		TSet<FString> SeenNames;
		for (const TSharedPtr<FJsonValue>& V : *Enumerators)
		{
			TSharedPtr<FJsonObject> Obj = V->AsObject();
			if (!Obj.IsValid())
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
					TEXT("Each enumerator entry must be an object"));
				return true;
			}
			FString Name;
			if (!Obj->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
					TEXT("Enumerator missing required 'name'"));
				return true;
			}
			if (!IsValidIdentifier(Name))
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_NAME"),
					FString::Printf(TEXT("Invalid enumerator name: '%s'"), *Name));
				return true;
			}
			if (SeenNames.Contains(Name))
			{
				SendError(Self, Socket, RequestId, TEXT("DUPLICATE_NAME"),
					FString::Printf(TEXT("Duplicate enumerator name: '%s'"), *Name));
				return true;
			}
			SeenNames.Add(Name);
		}

		// Create empty UUserDefinedEnum via AssetTools
		FAssetToolsModule& AT = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UEnumFactory* Factory = NewObject<UEnumFactory>();
		UObject* NewObj = AT.Get().CreateAsset(AssetName, PackagePath,
			UUserDefinedEnum::StaticClass(), Factory);
		UUserDefinedEnum* NewEnum = Cast<UUserDefinedEnum>(NewObj);
		if (!NewEnum)
		{
			SendError(Self, Socket, RequestId, TEXT("OPERATION_FAILED"),
				FString::Printf(TEXT("AssetTools failed to create UUserDefinedEnum at '%s'"), *AssetPath));
			return true;
		}

		// Add enumerators
		int32 Count = 0;
		for (const TSharedPtr<FJsonValue>& V : *Enumerators)
		{
			TSharedPtr<FJsonObject> Obj = V->AsObject();
			const FString Name = Obj->GetStringField(TEXT("name"));
			FString DisplayName;
			Obj->TryGetStringField(TEXT("displayName"), DisplayName);

			FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(NewEnum);
			const int32 NewIndex = NewEnum->NumEnums() - 2; // -1 是 _MAX
			FEnumEditorUtils::SetEnumeratorName(NewEnum, NewIndex, FName(*Name));
			if (!DisplayName.IsEmpty())
			{
				FEnumEditorUtils::SetEnumeratorDisplayName(NewEnum, NewIndex, FText::FromString(DisplayName));
			}
			++Count;
		}

		if (!UEditorAssetLibrary::SaveLoadedAsset(NewEnum))
		{
			SendError(Self, Socket, RequestId, TEXT("SAVE_FAILED"),
				FString::Printf(TEXT("Failed to save enum at '%s'"), *AssetPath));
			return true;
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), AssetPath);
		Data->SetNumberField(TEXT("enumeratorCount"), Count);
		TArray<TSharedPtr<FJsonValue>> WarningsArr;
		for (const FString& W : Warnings) WarningsArr.Add(MakeShared<FJsonValueString>(W));
		Data->SetArrayField(TEXT("warnings"), WarningsArr);

		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Created enum '%s' with %d enumerator(s)"), *AssetName, Count),
			Data);
		return true;
	}
#else
	bool HandleCreateEnum(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
			TEXT("create_enum requires editor build"));
		return true;
	}
#endif
#if WITH_EDITOR
	bool HandleCreateStruct(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString AssetPath, Err;
		if (!McpHandlerUtils::TryGetRequiredString(Payload, TEXT("assetPath"), AssetPath, Err))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"), Err);
			return true;
		}
		const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
		if (!Payload->TryGetArrayField(TEXT("members"), Members) || Members->Num() == 0)
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing or empty 'members' array"));
			return true;
		}
		const bool bOverwrite = Payload->HasTypedField<EJson::Boolean>(TEXT("overwrite")) &&
		                        Payload->GetBoolField(TEXT("overwrite"));

		FString PackagePath, AssetName;
		if (!SplitAssetPath(AssetPath, PackagePath, AssetName, Err))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"), Err);
			return true;
		}

		TArray<FString> Warnings;
		if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			if (!bOverwrite)
			{
				SendError(Self, Socket, RequestId, TEXT("ASSET_ALREADY_EXISTS"),
					FString::Printf(TEXT("Asset already exists at '%s'; pass overwrite=true to replace."), *AssetPath));
				return true;
			}
			const int32 Refs = CountReferencers(AssetPath);
			if (Refs > 0)
			{
				Warnings.Add(FString::Printf(
					TEXT("Replaced asset had %d referencer(s); pin types in dependent assets may now be unresolved."),
					Refs));
			}
			UObject* Existing = UEditorAssetLibrary::LoadAsset(AssetPath);
			if (!Existing || !UEditorAssetLibrary::DeleteLoadedAsset(Existing))
			{
				SendError(Self, Socket, RequestId, TEXT("OPERATION_FAILED"),
					FString::Printf(TEXT("Failed to delete existing asset at '%s'"), *AssetPath));
				return true;
			}
		}

		// Validate member names + parse all types up-front (fail-fast before any creation)
		TSet<FString> SeenNames;
		TArray<TPair<FString, FEdGraphPinType>> ParsedMembers;
		for (const TSharedPtr<FJsonValue>& V : *Members)
		{
			TSharedPtr<FJsonObject> Obj = V->AsObject();
			if (!Obj.IsValid())
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
					TEXT("Each member entry must be an object"));
				return true;
			}
			FString Name, TypeStr;
			if (!Obj->TryGetStringField(TEXT("name"), Name) || !IsValidIdentifier(Name))
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_NAME"),
					FString::Printf(TEXT("Invalid member name: '%s'"), *Name));
				return true;
			}
			if (SeenNames.Contains(Name))
			{
				SendError(Self, Socket, RequestId, TEXT("DUPLICATE_NAME"),
					FString::Printf(TEXT("Duplicate member name: '%s'"), *Name));
				return true;
			}
			SeenNames.Add(Name);
			if (!Obj->TryGetStringField(TEXT("type"), TypeStr))
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
					FString::Printf(TEXT("Member '%s' missing 'type'"), *Name));
				return true;
			}
			FEdGraphPinType PinType;
			FString TypeErr;
			if (!FMcpPinTypeParser::Parse(TypeStr, PinType, TypeErr))
			{
				SendError(Self, Socket, RequestId, TEXT("INVALID_TYPE_STRING"),
					FString::Printf(TEXT("Member '%s' type '%s': %s"), *Name, *TypeStr, *TypeErr));
				return true;
			}
			ParsedMembers.Emplace(Name, PinType);
		}

		// Create package + struct
		const FString FullObjectPath = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
		UPackage* Pkg = CreatePackage(*FullObjectPath);
		if (!Pkg)
		{
			SendError(Self, Socket, RequestId, TEXT("OPERATION_FAILED"),
				FString::Printf(TEXT("CreatePackage failed for '%s'"), *AssetPath));
			return true;
		}

		UUserDefinedStruct* NewStruct = FStructureEditorUtils::CreateUserDefinedStruct(
			Pkg, FName(*AssetName), RF_Public | RF_Standalone);
		if (!NewStruct)
		{
			SendError(Self, Socket, RequestId, TEXT("OPERATION_FAILED"),
				FString::Printf(TEXT("CreateUserDefinedStruct returned null for '%s'"), *AssetPath));
			return true;
		}

		// Add members
		for (const TPair<FString, FEdGraphPinType>& M : ParsedMembers)
		{
			FStructureEditorUtils::AddVariable(NewStruct, M.Value);
			const auto& Descs = FStructureEditorUtils::GetVarDesc(NewStruct);
			if (Descs.Num() == 0)
			{
				SendError(Self, Socket, RequestId, TEXT("OPERATION_FAILED"),
					FString::Printf(TEXT("AddVariable did not append a desc for '%s'"), *M.Key));
				return true;
			}
			FStructureEditorUtils::RenameVariable(NewStruct, Descs.Last().VarGuid, M.Key);
		}

		if (!UEditorAssetLibrary::SaveLoadedAsset(NewStruct))
		{
			SendError(Self, Socket, RequestId, TEXT("SAVE_FAILED"),
				FString::Printf(TEXT("Failed to save struct at '%s'"), *AssetPath));
			return true;
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("assetPath"), AssetPath);
		Data->SetNumberField(TEXT("memberCount"), ParsedMembers.Num());
		TArray<TSharedPtr<FJsonValue>> WarningsArr;
		for (const FString& W : Warnings) WarningsArr.Add(MakeShared<FJsonValueString>(W));
		Data->SetArrayField(TEXT("warnings"), WarningsArr);
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Created struct '%s' with %d member(s)"), *AssetName, ParsedMembers.Num()),
			Data);
		return true;
	}
#else
	bool HandleCreateStruct(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
			TEXT("create_struct requires editor build"));
		return true;
	}
#endif
#if WITH_EDITOR
	bool HandleModifyEnum(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString AssetPath, Err;
		if (!McpHandlerUtils::TryGetRequiredString(Payload, TEXT("assetPath"), AssetPath, Err))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"), Err);
			return true;
		}
		const TArray<TSharedPtr<FJsonValue>>* Operations = nullptr;
		if (!Payload->TryGetArrayField(TEXT("operations"), Operations) || Operations->Num() == 0)
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"),
				TEXT("Missing or empty 'operations' array"));
			return true;
		}
		if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			SendError(Self, Socket, RequestId, TEXT("ASSET_NOT_FOUND"),
				FString::Printf(TEXT("Asset not found: '%s'"), *AssetPath));
			return true;
		}
		UUserDefinedEnum* UDE = Cast<UUserDefinedEnum>(UEditorAssetLibrary::LoadAsset(AssetPath));
		if (!UDE)
		{
			SendError(Self, Socket, RequestId, TEXT("ASSET_WRONG_TYPE"),
				FString::Printf(TEXT("Asset at '%s' is not a UUserDefinedEnum"), *AssetPath));
			return true;
		}

		auto SendPartial = [&](int32 OpIdx, const TSharedPtr<FJsonObject>& OpJson,
		                       const FString& Code, const FString& Message)
		{
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("status"), TEXT("partial_failure"));
			Data->SetNumberField(TEXT("completedOps"), OpIdx);
			Data->SetNumberField(TEXT("failedOpIndex"), OpIdx);
			Data->SetObjectField(TEXT("failedOp"), OpJson);
			Data->SetStringField(TEXT("errorCode"), Code);
			Self->SendAutomationResponse(Socket, RequestId, false, Message, Data, Code);
		};

		int32 OpIdx = 0;
		for (const TSharedPtr<FJsonValue>& V : *Operations)
		{
			TSharedPtr<FJsonObject> Op = V->AsObject();
			if (!Op.IsValid())
			{
				SendPartial(OpIdx, MakeShared<FJsonObject>(),
					TEXT("INVALID_PARAMS"), TEXT("Operation must be an object"));
				return true;
			}
			FString OpName;
			if (!Op->TryGetStringField(TEXT("op"), OpName))
			{
				SendPartial(OpIdx, Op, TEXT("INVALID_PARAMS"), TEXT("Operation missing 'op'"));
				return true;
			}
			const int32 N = UDE->NumEnums() - 1; // exclude _MAX

			if (OpName == TEXT("add"))
			{
				FString Name;
				if (!Op->TryGetStringField(TEXT("name"), Name) || !IsValidIdentifier(Name))
				{
					SendPartial(OpIdx, Op, TEXT("INVALID_NAME"), TEXT("Invalid 'name'"));
					return true;
				}
				FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(UDE);
				const int32 NewIdx = UDE->NumEnums() - 2;
				FEnumEditorUtils::SetEnumeratorName(UDE, NewIdx, FName(*Name));
				FString DN;
				if (Op->TryGetStringField(TEXT("displayName"), DN) && !DN.IsEmpty())
				{
					FEnumEditorUtils::SetEnumeratorDisplayName(UDE, NewIdx, FText::FromString(DN));
				}
			}
			else if (OpName == TEXT("remove"))
			{
				int32 Idx;
				if (!Op->TryGetNumberField(TEXT("index"), Idx) || Idx < 0 || Idx >= N)
				{
					SendPartial(OpIdx, Op, TEXT("INDEX_OUT_OF_RANGE"),
						FString::Printf(TEXT("Index out of range [0,%d)"), N));
					return true;
				}
				FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(UDE, Idx);
			}
			else if (OpName == TEXT("rename"))
			{
				int32 Idx;
				FString NewName;
				if (!Op->TryGetNumberField(TEXT("index"), Idx) || Idx < 0 || Idx >= N)
				{
					SendPartial(OpIdx, Op, TEXT("INDEX_OUT_OF_RANGE"),
						FString::Printf(TEXT("Index out of range [0,%d)"), N));
					return true;
				}
				if (!Op->TryGetStringField(TEXT("newName"), NewName) || !IsValidIdentifier(NewName))
				{
					SendPartial(OpIdx, Op, TEXT("INVALID_NAME"), TEXT("Invalid 'newName'"));
					return true;
				}
				FEnumEditorUtils::SetEnumeratorName(UDE, Idx, FName(*NewName));
			}
			else if (OpName == TEXT("set_display_name"))
			{
				int32 Idx;
				FString NewDN;
				if (!Op->TryGetNumberField(TEXT("index"), Idx) || Idx < 0 || Idx >= N)
				{
					SendPartial(OpIdx, Op, TEXT("INDEX_OUT_OF_RANGE"),
						FString::Printf(TEXT("Index out of range [0,%d)"), N));
					return true;
				}
				if (!Op->TryGetStringField(TEXT("newDisplayName"), NewDN))
				{
					SendPartial(OpIdx, Op, TEXT("INVALID_PARAMS"), TEXT("Missing 'newDisplayName'"));
					return true;
				}
				FEnumEditorUtils::SetEnumeratorDisplayName(UDE, Idx, FText::FromString(NewDN));
			}
			else
			{
				SendPartial(OpIdx, Op, TEXT("INVALID_PARAMS"),
					FString::Printf(TEXT("Unknown op: '%s'"), *OpName));
				return true;
			}
			++OpIdx;
		}

		if (!UEditorAssetLibrary::SaveLoadedAsset(UDE))
		{
			SendError(Self, Socket, RequestId, TEXT("SAVE_FAILED"),
				FString::Printf(TEXT("Failed to save enum at '%s'"), *AssetPath));
			return true;
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("status"), TEXT("success"));
		Data->SetStringField(TEXT("assetPath"), AssetPath);
		Data->SetNumberField(TEXT("completedOps"), OpIdx);
		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Modified enum '%s' (%d ops)"), *UDE->GetName(), OpIdx),
			Data);
		return true;
	}
#else
	bool HandleModifyEnum(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
			TEXT("modify_enum requires editor build"));
		return true;
	}
#endif
	bool HandleModifyStruct(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("modify_struct stub"));
		return true;
	}
#if WITH_EDITOR
	bool HandleInspectEnum(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		FString AssetPath, Err;
		if (!McpHandlerUtils::TryGetRequiredString(Payload, TEXT("assetPath"), AssetPath, Err))
		{
			SendError(Self, Socket, RequestId, TEXT("INVALID_PARAMS"), Err);
			return true;
		}
		if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			SendError(Self, Socket, RequestId, TEXT("ASSET_NOT_FOUND"),
				FString::Printf(TEXT("Asset not found: '%s'"), *AssetPath));
			return true;
		}
		UObject* Obj = UEditorAssetLibrary::LoadAsset(AssetPath);
		UUserDefinedEnum* UDE = Cast<UUserDefinedEnum>(Obj);
		if (!UDE)
		{
			SendError(Self, Socket, RequestId, TEXT("ASSET_WRONG_TYPE"),
				FString::Printf(TEXT("Asset at '%s' is not a UUserDefinedEnum"), *AssetPath));
			return true;
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("name"), UDE->GetName());
		Data->SetStringField(TEXT("assetPath"), AssetPath);

		TArray<TSharedPtr<FJsonValue>> Arr;
		// NumEnums() includes _MAX; iterate up to NumEnums() - 1.
		const int32 N = UDE->NumEnums() - 1;
		for (int32 i = 0; i < N; ++i)
		{
			TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
			E->SetStringField(TEXT("name"), UDE->GetNameStringByIndex(i));
			E->SetStringField(TEXT("displayName"), UDE->GetDisplayNameTextByIndex(i).ToString());
			E->SetNumberField(TEXT("index"), i);
			Arr.Add(MakeShared<FJsonValueObject>(E));
		}
		Data->SetArrayField(TEXT("enumerators"), Arr);

		SendSuccess(Self, Socket, RequestId,
			FString::Printf(TEXT("Inspected enum '%s' (%d enumerators)"), *UDE->GetName(), N),
			Data);
		return true;
	}
#else
	bool HandleInspectEnum(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
			TEXT("inspect_enum requires editor build"));
		return true;
	}
#endif
	bool HandleInspectStruct(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
	                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
	{
		SendError(Self, Socket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("inspect_struct stub"));
		return true;
	}
}

bool HandleAction(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                  const FString& Action, const TSharedPtr<FJsonObject>& Payload,
                  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
	const FString Lower = Action.ToLower();
	if (Lower == TEXT("create_enum"))    return HandleCreateEnum   (Self, RequestId, Payload, RequestingSocket);
	if (Lower == TEXT("create_struct"))  return HandleCreateStruct (Self, RequestId, Payload, RequestingSocket);
	if (Lower == TEXT("modify_enum"))    return HandleModifyEnum   (Self, RequestId, Payload, RequestingSocket);
	if (Lower == TEXT("modify_struct"))  return HandleModifyStruct (Self, RequestId, Payload, RequestingSocket);
	if (Lower == TEXT("inspect_enum"))   return HandleInspectEnum  (Self, RequestId, Payload, RequestingSocket);
	if (Lower == TEXT("inspect_struct")) return HandleInspectStruct(Self, RequestId, Payload, RequestingSocket);
	return false;
#else
	return false;
#endif
}

} // namespace McpBlueprintTypeHandlers
