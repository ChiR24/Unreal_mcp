#include "Transport/Connection/McpConnectionManagerPrivate.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

#include "Misc/PackageName.h"

TArray<TSharedPtr<FJsonValue>> McpBuildContentRootValues()
{
	// The editor's mounted content roots ("/Game", "/Engine", "/ShooterCore", ...),
	// as bridge_ack and content_roots_changed report them.
	TArray<FString> Roots;
	FPackageName::QueryRootContentPaths(Roots, /*bIncludeReadOnlyRoots*/ false,
		/*bWithoutLeadingSlashes*/ false, /*bWithoutTrailingSlashes*/ true);
	Roots.Sort();

	return McpHandlerUtils::ToJsonStringArray(Roots);
}

void FMcpConnectionManager::SubscribeContentRootChanges()
{
	if (!ContentPathMountedHandle.IsValid())
	{
		ContentPathMountedHandle = FPackageName::OnContentPathMounted().AddSP(
			this, &FMcpConnectionManager::HandleContentPathChanged);
	}
	if (!ContentPathDismountedHandle.IsValid())
	{
		ContentPathDismountedHandle = FPackageName::OnContentPathDismounted().AddSP(
			this, &FMcpConnectionManager::HandleContentPathChanged);
	}
}

void FMcpConnectionManager::UnsubscribeContentRootChanges()
{
	FPackageName::OnContentPathMounted().Remove(ContentPathMountedHandle);
	FPackageName::OnContentPathDismounted().Remove(ContentPathDismountedHandle);
	ContentPathMountedHandle.Reset();
	ContentPathDismountedHandle.Reset();
	bContentRootsDirty.store(false);
}

void FMcpConnectionManager::HandleContentPathChanged(const FString& RootPath, const FString& ContentPath)
{
	// Fires on whichever thread mounts, possibly inside the engine's mount
	// bookkeeping. Only mark the snapshot stale; the ticker takes it.
	(void)RootPath;
	(void)ContentPath;
	bContentRootsDirty.store(true);
}

bool FMcpConnectionManager::HasPendingContentRootsUpdate() const
{
	return bContentRootsDirty.load();
}

void FMcpConnectionManager::FlushContentRootsUpdate()
{
	if (!bContentRootsDirty.exchange(false))
	{
		return;
	}

	TArray<TSharedPtr<FMcpBridgeWebSocket>> Targets;
	{
		FScopeLock Lock(&AuthSocketsMutex);
		for (const TSharedPtr<FMcpBridgeWebSocket>& Sock : ActiveSockets)
		{
			if (Sock.IsValid() && Sock->IsConnected() && AuthenticatedSockets.Contains(Sock.Get()))
			{
				Targets.Add(Sock);
			}
		}
	}
	if (Targets.Num() == 0)
	{
		return;
	}

	// One snapshot per ticker pass, however many mounts came and went since the last.
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetArrayField(TEXT("contentRoots"), McpBuildContentRootValues());
	McpSendAutomationEvent(TEXT("content_roots_changed"), Payload, Targets);
}
