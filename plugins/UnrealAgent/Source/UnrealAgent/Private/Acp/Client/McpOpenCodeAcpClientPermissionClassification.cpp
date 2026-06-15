#include "Acp/Client/McpOpenCodeAcpClientPermissionClassification.h"

namespace UnrealAgent::OpenCodeAcp
{
namespace
{
enum class EPermissionOptionSemantics : uint8
{
    Unknown,
    AllowOnce,
    AllowAlways,
    Reject
};

EPermissionOptionSemantics ClassifyPermissionOptionToken(const FString& Token)
{
    FString Normalized = Token.ToLower();
    Normalized.ReplaceInline(TEXT("-"), TEXT("_"));
    if (Normalized == TEXT("once")
        || Normalized == TEXT("allow")
        || Normalized == TEXT("allow_once"))
    {
        return EPermissionOptionSemantics::AllowOnce;
    }
    if (Normalized == TEXT("always") || Normalized == TEXT("allow_always"))
    {
        return EPermissionOptionSemantics::AllowAlways;
    }
    if (Normalized == TEXT("reject")
        || Normalized == TEXT("reject_once")
        || Normalized == TEXT("reject_always")
        || Normalized == TEXT("deny"))
    {
        return EPermissionOptionSemantics::Reject;
    }
    return EPermissionOptionSemantics::Unknown;
}

bool IsRejectOption(const FOpenCodeAcpPermissionOption& Option)
{
    if (!Option.Kind.IsEmpty())
    {
        return Option.Kind.Equals(TEXT("reject_once"), ESearchCase::IgnoreCase)
            || Option.Kind.Equals(TEXT("reject_always"), ESearchCase::IgnoreCase)
            || Option.Kind.Equals(TEXT("deny"), ESearchCase::IgnoreCase);
    }
    return Option.Id.Equals(TEXT("reject"), ESearchCase::IgnoreCase)
        || Option.Id.Equals(TEXT("reject-once"), ESearchCase::IgnoreCase)
        || Option.Id.Equals(TEXT("reject_once"), ESearchCase::IgnoreCase)
        || Option.Id.Equals(TEXT("deny"), ESearchCase::IgnoreCase);
}
}

bool HasConflictingPermissionOptionSemantics(const FOpenCodeAcpPermissionOption& Option)
{
    if (Option.Kind.IsEmpty())
    {
        return false;
    }
    const EPermissionOptionSemantics IdSemantics =
        ClassifyPermissionOptionToken(Option.Id);
    const EPermissionOptionSemantics KindSemantics =
        ClassifyPermissionOptionToken(Option.Kind);
    return IdSemantics != EPermissionOptionSemantics::Unknown
        && KindSemantics != EPermissionOptionSemantics::Unknown
        && IdSemantics != KindSemantics;
}

bool IsAllowAlwaysOption(const FOpenCodeAcpPermissionOption& Option)
{
    if (!Option.Kind.IsEmpty())
    {
        return Option.Kind.Equals(TEXT("allow_always"), ESearchCase::IgnoreCase);
    }
    return Option.Id.Equals(TEXT("always"), ESearchCase::IgnoreCase)
        || Option.Id.Equals(TEXT("allow-always"), ESearchCase::IgnoreCase)
        || Option.Id.Equals(TEXT("allow_always"), ESearchCase::IgnoreCase);
}

FString FindRejectOptionId(const TArray<FOpenCodeAcpPermissionOption>& Options)
{
    const FOpenCodeAcpPermissionOption* Match = Options.FindByPredicate([](const FOpenCodeAcpPermissionOption& Option)
    {
        return IsRejectOption(Option);
    });
    return Match != nullptr ? Match->Id : FString();
}

bool LooksLikeUnrealEditorPermission(const FString& Description)
{
    const FString Lower = Description.ToLower();
    const TCHAR* Markers[] = {
        TEXT("unreal-engine"), TEXT("unreal editor"), TEXT("mcpautomationbridge"),
        TEXT("/game"), TEXT("/engine"), TEXT("/script"), TEXT(".uasset"),
        TEXT(".umap"), TEXT(".uproject"), TEXT("set_project_setting"),
        TEXT("content/"), TEXT("blueprint"), TEXT("viewport"), TEXT("play in editor"),
        TEXT("manage_tools"), TEXT("inspect"), TEXT("manage_asset"),
        TEXT("manage_blueprint"), TEXT("manage_audio"), TEXT("manage_effect"),
        TEXT("manage_sequence"), TEXT("manage_geometry"), TEXT("manage_pcg"),
        TEXT("build_environment"), TEXT("manage_level"),
        TEXT("manage_level_structure"), TEXT("manage_networking"),
        TEXT("manage_inventory"), TEXT("manage_interaction"),
        TEXT("manage_character"), TEXT("manage_combat"), TEXT("manage_ai"),
        TEXT("manage_gas"), TEXT("animation_physics"), TEXT("control_editor"),
        TEXT("control_actor"), TEXT("system_control")
    };
    for (const TCHAR* Marker : Markers)
    {
        if (Lower.Contains(Marker))
        {
            return true;
        }
    }
    return false;
}
}
