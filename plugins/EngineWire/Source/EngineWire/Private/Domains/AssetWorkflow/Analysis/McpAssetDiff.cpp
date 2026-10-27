// asset diff_asset: an asset against its saved file (its unsaved edits) or against another asset of the same kind.
// The saved file is copied to Saved/Diff and loaded as a /Temp package with LOAD_ForDiff, as the editor's own diff
// does, then released; nothing is written to the asset.

#include "Domains/AssetWorkflow/Analysis/McpAssetDiff.h"
#include "Domains/AssetWorkflow/Analysis/Shared.h"

#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Linker.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

namespace
{
constexpr int32 AssetDiffTextChars = 200;

FString AssetDiffText(const FProperty* Property, const void* Container)
{
	FString Text;
	const void* Value = Property->ContainerPtrToValuePtr<void>(Container);
	MCP_PROPERTY_EXPORT_TEXT(Property, Text, Value, Value, nullptr, PPF_None);
	return Text;
}

FString AssetDiffClip(const FString& Text)
{
	return Text.Len() > AssetDiffTextChars ? Text.Left(AssetDiffTextChars - 3) + TEXT("...") : Text;
}

// Only what the package keeps: a transient value differs between a live object and a fresh load without any edit.
bool AssetDiffSaved(const FProperty* Property)
{
	return !Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_Deprecated);
}

TSharedPtr<FJsonObject> AssetDiffPropertyRow(const FString& Object, const FProperty* Property, const TCHAR* Change,
	const FString& Now, const FString& Saved)
{
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("object"), Object);
	Row->SetStringField(TEXT("property"), Property->GetAuthoredName());
	Row->SetStringField(TEXT("change"), Change);
	if (!Now.IsEmpty()) Row->SetStringField(TEXT("now"), AssetDiffClip(Now));
	if (!Saved.IsEmpty()) Row->SetStringField(TEXT("saved"), AssetDiffClip(Saved));
	return Row;
}

// The saved copy is for this read only: detach its file, let garbage collection take it, and delete the copy.
void AssetDiffRelease(UPackage* Package, const FString& Dir)
{
	if (Package)
	{
		ResetLoaders(Package);
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Package, Objects);
		for (UObject* Object : Objects)
		{
			Object->ClearFlags(RF_Standalone);
		}
		Package->ClearFlags(RF_Standalone);
	}
	if (!Dir.IsEmpty())
	{
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
	}
}

// A copy of the asset's saved file, loaded for diffing; nullptr with the reason when there is none.
UObject* AssetDiffLoadSaved(const UObject* Live, UPackage*& OutPackage, FString& OutDir, FString& OutCode, FString& OutError)
{
	const FString PackageName = Live->GetOutermost()->GetName();
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (!IFileManager::Get().FileExists(*Filename))
	{
		OutCode = TEXT("NOT_SAVED");
		OutError = FString::Printf(TEXT("%s has no saved file yet, so all of it is unsaved."), *PackageName);
		return nullptr;
	}
	OutDir = FPaths::DiffDir() / FString::Printf(TEXT("McpDiff_%s"), *FGuid::NewGuid().ToString());
	const FString Copy = OutDir / FPaths::GetCleanFilename(Filename);
	FString TempPackageName;
	if (IFileManager::Get().Copy(*Copy, *Filename) != COPY_OK ||
		!FPackageName::TryConvertFilenameToLongPackageName(Copy, TempPackageName))
	{
		OutCode = TEXT("DIFF_COPY_FAILED");
		OutError = FString::Printf(TEXT("The saved file of %s could not be copied for diffing."), *PackageName);
		return nullptr;
	}
	OutPackage = LoadPackage(nullptr, *TempPackageName, LOAD_ForDiff | LOAD_DisableCompileOnLoad);
	UObject* Saved = OutPackage ? FindObject<UObject>(OutPackage, *Live->GetName()) : nullptr;
	if (!Saved)
	{
		OutCode = TEXT("DIFF_LOAD_FAILED");
		OutError = FString::Printf(TEXT("The saved file of %s did not load for diffing."), *PackageName);
	}
	return Saved;
}
} // namespace

FString FMcpAssetDiff::Normalize(const FString& OtherText) const
{
	FString Text = OtherText.Replace(*OtherPackage, *LivePackage, ESearchCase::CaseSensitive);
	return OtherName.Equals(LiveName, ESearchCase::CaseSensitive) ? Text : Text.Replace(*OtherName, *LiveName, ESearchCase::CaseSensitive);
}

void FMcpAssetDiff::Add(TArray<TSharedPtr<FJsonValue>>& List, const TSharedPtr<FJsonObject>& Row)
{
	if (List.Num() >= MaxRows)
	{
		bTruncated = true;
		return;
	}
	List.Add(MakeShared<FJsonValueObject>(Row));
}

void FMcpAssetDiff::DiffContainers(const FString& Object, const UStruct* LiveType, const void* Live, const UStruct* OtherType, const void* Other)
{
	for (TFieldIterator<FProperty> It(LiveType); It; ++It)
	{
		if (!AssetDiffSaved(*It) || Skip.Contains(It->GetFName())) continue;
		const FString Now = AssetDiffText(*It, Live);
		const FProperty* OtherProperty = FindFProperty<FProperty>(OtherType, It->GetFName());
		if (!OtherProperty)
		{
			Add(Properties, AssetDiffPropertyRow(Object, *It, TEXT("added"), Now, FString()));
			continue;
		}
		const FString Saved = Normalize(AssetDiffText(OtherProperty, Other));
		if (!Now.Equals(Saved, ESearchCase::CaseSensitive))
		{
			Add(Properties, AssetDiffPropertyRow(Object, *It, TEXT("changed"), Now, Saved));
		}
	}
	for (TFieldIterator<FProperty> It(OtherType); It; ++It)
	{
		if (AssetDiffSaved(*It) && !Skip.Contains(It->GetFName()) && !FindFProperty<FProperty>(LiveType, It->GetFName()))
		{
			Add(Properties, AssetDiffPropertyRow(Object, *It, TEXT("removed"), FString(), Normalize(AssetDiffText(*It, Other))));
		}
	}
}

bool McpAssetDiff::HandleDiffAsset(UEngineWireSubsystem* Bridge, const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
	FString AssetPath, AgainstPath;
	Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
	Payload->TryGetStringField(TEXT("againstPath"), AgainstPath);
	const FString SafePath = SanitizeProjectRelativePath(AssetPath);
	const FString SafeAgainst = AgainstPath.IsEmpty() ? FString() : SanitizeProjectRelativePath(AgainstPath);
	if (SafePath.IsEmpty() || (!AgainstPath.IsEmpty() && SafeAgainst.IsEmpty()))
	{
		const bool bAsset = SafePath.IsEmpty();
		Bridge->SendAutomationError(Socket, RequestId,
			McpPathRefusalMessage(bAsset ? TEXT("assetPath") : TEXT("againstPath"), bAsset ? AssetPath : AgainstPath), TEXT("INVALID_PATH"));
		return true;
	}
	UObject* Live = McpLoadAsset(SafePath);
	if (!Live)
	{
		Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Asset not found: %s"), *SafePath), TEXT("ASSET_NOT_FOUND"));
		return true;
	}
	if (!Live->IsA<UBlueprint>() && !Live->IsA<UDataTable>() && !Live->IsA<UDataAsset>())
	{
		Bridge->SendAutomationError(Socket, RequestId, FString::Printf(
			TEXT("%s is a %s; diff_asset compares Blueprints, DataTables and data assets."), *SafePath, *Live->GetClass()->GetName()),
			TEXT("UNSUPPORTED_ASSET_TYPE"));
		return true;
	}

	UPackage* SavedPackage = nullptr;
	FString SavedDir, ErrorCode, Error;
	UObject* Other = SafeAgainst.IsEmpty()
		? AssetDiffLoadSaved(Live, SavedPackage, SavedDir, ErrorCode, Error)
		: McpLoadAsset(SafeAgainst);
	if (!SafeAgainst.IsEmpty() && !Other)
	{
		ErrorCode = TEXT("ASSET_NOT_FOUND");
		Error = FString::Printf(TEXT("Asset not found: %s"), *SafeAgainst);
	}
	else if (!SafeAgainst.IsEmpty() && (Live->IsA<UBlueprint>() ? !Other->IsA<UBlueprint>() : Other->GetClass() != Live->GetClass()))
	{
		ErrorCode = TEXT("KIND_MISMATCH");
		Error = FString::Printf(TEXT("%s is a %s and %s a %s; compare assets of the same kind."), *SafePath,
			*Live->GetClass()->GetName(), *SafeAgainst, *Other->GetClass()->GetName());
		Other = nullptr;
	}
	if (!Other)
	{
		AssetDiffRelease(SavedPackage, SavedDir);
		Bridge->SendAutomationError(Socket, RequestId, Error, ErrorCode);
		return true;
	}

	FMcpAssetDiff Diff;
	Diff.LivePackage = Live->GetOutermost()->GetName();
	Diff.OtherPackage = Other->GetOutermost()->GetName();
	Diff.LiveName = Live->GetName();
	Diff.OtherName = Other->GetName();
	// A Blueprint's variables read as its class defaults below; the open-tab list and the GUIDs (new in a copy) are
	// the editor's bookkeeping.
	if (Live->IsA<UBlueprint>())
	{
		Diff.Skip = {FName(TEXT("NewVariables")), FName(TEXT("LastEditedDocuments")), FName(TEXT("BlueprintGuid"))};
	}
	Diff.DiffContainers(TEXT("asset"), Live->GetClass(), Live, Other->GetClass(), Other);
	Diff.Skip.Reset();
	if (UBlueprint* LiveBlueprint = Cast<UBlueprint>(Live))
	{
		McpAssetDiff::DiffBlueprint(Diff, LiveBlueprint, Cast<UBlueprint>(Other));
	}
	if (UDataTable* LiveTable = Cast<UDataTable>(Live))
	{
		McpAssetDiff::DiffDataTable(Diff, LiveTable, Cast<UDataTable>(Other));
	}
	const FString Against = SafeAgainst.IsEmpty() ? FString(TEXT("saved file")) : Other->GetPathName();
	AssetDiffRelease(SavedPackage, SavedDir);

	const int32 Count = Diff.Properties.Num() + Diff.Components.Num() + Diff.Graphs.Num() + Diff.Rows.Num();
	TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
	Resp->SetBoolField(TEXT("success"), true);
	Resp->SetStringField(TEXT("assetPath"), Live->GetPathName());
	Resp->SetStringField(TEXT("against"), Against);
	Resp->SetBoolField(TEXT("identical"), Count == 0);
	Resp->SetArrayField(TEXT("propertyChanges"), Diff.Properties);
	Resp->SetArrayField(TEXT("componentChanges"), Diff.Components);
	Resp->SetArrayField(TEXT("graphChanges"), Diff.Graphs);
	Resp->SetArrayField(TEXT("rowChanges"), Diff.Rows);
	if (Diff.bTruncated) Resp->SetBoolField(TEXT("truncated"), true);
	const FString AgainstText = SafeAgainst.IsEmpty() ? FString(TEXT("its saved file")) : Against;
	const FString Message = Count == 0
		? FString::Printf(TEXT("%s matches %s."), *Diff.LiveName, *AgainstText)
		: FString::Printf(TEXT("%s differs from %s: %d property, %d component, %d graph and %d row change(s)%s."),
			*Diff.LiveName, *AgainstText, Diff.Properties.Num(), Diff.Components.Num(),
			Diff.Graphs.Num(), Diff.Rows.Num(), Diff.bTruncated ? TEXT(", lists cut at 50") : TEXT(""));
	Bridge->SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
	return true;
}
