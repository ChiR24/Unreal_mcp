#include "MCP/DynamicTools/McpSessionConfigureStore.h"
#include "Misc/ScopeLock.h"

namespace McpSessionConfigureInternal
{
	// Closed, policy-bounded set of numeric limits. Unknown keys are rejected.
	static bool LimitBounds(const FString& Key, int64& OutMin, int64& OutMax)
	{
		if (Key == TEXT("maxResults")) { OutMin = 1; OutMax = 1000; return true; }
		if (Key == TEXT("maxDepth")) { OutMin = 1; OutMax = 32; return true; }
		if (Key == TEXT("pageSize")) { OutMin = 1; OutMax = 200; return true; }
		return false;
	}

	static constexpr int32 MaxPreferenceKeys = 16;
	static constexpr int32 MaxPreferenceValueLength = 256;

	static TArray<TSharedPtr<FJsonValue>> ToJsonArray(const TArray<FString>& Names)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const FString& Name : Names) Out.Add(MakeShared<FJsonValueString>(Name));
		return Out;
	}
}

bool FMcpSessionConfigureStore::IsProtectedTool(const FString& Name)
{
	return Name == TEXT("manage_tools") || Name == TEXT("inspect");
}

bool FMcpSessionConfigureStore::IsProtectedCategory(const FString& Name)
{
	return Name == TEXT("core");
}

// Only the enabled flags IsToolEnabled_NoLock() reads, sorted for determinism, so
// a limit/preference change or a reset that only rewrites caches is never a move.
FString FMcpSessionConfigureStore::Fingerprint(const FOverlay& Overlay)
{
	TArray<FString> Parts;
	for (const auto& Pair : Overlay.ToolStates)
	{
		Parts.Add(FString::Printf(TEXT("t:%s=%d"), *Pair.Key, Pair.Value.bEnabled ? 1 : 0));
	}
	for (const auto& Pair : Overlay.CategoryStates)
	{
		Parts.Add(FString::Printf(TEXT("c:%s=%d"), *Pair.Key, Pair.Value.bEnabled ? 1 : 0));
	}
	Parts.Sort();
	return FString::Join(Parts, TEXT(","));
}

void FMcpSessionConfigureStore::SeedFrom(const TArray<FSeedEntry>& Entries)
{
	FScopeLock Lock(&StateMutex);
	Seed = Entries;
	Overlays.Empty();
}

FMcpSessionConfigureStore::FOverlay& FMcpSessionConfigureStore::OverlayFor_NoLock(const FString& SessionId) const
{
	if (FOverlay* Existing = Overlays.Find(SessionId))
	{
		return *Existing;
	}

	FOverlay& Overlay = Overlays.Add(SessionId);
	for (const FSeedEntry& Entry : Seed)
	{
		FToolState& Tool = Overlay.ToolStates.Add(Entry.Name);
		Tool.Name = Entry.Name;
		Tool.Category = Entry.Category;
		Tool.bEnabled = true;

		FCategoryState& Category = Overlay.CategoryStates.FindOrAdd(Entry.Category);
		Category.Name = Entry.Category;
		Category.bEnabled = true;
	}
	return Overlay;
}

bool FMcpSessionConfigureStore::IsToolEnabled_NoLock(const FOverlay& Overlay, const FString& ToolName) const
{
	const FToolState* Tool = Overlay.ToolStates.Find(ToolName);
	if (!Tool) return false;
	const FCategoryState* Category = Overlay.CategoryStates.Find(Tool->Category);
	return Tool->bEnabled && (!Category || Category->bEnabled);
}

uint64 FMcpSessionConfigureStore::GetCatalogStateRevision(const FString& SessionId) const
{
	FScopeLock Lock(&StateMutex);
	const FOverlay* Overlay = Overlays.Find(SessionId);
	return Overlay ? Overlay->CatalogStateRevision : 0;
}

bool FMcpSessionConfigureStore::HasSession(const FString& SessionId) const
{
	FScopeLock Lock(&StateMutex);
	return Overlays.Contains(SessionId);
}

bool FMcpSessionConfigureStore::ClearSession(const FString& SessionId)
{
	FScopeLock Lock(&StateMutex);
	return Overlays.Remove(SessionId) > 0;
}

TSharedPtr<FJsonObject> FMcpSessionConfigureStore::EnableTools(const FString& SessionId, const TArray<FString>& ToolNames)
{
	FScopeLock Lock(&StateMutex);
	FOverlay& Overlay = OverlayFor_NoLock(SessionId);
	const FString Before = Fingerprint(Overlay);

	TArray<FString> Enabled;
	TArray<FString> NotFound;
	for (const FString& Name : ToolNames)
	{
		FToolState* Tool = Overlay.ToolStates.Find(Name);
		if (!Tool) { NotFound.Add(Name); continue; }
		if (FCategoryState* Category = Overlay.CategoryStates.Find(Tool->Category)) Category->bEnabled = true;
		Tool->bEnabled = true;
		Enabled.Add(Name);
	}

	if (Fingerprint(Overlay) != Before) ++Overlay.CatalogStateRevision;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("enabled"), McpSessionConfigureInternal::ToJsonArray(Enabled));
	Result->SetArrayField(TEXT("notFound"), McpSessionConfigureInternal::ToJsonArray(NotFound));
	return Result;
}

TSharedPtr<FJsonObject> FMcpSessionConfigureStore::DisableTools(const FString& SessionId, const TArray<FString>& ToolNames)
{
	FScopeLock Lock(&StateMutex);
	FOverlay& Overlay = OverlayFor_NoLock(SessionId);
	const FString Before = Fingerprint(Overlay);

	TArray<FString> Disabled;
	TArray<FString> NotFound;
	TArray<FString> Protected;
	for (const FString& Name : ToolNames)
	{
		if (IsProtectedTool(Name)) { Protected.Add(Name); continue; }
		FToolState* Tool = Overlay.ToolStates.Find(Name);
		if (!Tool) { NotFound.Add(Name); continue; }
		Tool->bEnabled = false;
		Disabled.Add(Name);
	}

	if (Fingerprint(Overlay) != Before) ++Overlay.CatalogStateRevision;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("disabled"), McpSessionConfigureInternal::ToJsonArray(Disabled));
	Result->SetArrayField(TEXT("notFound"), McpSessionConfigureInternal::ToJsonArray(NotFound));
	Result->SetArrayField(TEXT("protected"), McpSessionConfigureInternal::ToJsonArray(Protected));
	return Result;
}

TSharedPtr<FJsonObject> FMcpSessionConfigureStore::DisableCategory(const FString& SessionId, const FString& Category)
{
	FScopeLock Lock(&StateMutex);
	FOverlay& Overlay = OverlayFor_NoLock(SessionId);
	const FString Before = Fingerprint(Overlay);

	TArray<FString> Disabled;
	TArray<FString> Protected;
	const bool bAll = (Category == TEXT("all"));

	// A protected category is rejected outright, never partially applied: the
	// catalog advertises that core cannot be disabled.
	if (!bAll && IsProtectedCategory(Category))
	{
		for (const auto& Pair : Overlay.ToolStates)
		{
			if (Pair.Value.Category == Category && IsProtectedTool(Pair.Key)) Protected.Add(Pair.Key);
		}
	}
	else
	{
		for (auto& Pair : Overlay.ToolStates)
		{
			if (!bAll && Pair.Value.Category != Category) continue;
			if (IsProtectedTool(Pair.Key) || IsProtectedCategory(Pair.Value.Category)) { Protected.Add(Pair.Key); continue; }
			if (Pair.Value.bEnabled) { Pair.Value.bEnabled = false; Disabled.Add(Pair.Key); }
		}
		for (auto& Pair : Overlay.CategoryStates)
		{
			if (IsProtectedCategory(Pair.Key)) continue;
			if (bAll || Pair.Key == Category) Pair.Value.bEnabled = false;
		}
	}

	if (Fingerprint(Overlay) != Before) ++Overlay.CatalogStateRevision;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("disabled"), McpSessionConfigureInternal::ToJsonArray(Disabled));
	Result->SetArrayField(TEXT("protected"), McpSessionConfigureInternal::ToJsonArray(Protected));
	return Result;
}

TSharedPtr<FJsonObject> FMcpSessionConfigureStore::Reset(const FString& SessionId)
{
	FScopeLock Lock(&StateMutex);
	FOverlay& Overlay = OverlayFor_NoLock(SessionId);
	const FString Before = Fingerprint(Overlay);

	int32 Changed = 0;
	for (auto& Pair : Overlay.ToolStates)
	{
		if (!Pair.Value.bEnabled) { Pair.Value.bEnabled = true; ++Changed; }
	}
	for (auto& Pair : Overlay.CategoryStates)
	{
		Pair.Value.bEnabled = true;
	}

	if (Fingerprint(Overlay) != Before) ++Overlay.CatalogStateRevision;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("enabled"), Changed);
	return Result;
}

bool FMcpSessionConfigureStore::SetLimit(const FString& SessionId, const FString& Key, int64 Value)
{
	int64 Min = 0;
	int64 Max = 0;
	if (!McpSessionConfigureInternal::LimitBounds(Key, Min, Max)) return false;

	const int64 Clamped = FMath::Clamp(Value, Min, Max);
	FScopeLock Lock(&StateMutex);
	OverlayFor_NoLock(SessionId).Limits.Add(Key, Clamped);
	return true;
}

bool FMcpSessionConfigureStore::SetPreference(const FString& SessionId, const FString& Key, const FString& Value)
{
	if (Value.Len() > McpSessionConfigureInternal::MaxPreferenceValueLength) return false;

	FScopeLock Lock(&StateMutex);
	FOverlay& Overlay = OverlayFor_NoLock(SessionId);
	if (!Overlay.Preferences.Contains(Key) && Overlay.Preferences.Num() >= McpSessionConfigureInternal::MaxPreferenceKeys)
	{
		return false;
	}
	Overlay.Preferences.Add(Key, Value);
	return true;
}

bool FMcpSessionConfigureStore::IsToolEnabled(const FString& SessionId, const FString& ToolName) const
{
	FScopeLock Lock(&StateMutex);
	return IsToolEnabled_NoLock(OverlayFor_NoLock(SessionId), ToolName);
}

TSharedPtr<FJsonObject> FMcpSessionConfigureStore::GetStatus(const FString& SessionId) const
{
	FScopeLock Lock(&StateMutex);
	FOverlay& Overlay = OverlayFor_NoLock(SessionId);

	int32 EnabledCount = 0;
	for (const auto& Pair : Overlay.ToolStates)
	{
		if (IsToolEnabled_NoLock(Overlay, Pair.Key)) ++EnabledCount;
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("totalTools"), Overlay.ToolStates.Num());
	Result->SetNumberField(TEXT("enabledTools"), EnabledCount);
	Result->SetNumberField(TEXT("disabledTools"), Overlay.ToolStates.Num() - EnabledCount);
	Result->SetNumberField(TEXT("catalogStateRevision"), static_cast<double>(Overlay.CatalogStateRevision));
	Result->SetNumberField(TEXT("limits"), Overlay.Limits.Num());
	Result->SetNumberField(TEXT("preferences"), Overlay.Preferences.Num());
	return Result;
}
