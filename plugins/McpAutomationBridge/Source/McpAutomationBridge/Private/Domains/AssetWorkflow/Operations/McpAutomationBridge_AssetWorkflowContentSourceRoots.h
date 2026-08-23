#pragma once

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#if WITH_EDITOR

/**
 * Content source roots for asset ingestion (list_content_sources, migrate_assets).
 *
 * SECURITY: a migrate request never carries a filesystem path. It carries a
 * root TOKEN plus a relative id underneath it, and both are resolved here. That
 * keeps arbitrary-directory reads off the wire entirely rather than trying to
 * reject them after the fact, which is how `..`, UNC paths, symlinks and
 * drive-relative forms leak through path filters.
 *
 * Roots deliberately cover only content shipped with the engine or downloaded
 * by a first-party launcher (Quixel Bridge / Fab), plus the current project.
 */
namespace McpContentSources
{
/** First candidate that exists, else the first candidate (stable to report). */
inline FString FirstExistingDir(const TArray<FString>& Candidates)
{
	for (const FString& Candidate : Candidates)
	{
		if (!Candidate.IsEmpty() && IFileManager::Get().DirectoryExists(*Candidate))
		{
			return FPaths::ConvertRelativePathToFull(Candidate);
		}
	}
	return Candidates.Num() > 0 ? FPaths::ConvertRelativePathToFull(Candidates[0]) : FString();
}

/**
 * Bridge/Megascans library location.
 *
 * FPlatformProcess::UserDir() returns the SHELL Documents folder, which OneDrive
 * redirects (and localizes — it came back as
 * "OneDrive/ドキュメント" on the machine this was first
 * run on) while Bridge keeps writing to the real profile Documents. Probing one
 * path reported a library that was not there, so try both and let the override
 * win outright.
 */
inline FString MegascansLibraryDir()
{
	const FString Override = FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_MEGASCANS_LIBRARY_DIR"));
	if (!Override.IsEmpty())
	{
		return FPaths::ConvertRelativePathToFull(Override);
	}
	const FString Suffix = FPaths::Combine(TEXT("Megascans Library"), TEXT("Downloaded"), TEXT("UAssets"));
	const FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	TArray<FString> Candidates;
	Candidates.Add(FPaths::Combine(FPlatformProcess::UserDir(), Suffix));
	if (!UserProfile.IsEmpty())
	{
		Candidates.Add(FPaths::Combine(UserProfile, TEXT("Documents"), Suffix));
	}
	return FirstExistingDir(Candidates);
}

/**
 * Fab local library. The Fab plugin owns authentication and downloading; this
 * only reads where it put the results, so no Fab module dependency is taken and
 * the root degrades to "missing" when the plugin is absent. Mirrors
 * UFabSettings::CacheDirectoryPath, whose default is UserTempDir()/FabLibrary.
 */
inline FString FabLibraryDir()
{
	const FString Override = FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_FAB_LIBRARY_DIR"));
	if (!Override.IsEmpty())
	{
		return FPaths::ConvertRelativePathToFull(Override);
	}
	FString Configured;
	if (GConfig != nullptr)
	{
		// FDirectoryPath serializes as (Path="..."), so the struct text needs
		// unwrapping before it is a usable directory.
		GConfig->GetString(TEXT("/Script/Fab.FabSettings"), TEXT("CacheDirectoryPath"),
		                   Configured, GEditorPerProjectIni);
		if (Configured.Contains(TEXT("Path=")))
		{
			FParse::Value(*Configured, TEXT("Path="), Configured);
		}
	}
	TArray<FString> Candidates;
	if (!Configured.IsEmpty())
	{
		Candidates.Add(Configured);
	}
	Candidates.Add(FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("FabLibrary")));
	return FirstExistingDir(Candidates);
}

/**
 * Templates and FeaturePacks sit BESIDE Engine/ in a launcher install
 * (UE_5.7/Templates), but inside it in some source builds. Prefer whichever
 * exists; fall back to the install-root form so the token still resolves to a
 * stable path that list_content_sources can report as missing.
 */
inline FString EngineSiblingDir(const TCHAR* Leaf)
{
	const FString Inside = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineDir(), Leaf));
	if (IFileManager::Get().DirectoryExists(*Inside))
	{
		return Inside;
	}
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::RootDir(), Leaf));
}

/** Resolves a root token to an absolute directory. Empty when unknown. */
inline FString ResolveRootDir(const FString& RootToken)
{
	if (RootToken == TEXT("engineTemplates"))
	{
		return EngineSiblingDir(TEXT("Templates"));
	}
	if (RootToken == TEXT("engineFeaturePacks"))
	{
		return EngineSiblingDir(TEXT("FeaturePacks"));
	}
	if (RootToken == TEXT("engineContent"))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir());
	}
	if (RootToken == TEXT("enginePlugins"))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir());
	}
	if (RootToken == TEXT("megascansLibrary"))
	{
		return MegascansLibraryDir();
	}
	if (RootToken == TEXT("fabLibrary"))
	{
		return FabLibraryDir();
	}
	if (RootToken == TEXT("projectContent"))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	}
	if (RootToken == TEXT("projectPlugins"))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
	}
	return FString();
}

inline const TArray<FString>& AllRootTokens()
{
	static const TArray<FString> Tokens = {
		TEXT("engineTemplates"), TEXT("engineFeaturePacks"), TEXT("engineContent"),
		TEXT("enginePlugins"), TEXT("megascansLibrary"), TEXT("fabLibrary"),
		TEXT("projectContent"), TEXT("projectPlugins")};
	return Tokens;
}

/**
 * Joins RootToken + SourceId into an absolute directory, refusing anything that
 * escapes the root. SourceId is relative, forward-slashed, and may be empty
 * (meaning the root itself).
 */
inline bool ResolveSourceDir(const FString& RootToken, const FString& SourceId,
                             FString& OutAbsoluteDir, FString& OutError)
{
	const FString RootDir = ResolveRootDir(RootToken);
	if (RootDir.IsEmpty())
	{
		OutError = FString::Printf(
			TEXT("Unknown sourceRoot '%s'. Allowed: %s"), *RootToken,
			*FString::Join(AllRootTokens(), TEXT(", ")));
		return false;
	}

	FString Relative = SourceId;
	Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (Relative.Contains(TEXT("..")) || Relative.StartsWith(TEXT("/")) ||
		Relative.Contains(TEXT(":")))
	{
		OutError = TEXT("SECURITY_VIOLATION: sourceId must be a relative path with no '..', leading '/', or drive prefix");
		return false;
	}

	FString Candidate = FPaths::ConvertRelativePathToFull(
		Relative.IsEmpty() ? RootDir : FPaths::Combine(RootDir, Relative));
	FPaths::NormalizeDirectoryName(Candidate);

	FString NormalizedRoot = RootDir;
	FPaths::NormalizeDirectoryName(NormalizedRoot);
	if (Candidate != NormalizedRoot && !Candidate.StartsWith(NormalizedRoot + TEXT("/")))
	{
		OutError = TEXT("SECURITY_VIOLATION: resolved sourceId escapes its sourceRoot");
		return false;
	}

	OutAbsoluteDir = Candidate;
	return true;
}

/**
 * The directory whose children map onto /Game. For an engine template or a
 * plugin that is the `Content` subfolder; a Bridge pack is already laid out
 * under its own root. Returns Dir itself when there is no `Content` child.
 */
inline FString ContentDirFor(const FString& Dir)
{
	const FString WithContent = FPaths::Combine(Dir, TEXT("Content"));
	return IFileManager::Get().DirectoryExists(*WithContent) ? WithContent : Dir;
}
}

#endif
