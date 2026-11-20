// McpConsentWindows.cpp — see header.
#include "Core/Security/McpConsentWindows.h"

#include "EngineWireSettings.h"
#include "Foundation/McpCapabilityAuthorization.h"

#include "Async/Async.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace
{
// What the gate's CheckPaths takes as canonical: rooted, and no traversal, drive colon or backslash left in it.
bool IsCanonical(const FString& Path)
{
    return Path.StartsWith(TEXT("/")) && !Path.Contains(TEXT("..")) && !Path.Contains(TEXT(":")) && !Path.Contains(TEXT("\\"));
}

// Windows on, explicit consent, no Admin anywhere in the demand. Elevated consent (run_script, execute_python) and
// Admin capabilities stay one grant per call.
bool Windowable(const FMcpCapabilityDemand& Demand)
{
    const UEngineWireSettings* Settings = GetDefault<UEngineWireSettings>();
    return Settings && Settings->bAllowConsentWindows && Demand.ConsentMode == TEXT("explicit") &&
           Demand.RequiredScope != EMcpCapabilityScope::Admin && !Demand.AlsoRequiredScopes.Contains(EMcpCapabilityScope::Admin);
}

// ponytail: a window checks the paths a call names. A capability that writes to a default folder when its path is
// left out is covered by what it does name; per-key shapes would close that, if a capability needs it.
bool StrictPaths(const TSharedPtr<FJsonObject>& Payload, TArray<FString>& OutPaths)
{
    const FMcpPayloadPathScan Scan = McpCapabilityAuthorization::CollectPayloadPaths(Payload, true);
    OutPaths = Scan.Paths;
    return !Scan.bTruncated && Scan.Paths.Num() > 0;
}

// The user's Allow or Deny for a pending window, on a notification that does not take focus.
void AskInEditor(const FString& Id, const FString& Text)
{
    AsyncTask(ENamedThreads::GameThread, [Id, Text]()
    {
        if (!FSlateApplication::IsInitialized())
        {
            FMcpConsentWindows::Get().Settle(Id, false);
            return;
        }
        const TSharedRef<TWeakPtr<SNotificationItem>> Item = MakeShared<TWeakPtr<SNotificationItem>>();
        const auto Answer = [Id, Item](bool bAllow)
        {
            FMcpConsentWindows::Get().Settle(Id, bAllow);
            if (const TSharedPtr<SNotificationItem> Shown = Item->Pin())
            {
                Shown->SetCompletionState(bAllow ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
                Shown->ExpireAndFadeout();
            }
        };
        FNotificationInfo Info(FText::FromString(Text));
        Info.bFireAndForget = false;
        Info.ButtonDetails.Add(FNotificationButtonInfo(FText::FromString(TEXT("Allow")), FText::GetEmpty(),
            FSimpleDelegate::CreateLambda([Answer]() { Answer(true); }), SNotificationItem::CS_Pending));
        Info.ButtonDetails.Add(FNotificationButtonInfo(FText::FromString(TEXT("Deny")), FText::GetEmpty(),
            FSimpleDelegate::CreateLambda([Answer]() { Answer(false); }), SNotificationItem::CS_Pending));
        *Item = FSlateNotificationManager::Get().AddNotification(Info);
        if (const TSharedPtr<SNotificationItem> Shown = Item->Pin())
        {
            Shown->SetCompletionState(SNotificationItem::CS_Pending);
        }
    });
}
} // namespace

FMcpConsentWindows& FMcpConsentWindows::Get()
{
    static FMcpConsentWindows Instance;
    return Instance;
}

FString FMcpConsentWindows::FolderOf(const TArray<FString>& Paths)
{
    if (Paths.Num() == 0 || Paths.ContainsByPredicate([](const FString& Path) { return !IsCanonical(Path); }))
    {
        return FString();
    }
    // The folder of the first path, shortened until it holds every other one.
    FString Folder = Paths[0];
    int32 Slash = INDEX_NONE;
    Folder = Folder.FindLastChar(TEXT('/'), Slash) ? Folder.Left(Slash + 1) : FString();
    for (const FString& Path : Paths)
    {
        while (!Folder.IsEmpty() && !Path.StartsWith(Folder, ESearchCase::IgnoreCase))
        {
            Folder.LeftChopInline(1);
            Folder = Folder.FindLastChar(TEXT('/'), Slash) ? Folder.Left(Slash + 1) : FString();
        }
    }
    // A content root itself (/Game/, /Engine/) covers a whole project: never a window's folder.
    int32 Separators = 0;
    for (const TCHAR Char : Folder)
    {
        Separators += Char == TEXT('/') ? 1 : 0;
    }
    return Separators >= 3 ? Folder : FString();
}

FString FMcpConsentWindows::OfferFor(const FMcpCapabilityDemand& Demand, const TSharedPtr<FJsonObject>& Arguments)
{
    TArray<FString> Paths;
    return Windowable(Demand) && StrictPaths(Arguments, Paths) ? FolderOf(Paths) : FString();
}

FString FMcpConsentWindows::Open(const FString& Identity, const FString& CapabilityId, const FString& Prefix, int32 Uses,
                                 int32 Minutes, bool bPending)
{
    FMcpConsentWindow Window;
    Window.Id = FGuid::NewGuid().ToString(EGuidFormats::Short);
    Window.Identity = Identity;
    Window.CapabilityId = CapabilityId;
    Window.Prefix = Prefix;
    Window.UsesLeft = FMath::Max(Uses, 1);
    Window.ExpiresAtSeconds = FPlatformTime::Seconds() + 60.0 * FMath::Max(Minutes, 1);
    Window.bPending = bPending;
    FScopeLock Lock(&Mutex);
    DropDeadLocked();
    Windows.Add(Window);
    return Window.Id;
}

void FMcpConsentWindows::OpenFromForm(const FString& Identity, const FString& CapabilityId, const FString& Prefix)
{
    const UEngineWireSettings* Settings = GetDefault<UEngineWireSettings>();
    if (!Settings || !Settings->bAllowConsentWindows || Prefix.IsEmpty())
    {
        return;
    }
    const int32 Uses = FMath::Clamp(Settings->ConsentWindowMaxCalls, 1, 50);
    const int32 Minutes = FMath::Clamp(Settings->ConsentWindowMinutes, 1, 30);
    const bool bConfirm = Settings->bConfirmConsentWindowsInEditor;
    const FString Id = Open(Identity, CapabilityId, Prefix, Uses, Minutes, bConfirm);
    if (bConfirm)
    {
        AskInEditor(Id, FString::Printf(TEXT("An MCP client asks to run %s under %s without asking, for its next %d calls "
            "or %d minutes. Allow it?"), *CapabilityId, *Prefix, Uses, Minutes));
    }
}

void FMcpConsentWindows::Settle(const FString& Id, bool bAllow)
{
    FScopeLock Lock(&Mutex);
    for (int32 Index = Windows.Num() - 1; Index >= 0; --Index)
    {
        if (Windows[Index].Id == Id)
        {
            if (bAllow) Windows[Index].bPending = false;
            else Windows.RemoveAt(Index);
        }
    }
}

FMcpConsentWindowUse FMcpConsentWindows::Take(const FString& Identity, const FMcpCapabilityDemand& Demand, const TArray<FString>& Paths)
{
    FMcpConsentWindowUse Use;
    if (Identity.IsEmpty() || Paths.Num() == 0)
    {
        return Use;
    }
    FScopeLock Lock(&Mutex);
    DropDeadLocked();
    for (FMcpConsentWindow& Window : Windows)
    {
        if (Window.bPending || Window.Identity != Identity || !Demand.AcceptsConsentName(Window.CapabilityId))
        {
            continue;
        }
        const bool bInside = !Paths.ContainsByPredicate([&Window](const FString& Path)
        {
            return !IsCanonical(Path) || !McpCapabilityAuthorization::IsPathWithinPrefix(Path, Window.Prefix);
        });
        if (bInside)
        {
            Use.UsesLeft = --Window.UsesLeft;
            Use.Id = Window.Id;
            return Use;
        }
    }
    return Use;
}

FMcpConsentWindowUse FMcpConsentWindows::TakeFor(const FMcpCapabilityPrincipal& Principal, const FMcpCapabilityDemand& Demand,
                                                 const TSharedPtr<FJsonObject>& Payload, bool bNativeDoor)
{
    TArray<FString> Paths;
    return bNativeDoor && Windowable(Demand) && StrictPaths(Payload, Paths) ? Take(Principal.Identity, Demand, Paths)
                                                                             : FMcpConsentWindowUse();
}

int32 FMcpConsentWindows::Revoke(const FString& Id)
{
    FScopeLock Lock(&Mutex);
    return Windows.RemoveAll([&Id](const FMcpConsentWindow& Window) { return Id.IsEmpty() || Window.Id == Id; });
}

TArray<FMcpConsentWindow> FMcpConsentWindows::List()
{
    FScopeLock Lock(&Mutex);
    DropDeadLocked();
    return Windows;
}

void FMcpConsentWindows::DropDeadLocked()
{
    const double Now = FPlatformTime::Seconds();
    Windows.RemoveAll([Now](const FMcpConsentWindow& Window) { return Window.UsesLeft <= 0 || Window.ExpiresAtSeconds <= Now; });
}

TSharedPtr<FJsonObject> McpConsentWindowsReply(const FString& Action, const TSharedPtr<FJsonObject>& Args)
{
    const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    if (Action == TEXT("revoke_consent_window"))
    {
        FString Id;
        if (Args.IsValid()) Args->TryGetStringField(TEXT("id"), Id);
        const int32 Closed = FMcpConsentWindows::Get().Revoke(Id);
        if (!Id.IsEmpty() && Closed == 0)
        {
            Out->SetBoolField(TEXT("success"), false);
            Out->SetStringField(TEXT("errorCode"), TEXT("WINDOW_NOT_FOUND"));
            Out->SetStringField(TEXT("error"), FString::Printf(TEXT("No consent window %s is open (list_consent_windows lists them)."), *Id));
            return Out;
        }
        Out->SetBoolField(TEXT("success"), true);
        Out->SetNumberField(TEXT("closed"), Closed);
        Out->SetStringField(TEXT("message"), FString::Printf(TEXT("%d consent window(s) closed."), Closed));
        return Out;
    }
    const double Now = FPlatformTime::Seconds();
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FMcpConsentWindow& Window : FMcpConsentWindows::Get().List())
    {
        const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("id"), Window.Id);
        Row->SetStringField(TEXT("capability"), Window.CapabilityId);
        Row->SetStringField(TEXT("folder"), Window.Prefix);
        Row->SetNumberField(TEXT("usesLeft"), Window.UsesLeft);
        Row->SetNumberField(TEXT("secondsLeft"), FMath::Max(0, FMath::FloorToInt(Window.ExpiresAtSeconds - Now)));
        Row->SetBoolField(TEXT("pending"), Window.bPending);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetBoolField(TEXT("success"), true);
    Out->SetArrayField(TEXT("windows"), Rows);
    Out->SetStringField(TEXT("message"), FString::Printf(TEXT("%d consent window(s) open."), Rows.Num()));
    return Out;
}
