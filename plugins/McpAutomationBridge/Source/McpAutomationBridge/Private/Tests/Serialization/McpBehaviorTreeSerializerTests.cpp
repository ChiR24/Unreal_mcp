// B11 - Behavior Tree serializer regression tests.
//
// These run IN the editor process against synthetic BT node objects. They lock
// three contracts of McpBehaviorTreeSerializers::SerializeBTNode:
//   * R16: CollectBlackboardKeySelectors enumerates ALL FBlackboardKeySelector
//     props (the old implementation broke after the first selector), so a node
//     carrying two selectors must expose BOTH in keyProperties{}.
//   * R12: the max-depth guard emits a per-node serializationError instead of
//     recursing forever once Depth exceeds the file-local GMaxTreeDepth (64).
//   * R13: the cycle guard emits a per-node serializationError when a node is
//     visited twice through the shared Visited set.

#include "Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeSerializers.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/Decorators/BTDecorator_DoesPathExist.h"
#include "Misc/AutomationTest.h"

namespace
{
class FBTDecorator_DoesPathExist_TestAccess : public UBTDecorator_DoesPathExist
{
public:
	using UBTDecorator_DoesPathExist::BlackboardKeyA;
	using UBTDecorator_DoesPathExist::BlackboardKeyB;
};

TSharedPtr<FJsonObject> SerializeTwoSelectorNode()
{
	FBTDecorator_DoesPathExist_TestAccess* Node = NewObject<FBTDecorator_DoesPathExist_TestAccess>();
	Node->BlackboardKeyA.SelectedKeyName = FName(TEXT("LocationA"));
	Node->BlackboardKeyB.SelectedKeyName = FName(TEXT("LocationB"));

	TSet<const UBTNode*> Visited;
	int32 NodeCount = 0;
	int32 ExecNodeCount = 0;
	return McpBehaviorTreeSerializers::SerializeBTNode(
		Node, nullptr, nullptr, 0, Visited, NodeCount, ExecNodeCount);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpBehaviorTreeCollectsAllSelectorsTest,
	"McpAutomationBridge.BehaviorTree.CollectsAllBlackboardKeySelectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpBehaviorTreeCollectsAllSelectorsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TSharedPtr<FJsonObject> Obj = SerializeTwoSelectorNode();
	const TSharedPtr<FJsonValue>* KeyProps = Obj->Values.Find(TEXT("keyProperties"));
	TestTrue(TEXT("keyProperties emitted for a node with selectors"), KeyProps != nullptr);
	if (KeyProps)
	{
		const TSharedPtr<FJsonObject> Inner = (*KeyProps)->AsObject();
		TestTrue(TEXT("first selector collected"), Inner->HasField(TEXT("BlackboardKeyA")));
		TestTrue(TEXT("second selector collected"), Inner->HasField(TEXT("BlackboardKeyB")));
		TestTrue(TEXT("ALL selectors collected, not just the first"),
			Inner->Values.Num() == 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpBehaviorTreeMaxDepthTest,
	"McpAutomationBridge.BehaviorTree.MaxDepthExceeded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpBehaviorTreeMaxDepthTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UBTNode* Node = NewObject<UBTDecorator_DoesPathExist>();
	TSet<const UBTNode*> Visited;
	int32 NodeCount = 0;
	int32 ExecNodeCount = 0;
	// GMaxTreeDepth is 64 (file-local); Depth 65 exceeds it.
	const TSharedPtr<FJsonObject> Obj = McpBehaviorTreeSerializers::SerializeBTNode(
		Node, nullptr, nullptr, 65, Visited, NodeCount, ExecNodeCount);
	TestTrue(TEXT("max depth emits a serialization error"),
		Obj->GetStringField(TEXT("serializationError")) == TEXT("max depth exceeded"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpBehaviorTreeCycleTest,
	"McpAutomationBridge.BehaviorTree.CycleDetected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpBehaviorTreeCycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UBTNode* Node = NewObject<UBTDecorator_DoesPathExist>();
	TSet<const UBTNode*> Visited;
	Visited.Add(Node);
	int32 NodeCount = 0;
	int32 ExecNodeCount = 0;
	const TSharedPtr<FJsonObject> Obj = McpBehaviorTreeSerializers::SerializeBTNode(
		Node, nullptr, nullptr, 0, Visited, NodeCount, ExecNodeCount);
	TestTrue(TEXT("a revisited node emits a serialization error"),
		Obj->GetStringField(TEXT("serializationError")) == TEXT("cycle detected"));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
