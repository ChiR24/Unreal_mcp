#include "Domains/SystemControl/McpSystemPrivate.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/OutputDevice.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
// EMaterialDomain's own header exists only on the engines that split it out; 5.0 declares it in MaterialShared.h.
#if __has_include("MaterialDomain.h")
#include "MaterialDomain.h"
#endif
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

// validate_assets materialUsage: a usage flag a material lacks is set by the editor when it draws the mesh outside
// play, and saved only if the material is saved then. A mesh that appears only in play (spawned by a Blueprint) draws
// the default material in play and in a cooked game, with only a log warning.
namespace McpSystemControlHandlers {
namespace {
struct FMcpUsageNeed {
    EMaterialUsage Usage;
    const TCHAR *Name;
    const TCHAR *Property;
};
const FMcpUsageNeed McpNaniteNeed{MATUSAGE_Nanite, TEXT("Nanite"), TEXT("bUsedWithNanite")};
const FMcpUsageNeed McpInstancedNeed{MATUSAGE_InstancedStaticMeshes, TEXT("InstancedStaticMeshes"),
                                     TEXT("bUsedWithInstancedStaticMeshes")};
const FMcpUsageNeed McpSplineNeed{MATUSAGE_SplineMesh, TEXT("SplineMesh"), TEXT("bUsedWithSplineMeshes")};
const FMcpUsageNeed McpSkeletalNeed{MATUSAGE_SkeletalMesh, TEXT("SkeletalMesh"), TEXT("bUsedWithSkeletalMesh")};

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
// 5.8 gave material instances flags of their own, and sets a missing one without the engine's record of usage flags
// awaiting a save (UMaterial::IsUsageFlagDirty); its log line is the only notice. "<instance path>|<usage>" to when.
FCriticalSection GMcpInstanceUsageMutex;
TMap<FString, FDateTime> GMcpInstanceUsage;

class FMcpUsageLogWatch final : public FOutputDevice
{
public:
    void Serialize(const TCHAR *V, ELogVerbosity::Type, const FName &Category) override
    {
        static const FName MaterialLogCategory(TEXT("LogMaterial"));
        const TCHAR *Marker = TEXT(" needed to set usage flag ");
        const TCHAR *At = Category == MaterialLogCategory && FCString::Strncmp(V, TEXT("Material "), 9) == 0 ? FCString::Strstr(V, Marker) : nullptr;
        for (int32 Usage = 0; At && Usage < MATUSAGE_MAX; ++Usage)
        {
            if (UMaterialInterface::GetUsageName(EMaterialUsage(Usage)) == At + FCString::Strlen(Marker))
            {
                // A file time is whole seconds, so a save in the same second as the flag counts as after it.
                const FDateTime Now = FDateTime::UtcNow();
                FScopeLock Lock(&GMcpInstanceUsageMutex);
                GMcpInstanceUsage.Add(FString::ConstructFromPtrSize(V + 9, int32(At - V) - 9) + TEXT("|") + FString::FromInt(Usage),
                                      FDateTime(Now.GetYear(), Now.GetMonth(), Now.GetDay(), Now.GetHour(), Now.GetMinute(), Now.GetSecond()));
                return;
            }
        }
    }
    bool CanBeUsedOnAnyThread() const override { return true; }
    bool CanBeUsedOnMultipleThreads() const override { return true; }
};
FMcpUsageLogWatch GMcpUsageLogWatch;
bool bMcpWatchingUsage = false;
#endif

// Whether Owner holds a flag its asset file lacks. The engine keeps a base material's flag marked as awaiting a save
// until the material is saved, even when it could not mark the package dirty (while a mesh loads or builds); an
// unsaved package may hold a flag set by hand.
bool McpUsagePending(UMaterialInterface *Owner, EMaterialUsage Usage)
{
    if (Owner->GetOutermost()->IsDirty())
    {
        return true;
    }
    if (UMaterial *Material = Cast<UMaterial>(Owner))
    {
        return Material->IsUsageFlagDirty(Usage);
    }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    FDateTime Given;
    {
        FScopeLock Lock(&GMcpInstanceUsageMutex);
        const FDateTime *Found = GMcpInstanceUsage.Find(Owner->GetPathName() + TEXT("|") + FString::FromInt(Usage));
        if (!Found)
        {
            return false;
        }
        Given = *Found;
    }
    FString File;
    return !FPackageName::TryConvertLongPackageNameToFilename(Owner->GetOutermost()->GetName(), File,
                                                              FPackageName::GetAssetPackageExtension()) ||
           IFileManager::Get().GetTimeStamp(*FPaths::ConvertRelativePathToFull(File)) < Given;
#else
    return false;
#endif
}

bool McpUsesNanite(const UStaticMesh *Mesh)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    return Mesh && Mesh->IsNaniteEnabled();
#else
    return Mesh && Mesh->NaniteSettings.bEnabled;
#endif
}

void McpCheckUsage(UMaterialInterface *Interface, const FMcpUsageNeed &Need, const FString &Component,
                   TArray<TSharedPtr<FJsonValue>> &Out)
{
    UMaterial *Base = Interface ? Interface->GetMaterial() : nullptr;
    // Only surface materials take mesh usages; the engine's own fallbacks carry every usage.
    if (!Base || Base->MaterialDomain != MD_Surface || Base->bUsedAsSpecialEngineMaterial)
    {
        return;
    }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    // 5.8 gave material instances flags of their own: the editor sets a missing one on the instance, which then
    // compiles its own shaders. Earlier engines set it on the base material.
    UMaterialInterface *Owner = Interface->IsA<UMaterialInstanceDynamic>() ? Base : Interface;
    const bool bHas = Owner->GetUsageByFlag(Need.Usage);
#else
    UMaterialInterface *Owner = Base;
    const bool bHas = Base->GetUsageByFlag(Need.Usage);
#endif
    // A set flag is on disk only once its asset is saved; an instance also lacks what its base material has not saved.
    const UObject *Unsaved = !bHas                                 ? nullptr
                             : McpUsagePending(Owner, Need.Usage) ? static_cast<UObject *>(Owner)
                             : McpUsagePending(Base, Need.Usage)  ? Base
                                                                  : nullptr;
    if (bHas && !Unsaved)
    {
        return;
    }
    // A missing flag is set on the base material, which its instances inherit; an unsaved one is saved where it is.
    const FString Material = Unsaved ? Unsaved->GetPathName() : Base->GetPathName();
    for (const TSharedPtr<FJsonValue> &Seen : Out)
    {
        const TSharedPtr<FJsonObject> &Row = Seen->AsObject();
        if (Row->GetStringField(TEXT("material")) == Material && Row->GetStringField(TEXT("usage")) == Need.Name)
        {
            return;
        }
    }
    const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("material"), Material);
    Row->SetStringField(TEXT("usage"), Need.Name);
    Row->SetStringField(TEXT("property"), Need.Property);
    if (Unsaved)
    {
        Row->SetBoolField(TEXT("unsaved"), true);
    }
    if (!Component.IsEmpty())
    {
        Row->SetStringField(TEXT("component"), Component);
    }
    Out.Add(MakeShared<FJsonValueObject>(Row));
}

void McpCheckComponent(const UPrimitiveComponent *Part, const FString &Name, TArray<TSharedPtr<FJsonValue>> &Out)
{
    TArray<const FMcpUsageNeed *> Needs;
    if (const UStaticMeshComponent *Mesh = Cast<UStaticMeshComponent>(Part))
    {
        if (McpUsesNanite(Mesh->GetStaticMesh()))
        {
            Needs.Add(&McpNaniteNeed);
        }
        if (Part->IsA<UInstancedStaticMeshComponent>())
        {
            Needs.Add(&McpInstancedNeed);
        }
        if (Part->IsA<USplineMeshComponent>())
        {
            Needs.Add(&McpSplineNeed);
        }
    }
    else if (Part->IsA<USkinnedMeshComponent>())
    {
        Needs.Add(&McpSkeletalNeed);
    }
    for (int32 Slot = 0; Needs.Num() > 0 && Slot < Part->GetNumMaterials(); ++Slot)
    {
        for (const FMcpUsageNeed *Need : Needs)
        {
            McpCheckUsage(Part->GetMaterial(Slot), *Need, Name, Out);
        }
    }
}
} // namespace

void McpWatchMaterialUsage(bool bWatch)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    if (GLog && bWatch != bMcpWatchingUsage)
    {
        bWatch ? GLog->AddOutputDevice(&GMcpUsageLogWatch) : GLog->RemoveOutputDevice(&GMcpUsageLogWatch);
        bMcpWatchingUsage = bWatch;
    }
#endif
}

TArray<TSharedPtr<FJsonValue>> McpMissingMaterialUsage(UObject *Asset)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    if (const UStaticMesh *Mesh = Cast<UStaticMesh>(Asset))
    {
        for (int32 Slot = 0; McpUsesNanite(Mesh) && Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
        {
            McpCheckUsage(Mesh->GetStaticMaterials()[Slot].MaterialInterface, McpNaniteNeed, FString(), Out);
        }
    }
    else if (const USkeletalMesh *Skeletal = Cast<USkeletalMesh>(Asset))
    {
        for (const FSkeletalMaterial &Slot : Skeletal->GetMaterials())
        {
            McpCheckUsage(Slot.MaterialInterface, McpSkeletalNeed, FString(), Out);
        }
    }
    else if (const UBlueprint *Blueprint = Cast<UBlueprint>(Asset))
    {
        // The parent's own components (a Character's mesh) live on the class default object, the Blueprint's in
        // its construction script.
        if (const AActor *Defaults = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr)
        {
            TInlineComponentArray<UPrimitiveComponent *> Parts(Defaults);
            for (const UPrimitiveComponent *Part : Parts)
            {
                McpCheckComponent(Part, Part->GetName(), Out);
            }
        }
        if (const USimpleConstructionScript *Script = Blueprint->SimpleConstructionScript)
        {
            for (const USCS_Node *Node : Script->GetAllNodes())
            {
                // The template object carries a _GEN_VARIABLE suffix; the node's variable is the name the editor shows.
                if (const UPrimitiveComponent *Part = Node ? Cast<UPrimitiveComponent>(Node->ComponentTemplate) : nullptr)
                {
                    McpCheckComponent(Part, Node->GetVariableName().ToString(), Out);
                }
            }
        }
    }
    return Out;
}
} // namespace McpSystemControlHandlers
