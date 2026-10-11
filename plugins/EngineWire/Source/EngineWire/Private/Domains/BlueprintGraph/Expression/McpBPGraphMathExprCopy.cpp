#include "Domains/BlueprintGraph/Expression/McpBPGraphMathExpr.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EngineWireSubsystem.h"
#include "K2Node_MathExpression.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

// A Math Expression stores each member variable it reads as that variable's {GUID}. Duplicating a Blueprint gives
// the copy's variables new GUIDs and updates its variable nodes (PostDuplicateBlueprint), but not this text, so every
// such node in the copy failed to compile ("In use pin Return Value no longer exists") until its expression was set
// again by name.
namespace McpBlueprintMathExpression
{
int32 RebindDuplicatedExpressions(UBlueprint& Source, UBlueprint& Copy)
{
  TArray<UK2Node_MathExpression*> Nodes;
  FBlueprintEditorUtils::GetAllNodesOfClass(&Copy, Nodes);
  int32 Rebound = 0;
  for (UK2Node_MathExpression* Node : Nodes)
  {
    FString Text = Node->Expression;
    bool bChanged = false;
    for (int32 Open = Text.Find(TEXT("{")); Open != INDEX_NONE;
         Open = Text.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1))
    {
      // The source still holds the old GUIDs; the variable's name is the same in the copy.
      const int32 Close = Text.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
      FGuid Guid;
      const FName Name = Close != INDEX_NONE && FGuid::Parse(Text.Mid(Open, Close - Open + 1), Guid)
                             ? FBlueprintEditorUtils::FindMemberVariableNameByGuid(&Source, Guid)
                             : NAME_None;
      if (!Name.IsNone())
      {
        Text = Text.Left(Open) + Name.ToString() + Text.Mid(Close + 1);
        bChanged = true;
      }
    }
    if (bChanged)
    {
      // Rebuilding binds each name to the copy's own variable; the node's outer pins and their links stay.
      Node->Modify();
      Node->Expression = Text;
      Node->ReconstructNode();
      ++Rebound;
    }
  }
  if (Rebound > 0)
  {
    // The engine compiled the copy inside the duplicate, before this rebind, and the reply showed those errors as
    // current while the copy stayed uncompiled. Forget the copy's lines and compile it again: that logs what still fails.
    if (UEngineWireSubsystem *Bridge = GEditor ? GEditor->GetEditorSubsystem<UEngineWireSubsystem>() : nullptr)
    {
      const FString CopyPath = Copy.GetPathName();
      Bridge->ForgetCapturedMessages([&CopyPath](const FString &Line) { return Line.Contains(CopyPath); });
    }
    FKismetEditorUtilities::CompileBlueprint(&Copy);
  }
  return Rebound;
}
}
