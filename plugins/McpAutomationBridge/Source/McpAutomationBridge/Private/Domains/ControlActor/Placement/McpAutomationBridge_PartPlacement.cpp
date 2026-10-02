#include "Domains/ControlActor/Placement/McpAutomationBridge_PartPlacement.h"

#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "PreviewScene.h"
#include "Spatial/FastWinding.h"

namespace McpPartPlacement
{
namespace
{
using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FDynamicMeshAABBTree3;
using FWindingTree = UE::Geometry::TFastWindingTree<FDynamicMesh3>;

const FName AcceptedTag(TEXT("mcp.placement.ok"));
// mcp.placement.ok:<part> accepts one embed only (a neck in its collar, a hand round a handle), so the part is still
// checked against everything else.
const FString AcceptedPairPrefix(TEXT("mcp.placement.ok:"));
// Surface samples per part: enough to find a buried region a few cm across on a character-sized
// part, few enough that a 25-part rig answers in well under a second.
constexpr int32 MaxSamples = 3000;

// One mesh asset measured once per audit, in its own local space: the tree answers "nearest
// surface point", the winding tree "is this point inside", which a ray parity test gets wrong on
// the open or self-touching meshes a sculpting pass leaves behind.
struct FPartShape
{
    FDynamicMesh3 Mesh;
    TUniquePtr<FDynamicMeshAABBTree3> Tree;
    TUniquePtr<FWindingTree> Winding;
};

struct FPart
{
    FString Name;
    FTransform Transform;
    FBox Box = FBox(ForceInit);
    const FPartShape* Shape = nullptr;
    TArray<FVector> Samples;
    TSet<FString> Accepted;
};

TSharedPtr<FPartShape> BuildShape(UStaticMesh* Mesh)
{
    // The source mesh, not the render data: a Nanite mesh's render LOD 0 is its coarse fallback
    // (334 triangles for a 40k-triangle body), which would place surfaces a centimetre off.
    const FMeshDescription* Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
    if (!Description)
    {
        return nullptr;
    }
    TSharedPtr<FPartShape> Shape = MakeShared<FPartShape>();
    FMeshDescriptionToDynamicMesh Converter;
    Converter.Convert(Description, Shape->Mesh);
    if (Shape->Mesh.TriangleCount() == 0)
    {
        return nullptr;
    }
    Shape->Tree = MakeUnique<FDynamicMeshAABBTree3>(&Shape->Mesh, true);
    Shape->Winding = MakeUnique<FWindingTree>(Shape->Tree.Get(), true);
    return Shape;
}

// Parts worth measuring: every one for a full audit; for an edit, the parts it touched and those
// whose bounds reach them, so a one-component edit on a 25-part rig converts a handful of meshes.
TArray<UStaticMeshComponent*> RelevantParts(AActor* Actor, const TSet<FString>& Focus)
{
    TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
    TArray<UStaticMeshComponent*> Usable;
    for (UStaticMeshComponent* Component : Components)
    {
        if (Component && Component->GetStaticMesh() && Component->IsVisible() &&
            !Component->IsA<UInstancedStaticMeshComponent>() && !Component->ComponentHasTag(AcceptedTag))
        {
            Usable.Add(Component);
        }
    }
    if (Focus.Num() == 0)
    {
        return Usable;
    }
    TArray<FBox> FocusBoxes;
    for (UStaticMeshComponent* Component : Usable)
    {
        if (Focus.Contains(Component->GetName()))
        {
            FocusBoxes.Add(Component->Bounds.GetBox());
        }
    }
    return Usable.FilterByPredicate([&FocusBoxes](UStaticMeshComponent* Component) {
        const FBox Box = Component->Bounds.GetBox();
        return FocusBoxes.ContainsByPredicate([&Box](const FBox& Other) { return Other.Intersect(Box); });
    });
}

void CollectParts(AActor* Actor, const TSet<FString>& Focus, TMap<UStaticMesh*, TSharedPtr<FPartShape>>& Shapes,
                  TArray<FPart>& Parts)
{
    for (UStaticMeshComponent* Component : RelevantParts(Actor, Focus))
    {
        UStaticMesh* Mesh = Component->GetStaticMesh();
        TSharedPtr<FPartShape>& Shape = Shapes.FindOrAdd(Mesh);
        if (!Shape.IsValid()) Shape = BuildShape(Mesh);
        if (!Shape.IsValid()) continue;
        FPart& Part = Parts.AddDefaulted_GetRef();
        Part.Name = Component->GetName();
        Part.Transform = Component->GetComponentTransform();
        Part.Shape = Shape.Get();
        for (const FName& Tag : Component->ComponentTags)
        {
            if (Tag.ToString().StartsWith(AcceptedPairPrefix)) Part.Accepted.Add(Tag.ToString().Mid(AcceptedPairPrefix.Len()));
        }
        const int32 Stride = FMath::Max(1, Shape->Mesh.VertexCount() / MaxSamples);
        int32 Seen = 0;
        for (const int32 Vid : Shape->Mesh.VertexIndicesItr())
        {
            const FVector World = Part.Transform.TransformPosition(FVector(Shape->Mesh.GetVertex(Vid)));
            Part.Box += World;
            if (Seen++ % Stride == 0)
            {
                Part.Samples.Add(World);
            }
        }
    }
}

// How deep A's surface goes inside B, and what share of A's samples is inside B.
void MeasureInside(const FPart& A, const FPart& B, double& OutDepth, double& OutShare)
{
    OutDepth = 0.0;
    int32 Inside = 0;
    for (const FVector& Point : A.Samples)
    {
        if (!B.Box.IsInsideOrOn(Point)) continue;
        const FVector3d Local(B.Transform.InverseTransformPosition(Point));
        if (!B.Shape->Winding->IsInside(Local)) continue;
        ++Inside;
        const FVector Nearest = B.Transform.TransformPosition(FVector(B.Shape->Tree->FindNearestPoint(Local)));
        OutDepth = FMath::Max(OutDepth, FVector::Dist(Point, Nearest));
    }
    OutShare = A.Samples.Num() > 0 ? static_cast<double>(Inside) / A.Samples.Num() : 0.0;
}

void FindBuried(const TArray<FPart>& Parts, const TSet<FString>& Focus, double Tolerance, TArray<FPartIssue>& Out)
{
    for (int32 I = 0; I < Parts.Num(); ++I)
    {
        for (int32 J = I + 1; J < Parts.Num(); ++J)
        {
            const FPart& A = Parts[I];
            const FPart& B = Parts[J];
            if ((Focus.Num() > 0 && !Focus.Contains(A.Name) && !Focus.Contains(B.Name)) ||
                A.Accepted.Contains(B.Name) || B.Accepted.Contains(A.Name) || !A.Box.ExpandBy(-Tolerance).Intersect(B.Box))
            {
                continue;
            }
            double DepthAB = 0.0, ShareAB = 0.0, DepthBA = 0.0, ShareBA = 0.0;
            MeasureInside(A, B, DepthAB, ShareAB);
            MeasureInside(B, A, DepthBA, ShareBA);
            // One finding per pair: the part with more of its surface inside the other is the one that sank
            // (a leg's hip ball 15% inside a body, not the body 1% inside the ball at the same depth).
            const bool bAInB = DepthAB > Tolerance && (DepthBA <= Tolerance || ShareAB >= ShareBA);
            const double Depth = bAInB ? DepthAB : DepthBA;
            if (Depth <= Tolerance)
            {
                continue;
            }
            FPartIssue& Issue = Out.AddDefaulted_GetRef();
            Issue.Kind = TEXT("buried");
            Issue.Component = bAInB ? A.Name : B.Name;
            Issue.Other = bAInB ? B.Name : A.Name;
            Issue.Depth = Depth;
            Issue.InsideShare = bAInB ? ShareAB : ShareBA;
            Issue.Issue = FString::Printf(
                TEXT("'%s' is buried %.1f units inside '%s' (%.0f%% of its surface is inside). Move it out "
                     "until the two only touch, or tag the part mcp.placement.ok if the embedding is meant."),
                *Issue.Component, Depth, *Issue.Other, Issue.InsideShare * 100.0);
        }
    }
}

void FindSunk(const TArray<FPart>& Parts, const TSet<FString>& Focus, const FPartAudit& Audit,
              double Tolerance, TArray<FPartIssue>& Out)
{
    if (!Audit.bHasGround)
    {
        return;
    }
    for (const FPart& Part : Parts)
    {
        const double Depth = Audit.GroundZ - Part.Box.Min.Z;
        if ((Focus.Num() > 0 && !Focus.Contains(Part.Name)) || Depth <= Tolerance)
        {
            continue;
        }
        FPartIssue& Issue = Out.AddDefaulted_GetRef();
        Issue.Kind = TEXT("sunk");
        Issue.Component = Part.Name;
        Issue.Depth = Depth;
        Issue.Issue = FString::Printf(
            TEXT("'%s' reaches %.1f units below the actor's ground (the bottom of its capsule), so it sinks "
                 "into whatever the actor stands on. Raise it by that much."),
            *Part.Name, Depth);
    }
}

// A preview world, as the Blueprint editor uses: nothing is added to the level or its undo history.
AActor* SpawnPreview(FPreviewScene& Scene, UBlueprint* Blueprint, FString& OutError)
{
    UClass* Class = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
    if (!Class || !Class->IsChildOf(AActor::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
    {
        OutError = TEXT("Only an actor Blueprint that compiles to a spawnable class has parts to measure.");
        return nullptr;
    }
    UWorld* World = Scene.GetWorld();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags = RF_Transient;
    AActor* Actor = World ? World->SpawnActor<AActor>(Class, FTransform::Identity, Params) : nullptr;
    if (!Actor)
    {
        OutError = TEXT("The Blueprint's class could not be spawned in a preview world.");
    }
    return Actor;
}

void AuditActor(AActor* Actor, const TSet<FString>& Focus, double Tolerance, FPartAudit& Audit)
{
    if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Actor->GetRootComponent()))
    {
        Audit.bHasGround = true;
        Audit.GroundZ = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
    }
    TMap<UStaticMesh*, TSharedPtr<FPartShape>> Shapes;
    TArray<FPart> Parts;
    CollectParts(Actor, Focus, Shapes, Parts);
    Audit.Examined = Parts.Num();
    FindSunk(Parts, Focus, Audit, Tolerance, Audit.Issues);
    FindBuried(Parts, Focus, Tolerance, Audit.Issues);
    Audit.Issues.Sort([](const FPartIssue& L, const FPartIssue& R) { return L.Depth > R.Depth; });
}
} // namespace

FPartAudit AuditBlueprintParts(UBlueprint* Blueprint, const TSet<FString>& Focus, double Tolerance)
{
    FPartAudit Audit;
    FPreviewScene Scene;
    if (AActor* Actor = SpawnPreview(Scene, Blueprint, Audit.Error))
    {
        AuditActor(Actor, Focus, Tolerance, Audit);
        Actor->Destroy();
    }
    return Audit;
}

FPartAudit AuditBlueprintPartsUsingMesh(UBlueprint* Blueprint, const UStaticMesh* Mesh, double Tolerance)
{
    FPartAudit Audit;
    FPreviewScene Scene;
    AActor* Actor = SpawnPreview(Scene, Blueprint, Audit.Error);
    if (!Actor)
    {
        return Audit;
    }
    TSet<FString> Focus;
    TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
    for (const UStaticMeshComponent* Component : Components)
    {
        if (Component && Component->GetStaticMesh() == Mesh)
        {
            Focus.Add(Component->GetName());
        }
    }
    if (Focus.Num() > 0)
    {
        AuditActor(Actor, Focus, Tolerance, Audit);
    }
    Actor->Destroy();
    return Audit;
}

} // namespace McpPartPlacement
