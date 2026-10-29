// manage_asset reimport: assets brought up to date with their source files, as the Content Browser's Reimport does but
// with no dialog (an automated reimport): each from the source file it was imported from, or from sourceFile, a file in
// the project, for a single asset. Every asset is checked before any is reimported, so a refusal changes nothing; an
// asset whose recorded file is gone is listed, not reimported. Saved after. A reimport writes the asset from disk, so
// editor undo does not take it back.

#include "Domains/AssetWorkflow/Reimport/McpAssetReimport.h"

#include "Dom/JsonObject.h"
#include "EditorReimportHandler.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Safety/McpSafeOperations.h"

namespace McpAssetReimport
{
namespace
{
struct FPlan
{
	UObject* Asset = nullptr;
	FString Path;
	FString Source;
};
} // namespace

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
	const auto Refuse = [&](const FString& Message, const TCHAR* Code)
	{
		Bridge.SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was reimported."), Code);
		return true;
	};
	const TArray<FString> Paths = McpHandlerUtils::GetStringArrayField(Payload, TEXT("assetPaths"));
	const FString Override = GetJsonStringField(Payload, TEXT("sourceFile"));
	if (Paths.Num() == 0)
	{
		return Refuse(TEXT("assetPaths names the assets to reimport (1-50)."), TEXT("INVALID_ARGUMENT"));
	}
	if (!Override.IsEmpty() && Paths.Num() != 1)
	{
		return Refuse(TEXT("sourceFile replaces the source of one asset: name exactly one in assetPaths."), TEXT("INVALID_ARGUMENT"));
	}
	FString OverrideFile;
	if (!Override.IsEmpty())
	{
		FString Error;
		if (!McpResolveProjectFilePath(Override, OverrideFile, Error))
		{
			return Refuse(Error, TEXT("SECURITY_VIOLATION")); // as import: a file outside the project is never read
		}
		if (!FPaths::FileExists(OverrideFile))
		{
			return Refuse(FString::Printf(TEXT("No file is at %s."), *Override), TEXT("SOURCE_FILE_MISSING"));
		}
	}
	TArray<FPlan> Plans;
	TArray<TSharedPtr<FJsonValue>> Skipped;
	for (const FString& Path : Paths)
	{
		UObject* Asset = McpHandlerUtils::ResolveObjectFromPath(Path);
		if (!Asset)
		{
			return Refuse(FString::Printf(TEXT("No asset is at %s."), *Path), TEXT("ASSET_NOT_FOUND"));
		}
		if (Asset->GetPathName().StartsWith(TEXT("/Engine/")))
		{
			return Refuse(FString::Printf(TEXT("%s is engine content, which is never changed: duplicate it into /Game first."), *Asset->GetPathName()),
				TEXT("ENGINE_CONTENT"));
		}
		TArray<FString> Sources;
		if (!FReimportManager::Instance()->CanReimport(Asset, &Sources))
		{
			return Refuse(FString::Printf(TEXT("%s has no source file recorded (made in the editor, or a kind that is never imported)."),
				*Asset->GetPathName()), TEXT("NOT_REIMPORTABLE"));
		}
		const FString Source = !OverrideFile.IsEmpty() ? OverrideFile : Sources.Num() > 0 ? Sources[0] : FString();
		if (Source.IsEmpty() || !IFileManager::Get().FileExists(*Source))
		{
			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("assetPath"), FPackageName::ObjectPathToPackageName(Asset->GetPathName()));
			Row->SetStringField(TEXT("sourceFile"), Source);
			Row->SetStringField(TEXT("reason"), TEXT("source file missing"));
			Skipped.Add(MakeShared<FJsonValueObject>(Row));
			continue;
		}
		Plans.Add({Asset, FPackageName::ObjectPathToPackageName(Asset->GetPathName()), Source});
	}
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetArrayField(TEXT("missingSources"), Skipped);
	if (Plans.Num() == 0)
	{
		Bridge.SendAutomationResponse(Socket, RequestId, false, TEXT("No source file of these assets is on disk any more (missingSources): "
			"sourceFile points one asset at another file. Nothing was reimported."), Result, TEXT("SOURCE_FILE_MISSING"));
		return true;
	}
	const bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);
	TArray<TSharedPtr<FJsonValue>> Rows;
	int32 Reimported = 0;
	for (const FPlan& Plan : Plans)
	{
		// Automated: no dialog and no toast; another file only through sourceFile, never a file picker.
		const bool bDone = FReimportManager::Instance()->Reimport(Plan.Asset, false, false, OverrideFile, nullptr, INDEX_NONE,
			!OverrideFile.IsEmpty(), true);
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("assetPath"), Plan.Path);
		Row->SetStringField(TEXT("sourceFile"), Plan.Source);
		Row->SetBoolField(TEXT("reimported"), bDone);
		if (bDone && bSave)
		{
			Row->SetBoolField(TEXT("saved"), McpSafeOperations::McpSafeAssetSave(Plan.Asset));
		}
		Reimported += bDone ? 1 : 0;
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	Result->SetArrayField(TEXT("assets"), Rows);
	Result->SetNumberField(TEXT("reimported"), Reimported);
	const bool bAll = Reimported == Plans.Num();
	const FString Missing = Skipped.Num() ? FString::Printf(TEXT("; %d left out, their source file gone (missingSources)"), Skipped.Num()) : FString();
	Bridge.SendAutomationResponse(Socket, RequestId, bAll, bAll ? FString::Printf(TEXT("Reimported %d asset(s)%s."), Reimported, *Missing)
		: FString::Printf(TEXT("%d of %d reimport(s) failed (the editor log says why); the rest were reimported%s."), Plans.Num() - Reimported,
		Plans.Num(), *Missing), Result, bAll ? FString() : FString(TEXT("REIMPORT_FAILED")));
	return true;
}
}
