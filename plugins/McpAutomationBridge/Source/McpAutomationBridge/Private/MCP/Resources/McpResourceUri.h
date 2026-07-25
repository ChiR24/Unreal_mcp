// McpResourceUri.h
// Task 31 (native mirror): URI parse, path redaction, and byte-budget helpers
// for the version-aware read-only resource surface. Mirrors the guards in the
// TypeScript `src/resources/resource-errors.ts`. Every failure is reported as a
// boolean-plus-reason so a failed read never yields a success-shaped payload,
// and no host filesystem path is ever addressable. Task 37 owns wiring these
// into `resources/read`; this header is helper logic only and is compiled only
// where included.
#pragma once

#include "CoreMinimal.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"
#include "Misc/Paths.h"

namespace McpResourceUri
{
	// Maximum serialized byte size for a single bounded resource read (64 KiB).
	// Mirrors the TypeScript `MAX_RESOURCE_BYTES`.
	inline constexpr int64 MaxResourceBytes = 65536;

	// UE content mount roots a normalized object/asset handle may reference.
	// Mirrors the TypeScript `UE_CONTENT_ROOTS`.
	inline const TArray<FString>& ContentRoots()
	{
		static const TArray<FString> Roots = {
			TEXT("/Game"), TEXT("/Engine"), TEXT("/Script"), TEXT("/Temp"), TEXT("/Niagara"),
		};
		return Roots;
	}

	inline bool IsUnderContentRoot(const FString& Path)
	{
		for (const FString& Root : ContentRoots())
		{
			if (Path == Root || Path.StartsWith(Root + TEXT("/"), ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	inline bool LooksLikeHostPath(const FString& Raw)
	{
		if (Raw.Contains(TEXT("\\")))
		{
			return true;
		}
		if (Raw.Len() >= 2 && FChar::IsAlpha(Raw[0]) && Raw[1] == TEXT(':'))
		{
			return true;
		}
		const FString Lower = Raw.ToLower();
		static const TArray<FString> HostRoots = {
			TEXT("/home/"), TEXT("/users/"), TEXT("/etc/"), TEXT("/var/"), TEXT("/root/"), TEXT("/tmp/"),
		};
		for (const FString& HostRoot : HostRoots)
		{
			if (Lower.StartsWith(HostRoot, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	// Normalize a template path parameter into a safe UE content handle. Rejects
	// directory traversal, host filesystem paths, and any path that does not
	// resolve under a known UE mount root. Mirrors `normalizeContentPath`.
	inline bool TryNormalizeContentPath(const FString& Raw, FString& OutNormalized, FString& OutError)
	{
		if (Raw.IsEmpty())
		{
			OutError = TEXT("RESOURCE_INVALID_URI");
			return false;
		}
		if (LooksLikeHostPath(Raw))
		{
			OutError = TEXT("RESOURCE_TRAVERSAL_REJECTED");
			return false;
		}

		FString Normalized = Raw;
		while (Normalized.Contains(TEXT("//")))
		{
			Normalized = Normalized.Replace(TEXT("//"), TEXT("/"));
		}
		McpAssetPathCanonical::MapContentRootInline(Normalized);
		if (!Normalized.StartsWith(TEXT("/")))
		{
			Normalized = TEXT("/") + Normalized;
		}

		TArray<FString> Segments;
		Normalized.ParseIntoArray(Segments, TEXT("/"), true);
		if (Segments.Contains(TEXT("..")))
		{
			OutError = TEXT("RESOURCE_TRAVERSAL_REJECTED");
			return false;
		}
		if (Normalized.Len() > 1 && Normalized.EndsWith(TEXT("/")))
		{
			Normalized = Normalized.LeftChop(1);
		}
		if (!IsUnderContentRoot(Normalized))
		{
			OutError = TEXT("RESOURCE_INVALID_URI");
			return false;
		}

		OutNormalized = Normalized;
		return true;
	}

	// Reduce a raw project path to just the project name; never returns a
	// directory or extension. Mirrors `redactProjectName`.
	inline FString RedactProjectName(const FString& Raw)
	{
		const FString Trimmed = Raw.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return FString();
		}
		return FPaths::GetBaseFilename(Trimmed);
	}
}
