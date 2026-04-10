#!/usr/bin/env node
/**
 * Fully Consolidated Integration Test Suite
 *
 * Covers all 37 MCP tools across the current public surface:
 * - Groups 1-8: Original 17 tools
 * - Groups 9-26: Advanced authoring and gameplay tools
 *
 * Usage:
 *   node tests/integration.mjs
 *   npm test
 */

import { TestRunner, runToolTests } from "./test-runner.mjs";

const TEST_FOLDER = "/Game/IntegrationTest";
const ADV_TEST_FOLDER = "/Game/AdvancedIntegrationTest";
const WIDGET_FIXTURE_PACKAGE = "/Game/UI/WBP_TestWidget";
const WIDGET_FIXTURE_OBJECT = "/Game/UI/WBP_TestWidget.WBP_TestWidget";
const TARGETING_BLUEPRINT_PATH = `${TEST_FOLDER}/BP_IntegrationTest`;
const NAVIGATION_BLUEPRINT_PATH = `${TEST_FOLDER}/BP_SemanticNavigation`;
const NAVIGATION_WIDGET_PACKAGE = `${ADV_TEST_FOLDER}/WBP_SemanticNavigation`;
const NAVIGATION_WIDGET_OBJECT = `${NAVIGATION_WIDGET_PACKAGE}.WBP_SemanticNavigation`;
const NAVIGATION_WIDGET_FALLBACK_PACKAGE = "/Game/UI/WBP_SemanticNavigation";
const VALIDATION_BLUEPRINT_PATH = `${TEST_FOLDER}/BP_PublicSurfaceValidation`;
const VALIDATION_WIDGET_PACKAGE = `${ADV_TEST_FOLDER}/WBP_PublicSurfaceValidation`;
const VALIDATION_WIDGET_OBJECT = `${VALIDATION_WIDGET_PACKAGE}.WBP_PublicSurfaceValidation`;
const VALIDATION_WIDGET_FALLBACK_PACKAGE =
  "/Game/UI/WBP_PublicSurfaceValidation";
const DESIGNER_MARQUEE_WIDGET_PACKAGE = `${ADV_TEST_FOLDER}/WBP_DesignerMarquee`;
const DESIGNER_MARQUEE_WIDGET_OBJECT = `${DESIGNER_MARQUEE_WIDGET_PACKAGE}.WBP_DesignerMarquee`;
const DESIGNER_MARQUEE_WIDGET_FALLBACK_PACKAGE = "/Game/UI/WBP_DesignerMarquee";
const MARQUEE_WIDGET_A_SLOT = "MarqueeA";
const MARQUEE_WIDGET_B_SLOT = "MarqueeB";
const UI_TARGETING_WIDGET_PACKAGE = `${ADV_TEST_FOLDER}/WBP_UiTargeting`;
const UI_TARGETING_WIDGET_OBJECT = `${UI_TARGETING_WIDGET_PACKAGE}.WBP_UiTargeting`;
const UI_TARGETING_WIDGET_FALLBACK_PACKAGE = "/Game/UI/WBP_UiTargeting";
const UI_TARGETING_WIDGET_COMMAND = "UiTargetingWidgetDesigner";
const UI_TARGETING_WIDGET_TAB_ID = "SlatePreview";
const UI_TARGETING_PREOPEN_WINDOW_HINT = "WBP_UiTargeting__NeedsOpen";
const TARGET_POLICY_STALE_TAB_ID = "MissingSlatePreviewValidation";
const GRAPH_BATCHING_BLUEPRINT = `${ADV_TEST_FOLDER}/BP_GraphBatching`;
const WIDGET_BINDINGS_WIDGET_PACKAGE = `${ADV_TEST_FOLDER}/WBP_WidgetBindings`;
const WIDGET_BINDINGS_WIDGET_OBJECT = `${WIDGET_BINDINGS_WIDGET_PACKAGE}.WBP_WidgetBindings`;
const WIDGET_BINDINGS_WIDGET_FALLBACK_PACKAGE = "/Game/UI/WBP_WidgetBindings";

function isSuccessLike(response, allowedPhrases = []) {
  if (response?.success === true) {
    return true;
  }

  const combined =
    `${response?.message ?? ""} ${response?.error ?? ""}`.toLowerCase();
  return allowedPhrases.some((phrase) =>
    combined.includes(phrase.toLowerCase()),
  );
}

function requireStep(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

function hasPoint(value) {
  return (
    value &&
    typeof value === "object" &&
    typeof value.x === "number" &&
    typeof value.y === "number"
  );
}

function unwrapAutomationResult(response) {
  if (
    response &&
    typeof response === "object" &&
    response.result &&
    typeof response.result === "object"
  ) {
    return {
      success: response.success,
      message: response.message,
      error: response.error,
      errorCode: response.errorCode ?? response.error,
      ...response.result,
    };
  }

  return response;
}

function pickVisibleWindow(windows, titleHints = []) {
  if (!Array.isArray(windows) || windows.length === 0) {
    return null;
  }

  for (const titleHint of titleHints) {
    const hintedWindow = windows.find(
      (windowInfo) =>
        typeof windowInfo?.title === "string" &&
        windowInfo.title.includes(titleHint),
    );

    if (hintedWindow) {
      return hintedWindow;
    }
  }

  return (
    windows.find(
      (windowInfo) =>
        typeof windowInfo?.title === "string" &&
        windowInfo.title.trim().length > 0 &&
        Number(windowInfo?.clientWidth ?? 0) > 0 &&
        Number(windowInfo?.clientHeight ?? 0) > 0,
    ) ?? null
  );
}

function pickTargetingWindow(windows) {
  return pickVisibleWindow(windows, ["BP_IntegrationTest"]);
}

function pickValidationWindow(windows) {
  return pickVisibleWindow(windows, [
    "BP_PublicSurfaceValidation",
    "WBP_PublicSurfaceValidation",
  ]);
}

function pickDesignerMarqueeWindow(windows) {
  return pickVisibleWindow(windows, ["WBP_DesignerMarquee"]);
}

function pickUiTargetingWindow(windows) {
  return pickVisibleWindow(windows, ["WBP_UiTargeting"]);
}

function buildTargetingPoint(windowInfo) {
  const width = Number(windowInfo?.clientWidth ?? 0);
  const height = Number(windowInfo?.clientHeight ?? 0);
  const safeWidth = Number.isFinite(width) && width > 0 ? width : 200;
  const safeHeight = Number.isFinite(height) && height > 0 ? height : 160;

  return {
    clientX: Math.min(
      Math.max(Math.floor(safeWidth / 2), 40),
      Math.max(Math.floor(safeWidth) - 10, 40),
    ),
    clientY: Math.min(
      Math.max(Math.floor(safeHeight / 2), 40),
      Math.max(Math.floor(safeHeight) - 10, 40),
    ),
  };
}

function clampClientCoordinate(value, min, max) {
  const safeMin = Number.isFinite(min) ? min : 0;
  const safeMax = Number.isFinite(max) ? max : safeMin;

  return Math.min(Math.max(Math.floor(value), safeMin), safeMax);
}

function getWidgetTreeLayoutByPath(response, widgetPath) {
  if (typeof widgetPath !== "string" || widgetPath.length === 0) {
    return null;
  }

  function visitWidgetTree(node, currentPath = "") {
    if (!node || typeof node !== "object") {
      return null;
    }

    const nodeName = typeof node.name === "string" ? node.name : "";
    const nextPath =
      nodeName.length > 0
        ? currentPath.length > 0
          ? `${currentPath}/${nodeName}`
          : nodeName
        : currentPath;

    if (
      nextPath === widgetPath &&
      node.layout &&
      typeof node.layout === "object"
    ) {
      return node.layout;
    }

    if (!Array.isArray(node.children)) {
      return null;
    }

    for (const child of node.children) {
      const match = visitWidgetTree(child, nextPath);
      if (match) {
        return match;
      }
    }

    return null;
  }

  return visitWidgetTree(response?.widgetTree);
}

function getWidgetLayoutByPath(response, widgetPath) {
  if (typeof widgetPath !== "string" || widgetPath.length === 0) {
    return null;
  }

  const selectedWidgetLayout = Array.isArray(response?.selectedWidgets)
    ? (response.selectedWidgets.find(
        (widgetInfo) =>
          widgetInfo?.widgetPath === widgetPath &&
          widgetInfo?.layout &&
          typeof widgetInfo.layout === "object",
      )?.layout ?? null)
    : null;

  if (selectedWidgetLayout) {
    return selectedWidgetLayout;
  }

  return getWidgetTreeLayoutByPath(response, widgetPath);
}

function getWidgetLayoutBounds(layout) {
  const designerLeft = Number(layout?.designerBounds?.left ?? NaN);
  const designerTop = Number(layout?.designerBounds?.top ?? NaN);
  const designerRight = Number(layout?.designerBounds?.right ?? NaN);
  const designerBottom = Number(layout?.designerBounds?.bottom ?? NaN);

  if (
    Number.isFinite(designerLeft) &&
    Number.isFinite(designerTop) &&
    Number.isFinite(designerRight) &&
    Number.isFinite(designerBottom) &&
    designerRight > designerLeft &&
    designerBottom > designerTop
  ) {
    return {
      left: designerLeft,
      top: designerTop,
      right: designerRight,
      bottom: designerBottom,
    };
  }

  return null;
}

function buildDesignerMarqueeDrag(widgetALayout, widgetBLayout, windowInfo) {
  const widgetABounds = getWidgetLayoutBounds(widgetALayout);
  const widgetBBounds = getWidgetLayoutBounds(widgetBLayout);
  if (!widgetABounds || !widgetBBounds) {
    return null;
  }

  const width = Number(windowInfo?.clientWidth ?? 0);
  const height = Number(windowInfo?.clientHeight ?? 0);
  const safeWidth = Number.isFinite(width) && width > 0 ? width : 1400;
  const safeHeight = Number.isFinite(height) && height > 0 ? height : 900;
  const dragPadding = 16;
  const startX = clampClientCoordinate(
    Math.min(widgetABounds.left, widgetBBounds.left) - dragPadding,
    40,
    Math.max(safeWidth - 40, 40),
  );
  const startY = clampClientCoordinate(
    Math.min(widgetABounds.top, widgetBBounds.top) - dragPadding,
    40,
    Math.max(safeHeight - 40, 40),
  );
  const endX = clampClientCoordinate(
    Math.max(widgetABounds.right, widgetBBounds.right) + dragPadding,
    startX + 80,
    Math.max(safeWidth - 80, startX + 80),
  );
  const endY = clampClientCoordinate(
    Math.max(widgetABounds.bottom, widgetBBounds.bottom) + dragPadding,
    startY + 80,
    Math.max(safeHeight - 80, startY + 80),
  );

  return {
    geometrySource: "layout_metadata",
    widgetALayout,
    widgetBLayout,
    start: { clientX: startX, clientY: startY },
    end: { clientX: endX, clientY: endY },
  };
}

function buildDesignerRectFromLayout(layout, padding = 12) {
  const bounds = getWidgetLayoutBounds(layout);
  if (!bounds) {
    return null;
  }

  return {
    left: Math.max(Math.floor(bounds.left - padding), 0),
    top: Math.max(Math.floor(bounds.top - padding), 0),
    right: Math.floor(bounds.right + padding),
    bottom: Math.floor(bounds.bottom + padding),
  };
}

function buildDesignerRectFromLayouts(
  widgetALayout,
  widgetBLayout,
  padding = 16,
) {
  const widgetABounds = getWidgetLayoutBounds(widgetALayout);
  const widgetBBounds = getWidgetLayoutBounds(widgetBLayout);
  if (!widgetABounds || !widgetBBounds) {
    return null;
  }

  return {
    left: Math.max(
      Math.floor(Math.min(widgetABounds.left, widgetBBounds.left) - padding),
      0,
    ),
    top: Math.max(
      Math.floor(Math.min(widgetABounds.top, widgetBBounds.top) - padding),
      0,
    ),
    right: Math.floor(
      Math.max(widgetABounds.right, widgetBBounds.right) + padding,
    ),
    bottom: Math.floor(
      Math.max(widgetABounds.bottom, widgetBBounds.bottom) + padding,
    ),
  };
}

function findNamedWidgets(value, targetNames, foundNames = new Set()) {
  if (Array.isArray(value)) {
    for (const item of value) {
      findNamedWidgets(item, targetNames, foundNames);
      if (foundNames.size === targetNames.size) {
        break;
      }
    }

    return foundNames;
  }

  if (!value || typeof value !== "object") {
    return foundNames;
  }

  for (const candidate of [value.name, value.widgetName, value.slotName]) {
    if (typeof candidate === "string" && targetNames.has(candidate)) {
      foundNames.add(candidate);
    }
  }

  for (const nestedValue of Object.values(value)) {
    if (foundNames.size === targetNames.size) {
      break;
    }

    findNamedWidgets(nestedValue, targetNames, foundNames);
  }

  return foundNames;
}

function getSelectedWidgetPaths(response) {
  return Array.isArray(response?.selectedWidgets)
    ? response.selectedWidgets
        .map((widgetInfo) =>
          typeof widgetInfo?.widgetPath === "string"
            ? widgetInfo.widgetPath
            : null,
        )
        .filter((widgetPath) => typeof widgetPath === "string")
    : [];
}

function getSelectedWidgetCount(response) {
  const reportedCount = Number(response?.selectedWidgetCount ?? NaN);
  return Number.isFinite(reportedCount)
    ? reportedCount
    : getSelectedWidgetPaths(response).length;
}

function matchesResolvedWindow(requestedTitle, resolvedTitle) {
  if (typeof requestedTitle !== "string" || typeof resolvedTitle !== "string") {
    return false;
  }

  const requested = requestedTitle.toLowerCase();
  const resolved = resolvedTitle.toLowerCase();
  return (
    requested === resolved ||
    requested.includes(resolved) ||
    resolved.includes(requested)
  );
}

async function runTargetedWindowInputSuite() {
  const runner = new TestRunner("targeted-window-input");
  let selectedWindow = null;
  let targetPoint = null;

  runner.addStep(
    "Targeting: ensure test blueprint fixture exists",
    async (tools) => {
      const folderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(folderResult, ["already exists"]),
        `create_folder failed: ${folderResult?.error ?? folderResult?.message ?? "unknown error"}`,
      );

      const blueprintResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "create",
          name: "BP_IntegrationTest",
          path: TEST_FOLDER,
          parentClass: "Actor",
        }),
      );
      requireStep(
        isSuccessLike(blueprintResult, ["already exists"]),
        `create blueprint failed: ${blueprintResult?.error ?? blueprintResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Targeting: open blueprint editor for explicit window discovery",
    async (tools) => {
      const openResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: TARGETING_BLUEPRINT_PATH,
          },
          { timeoutMs: 15000 },
        ),
      );

      requireStep(
        openResult?.success === true,
        `open_asset failed: ${openResult?.error ?? openResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Targeting: discover visible windows and choose a live target",
    async (tools) => {
      const discoveryResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "list_visible_windows",
        }),
      );

      requireStep(
        discoveryResult?.success === true,
        `list_visible_windows failed: ${discoveryResult?.error ?? discoveryResult?.message ?? "unknown error"}`,
      );
      requireStep(
        Array.isArray(discoveryResult?.windows),
        "list_visible_windows did not return a windows array",
      );
      requireStep(
        Number(discoveryResult?.count ?? 0) > 0,
        "list_visible_windows returned no visible windows",
      );

      selectedWindow = pickTargetingWindow(discoveryResult.windows);
      requireStep(
        selectedWindow !== null,
        "No suitable visible editor window was discovered for targeted input verification",
      );

      targetPoint = buildTargetingPoint(selectedWindow);
      return (
        typeof selectedWindow.title === "string" &&
        selectedWindow.title.length > 0
      );
    },
  );

  runner.addStep(
    "Targeting: capture a targeted editor screenshot",
    async (tools) => {
      requireStep(
        selectedWindow !== null,
        "No selected window available for screenshot targeting",
      );

      const screenshotResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "screenshot",
            filename: "targeted-window-input.png",
            mode: "editor",
            windowTitle: selectedWindow.title,
            includeMenus: false,
          },
          { timeoutMs: 20000 },
        ),
      );

      requireStep(
        screenshotResult?.success === true,
        `screenshot failed: ${screenshotResult?.error ?? screenshotResult?.message ?? "unknown error"}`,
      );
      requireStep(
        screenshotResult?.captureTarget === "editor_window",
        `Expected captureTarget=editor_window, got ${screenshotResult?.captureTarget}`,
      );
      requireStep(
        screenshotResult?.requestedCaptureMode === "editor",
        `Expected requestedCaptureMode=editor, got ${screenshotResult?.requestedCaptureMode}`,
      );
      requireStep(
        screenshotResult?.requestedWindowTitle === selectedWindow.title,
        "Screenshot response did not preserve requestedWindowTitle",
      );
      requireStep(
        matchesResolvedWindow(
          selectedWindow.title,
          screenshotResult?.windowTitle,
        ),
        `Resolved screenshot windowTitle did not match discovery target: ${screenshotResult?.windowTitle}`,
      );

      return (
        typeof screenshotResult?.path === "string" &&
        screenshotResult.path.length > 0
      );
    },
  );

  runner.addStep(
    "Targeting: send targeted mouse input and verify diagnostics",
    async (tools) => {
      requireStep(
        selectedWindow !== null && targetPoint !== null,
        "No selected target window available for simulate_input verification",
      );

      const inputResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "simulate_input",
            inputAction: "mouse_move",
            windowTitle: selectedWindow.title,
            captureScreenshots: false,
            ...targetPoint,
          },
          { timeoutMs: 15000 },
        ),
      );

      requireStep(
        inputResult?.success === true,
        `simulate_input mouse_move failed: ${inputResult?.error ?? inputResult?.message ?? "unknown error"}`,
      );
      requireStep(
        inputResult?.resolvedTargetSource === "window_title",
        `Expected resolvedTargetSource=window_title, got ${inputResult?.resolvedTargetSource}`,
      );
      requireStep(
        matchesResolvedWindow(selectedWindow.title, inputResult?.windowTitle),
        `Resolved input windowTitle did not match discovery target: ${inputResult?.windowTitle}`,
      );
      requireStep(
        typeof inputResult?.clientX === "number" &&
          typeof inputResult?.clientY === "number",
        "simulate_input did not return client coordinates",
      );
      requireStep(
        typeof inputResult?.targetWidgetPathValid === "boolean",
        "simulate_input did not report targetWidgetPathValid",
      );
      requireStep(
        typeof inputResult?.keyboardFocusedWidgetType === "string",
        "simulate_input did not report keyboard focus diagnostics",
      );
      requireStep(
        typeof inputResult?.userFocusedWidgetType === "string",
        "simulate_input did not report user focus diagnostics",
      );

      return true;
    },
  );

  runner.addStep(
    "Targeting: send targeted text input and verify focus diagnostics",
    async (tools) => {
      requireStep(
        selectedWindow !== null,
        "No selected target window available for targeted text input",
      );

      const inputResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "simulate_input",
            inputAction: "text",
            windowTitle: selectedWindow.title,
            text: "targeted-window-input",
            submit: false,
            captureScreenshots: false,
          },
          { timeoutMs: 15000 },
        ),
      );

      requireStep(
        inputResult?.success === true,
        `simulate_input text failed: ${inputResult?.error ?? inputResult?.message ?? "unknown error"}`,
      );
      requireStep(
        inputResult?.resolvedTargetSource === "window_title",
        `Expected text input resolvedTargetSource=window_title, got ${inputResult?.resolvedTargetSource}`,
      );
      requireStep(
        inputResult?.text === "targeted-window-input",
        `Expected echoed text payload, got ${inputResult?.text}`,
      );
      requireStep(
        typeof inputResult?.keyboardFocusedWidgetType === "string",
        "text input did not report keyboard focus diagnostics",
      );
      requireStep(
        typeof inputResult?.userFocusedWidgetType === "string",
        "text input did not report user focus diagnostics",
      );

      return true;
    },
  );

  runner.addStep("Targeting: reset simulated input state", async (tools) => {
    const resetResult = unwrapAutomationResult(
      await tools.executeTool("control_editor", {
        action: "simulate_input",
        inputAction: "reset",
      }),
    );

    requireStep(
      resetResult?.success === true,
      `simulate_input reset failed: ${resetResult?.error ?? resetResult?.message ?? "unknown error"}`,
    );
    return true;
  });

  await runner.run();
}

async function runSemanticNavigationSuite() {
  const runner = new TestRunner("semantic-navigation");
  let graphName = "EventGraph";
  const blueprintHelperGraphName = "ReviewFunction";
  const widgetHelperGraphName = "SetPlayerName";
  let navigationNodeGuid = null;
  let rootWidgetName = null;
  let widgetObjectPath = NAVIGATION_WIDGET_OBJECT;
  let widgetAssetPath = NAVIGATION_WIDGET_PACKAGE;

  runner.addStep(
    "Semantic navigation: ensure fixture folders exist",
    async (tools) => {
      const testFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(testFolderResult, ["already exists"]),
        `create test folder failed: ${testFolderResult?.error ?? testFolderResult?.message ?? "unknown error"}`,
      );

      const advFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advFolderResult, ["already exists"]),
        `create advanced test folder failed: ${advFolderResult?.error ?? advFolderResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: create and open Blueprint navigation fixture",
    async (tools) => {
      const blueprintResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "create",
          name: "BP_SemanticNavigation",
          path: TEST_FOLDER,
          parentClass: "Actor",
        }),
      );
      requireStep(
        isSuccessLike(blueprintResult, ["already exists"]),
        `create blueprint failed: ${blueprintResult?.error ?? blueprintResult?.message ?? "unknown error"}`,
      );

      const openResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: NAVIGATION_BLUEPRINT_PATH,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        openResult?.success === true,
        `open_asset failed: ${openResult?.error ?? openResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: add a named helper function graph for semantic navigation",
    async (tools) => {
      const functionResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "add_function",
          blueprintPath: NAVIGATION_BLUEPRINT_PATH,
          functionName: blueprintHelperGraphName,
        }),
      );
      requireStep(
        isSuccessLike(functionResult, ["already exists", "duplicate"]),
        `add_function failed: ${functionResult?.error ?? functionResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: create node fixture for semantic Blueprint navigation",
    async (tools) => {
      const nodeResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "create_node",
          blueprintPath: NAVIGATION_BLUEPRINT_PATH,
          graphName,
          nodeType: "PrintString",
          x: 240,
          y: 0,
        }),
      );
      requireStep(
        nodeResult?.success === true,
        `create_node failed: ${nodeResult?.error ?? nodeResult?.message ?? "unknown error"}`,
      );

      if (
        typeof nodeResult?.graphName === "string" &&
        nodeResult.graphName.length > 0
      ) {
        graphName = nodeResult.graphName;
      }

      navigationNodeGuid = nodeResult?.nodeId ?? null;
      requireStep(
        typeof navigationNodeGuid === "string" && navigationNodeGuid.length > 0,
        "create_node did not return a nodeId for semantic navigation",
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: fit the Blueprint graph semantically",
    async (tools) => {
      const fitResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "fit_blueprint_graph",
          assetPath: NAVIGATION_BLUEPRINT_PATH,
          graphName,
          scope: "full",
        }),
      );
      requireStep(
        fitResult?.success === true,
        `fit_blueprint_graph failed: ${fitResult?.error ?? fitResult?.message ?? "unknown error"}`,
      );
      requireStep(
        fitResult?.scope === undefined || fitResult.scope === "full",
        `fit_blueprint_graph returned unexpected scope: ${fitResult?.scope}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: fit a named Blueprint helper function graph semantically",
    async (tools) => {
      const fitResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "fit_blueprint_graph",
          assetPath: NAVIGATION_BLUEPRINT_PATH,
          graphName: blueprintHelperGraphName,
          scope: "full",
        }),
      );
      requireStep(
        fitResult?.success === true,
        `fit_blueprint_graph failed for helper function graph: ${fitResult?.error ?? fitResult?.message ?? "unknown error"}`,
      );
      requireStep(
        fitResult?.resolvedGraphName === blueprintHelperGraphName ||
          fitResult?.graphName === blueprintHelperGraphName,
        `fit_blueprint_graph did not resolve the requested helper function graph: ${JSON.stringify(fitResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: move the Blueprint graph view semantically",
    async (tools) => {
      const viewResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_blueprint_graph_view",
          assetPath: NAVIGATION_BLUEPRINT_PATH,
          graphName,
          delta: { x: 240, y: 120 },
          preserveZoom: true,
        }),
      );
      requireStep(
        viewResult?.success === true,
        `set_blueprint_graph_view failed: ${viewResult?.error ?? viewResult?.message ?? "unknown error"}`,
      );
      requireStep(
        typeof viewResult?.graphName === "string" ||
          typeof viewResult?.resolvedGraphName === "string",
        "set_blueprint_graph_view did not report graph diagnostics",
      );
      requireStep(
        hasPoint(viewResult?.viewLocation) ||
          hasPoint(viewResult?.requestedViewLocation) ||
          hasPoint(viewResult?.delta),
        "set_blueprint_graph_view did not report view diagnostics",
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: reveal a Blueprint node semantically",
    async (tools) => {
      requireStep(
        typeof navigationNodeGuid === "string" && navigationNodeGuid.length > 0,
        "No node guid captured for semantic Blueprint navigation",
      );

      const jumpResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "jump_to_blueprint_node",
          assetPath: NAVIGATION_BLUEPRINT_PATH,
          graphName,
          nodeGuid: navigationNodeGuid,
        }),
      );
      requireStep(
        jumpResult?.success === true,
        `jump_to_blueprint_node failed: ${jumpResult?.error ?? jumpResult?.message ?? "unknown error"}`,
      );
      requireStep(
        typeof jumpResult?.matchedNodeId === "string" ||
          typeof jumpResult?.matchedNodeTitle === "string" ||
          typeof jumpResult?.nodeSelector === "string",
        "jump_to_blueprint_node did not return match diagnostics",
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: create widget fixture for Designer navigation",
    async (tools) => {
      const deleteWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: NAVIGATION_WIDGET_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteWidgetResult, ["not found"]),
        `delete widget fixture failed: ${deleteWidgetResult?.error ?? deleteWidgetResult?.message ?? "unknown error"}`,
      );

      const deleteFallbackWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: NAVIGATION_WIDGET_FALLBACK_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteFallbackWidgetResult, ["not found"]),
        `delete fallback widget fixture failed: ${deleteFallbackWidgetResult?.error ?? deleteFallbackWidgetResult?.message ?? "unknown error"}`,
      );

      const widgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "create_widget_blueprint",
          name: "WBP_SemanticNavigation",
          folder: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(widgetResult, ["already exists"]),
        `create_widget_blueprint failed: ${widgetResult?.error ?? widgetResult?.message ?? "unknown error"}`,
      );

      if (
        typeof widgetResult?.widgetPath === "string" &&
        widgetResult.widgetPath.length > 0
      ) {
        widgetObjectPath = widgetResult.widgetPath;
        const objectSeparatorIndex = widgetObjectPath.indexOf(
          ".",
          widgetObjectPath.lastIndexOf("/"),
        );
        widgetAssetPath =
          objectSeparatorIndex > 0
            ? widgetObjectPath.slice(0, objectSeparatorIndex)
            : widgetObjectPath;
      }

      const canvasResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "add_canvas_panel",
          widgetPath: widgetObjectPath,
          slotName: "RootCanvas",
        }),
      );
      requireStep(
        canvasResult?.success === true,
        `add_canvas_panel failed: ${canvasResult?.error ?? canvasResult?.message ?? "unknown error"}`,
      );

      const widgetTreeResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "get_widget_tree",
          widgetPath: widgetObjectPath,
        }),
      );
      requireStep(
        widgetTreeResult?.success === true,
        `get_widget_tree failed: ${widgetTreeResult?.error ?? widgetTreeResult?.message ?? "unknown error"}`,
      );

      rootWidgetName =
        widgetTreeResult?.rootWidgetName ??
        widgetTreeResult?.widgetTree?.name ??
        null;
      requireStep(
        typeof rootWidgetName === "string" && rootWidgetName.length > 0,
        "get_widget_tree did not return a root widget name for designer navigation",
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: open a named Widget Blueprint helper function graph semantically",
    async (tools) => {
      const functionResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "add_function",
          blueprintPath: widgetAssetPath,
          functionName: widgetHelperGraphName,
        }),
      );
      requireStep(
        isSuccessLike(functionResult, ["already exists", "duplicate"]),
        `add_function failed for widget blueprint: ${functionResult?.error ?? functionResult?.message ?? "unknown error"}`,
      );

      const graphModeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "graph",
        }),
      );
      requireStep(
        graphModeResult?.success === true,
        `set_widget_blueprint_mode graph failed: ${graphModeResult?.error ?? graphModeResult?.message ?? "unknown error"}`,
      );

      const fitResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "fit_blueprint_graph",
          assetPath: widgetAssetPath,
          graphName: widgetHelperGraphName,
          scope: "full",
        }),
      );
      requireStep(
        fitResult?.success === true,
        `fit_blueprint_graph failed for widget helper function graph: ${fitResult?.error ?? fitResult?.message ?? "unknown error"}`,
      );
      requireStep(
        fitResult?.resolvedGraphName === widgetHelperGraphName ||
          fitResult?.graphName === widgetHelperGraphName,
        `fit_blueprint_graph did not resolve the requested widget helper function graph: ${JSON.stringify(fitResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: switch the Widget Blueprint into Designer mode",
    async (tools) => {
      const openResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: widgetAssetPath,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        openResult?.success === true,
        `open_asset failed for widget fixture: ${JSON.stringify(openResult)}`,
      );

      const modeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "designer",
        }),
      );
      requireStep(
        modeResult?.success === true,
        `set_widget_blueprint_mode failed: ${modeResult?.error ?? modeResult?.message ?? "unknown error"}`,
      );
      requireStep(
        typeof modeResult?.currentMode === "string" ||
          typeof modeResult?.requestedMode === "string" ||
          typeof modeResult?.enteredDesignerMode === "boolean",
        "set_widget_blueprint_mode did not report mode diagnostics",
      );

      return true;
    },
  );

  runner.addStep(
    "Semantic navigation: fit and select within the Widget Designer semantically",
    async (tools) => {
      requireStep(
        typeof rootWidgetName === "string" && rootWidgetName.length > 0,
        "No root widget name captured for designer selection",
      );

      const fitResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "fit_widget_designer",
          assetPath: widgetAssetPath,
        }),
      );
      requireStep(
        fitResult?.success === true,
        `fit_widget_designer failed: ${fitResult?.error ?? fitResult?.message ?? "unknown error"}`,
      );
      requireStep(
        typeof fitResult?.fitExecuted === "boolean" ||
          typeof fitResult?.queuedDesignerAction === "boolean" ||
          typeof fitResult?.designerActionDisposition === "string",
        "fit_widget_designer did not report fit diagnostics",
      );

      const selectResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widget_in_designer",
          assetPath: widgetAssetPath,
          widgetName: rootWidgetName,
        }),
      );
      requireStep(
        selectResult?.success === true,
        `select_widget_in_designer failed: ${selectResult?.error ?? selectResult?.message ?? "unknown error"}`,
      );
      requireStep(
        typeof selectResult?.selectionApplied === "boolean" ||
          typeof selectResult?.queuedDesignerAction === "boolean" ||
          typeof selectResult?.resolvedWidgetName === "string" ||
          typeof selectResult?.revealExecuted === "boolean",
        "select_widget_in_designer did not return selection diagnostics",
      );

      return true;
    },
  );

  await runner.run();
}

async function runGraphReviewSuite() {
  const runner = new TestRunner("graph-review");
  const blueprintHelperGraphName = "ReviewFunction";
  const widgetHelperGraphName = "SetPlayerName";
  let widgetObjectPath = NAVIGATION_WIDGET_OBJECT;
  let widgetAssetPath = NAVIGATION_WIDGET_PACKAGE;
  let blueprintHelperNodeGuid = null;
  let widgetHelperNodeGuid = null;

  runner.addStep(
    "Graph review: provision the shipped semantic navigation helper-graph fixtures",
    async (tools) => {
      const testFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(testFolderResult, ["already exists"]),
        `create test folder failed: ${testFolderResult?.error ?? testFolderResult?.message ?? "unknown error"}`,
      );

      const advFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advFolderResult, ["already exists"]),
        `create advanced test folder failed: ${advFolderResult?.error ?? advFolderResult?.message ?? "unknown error"}`,
      );

      const blueprintResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "create",
          name: "BP_SemanticNavigation",
          path: TEST_FOLDER,
          parentClass: "Actor",
        }),
      );
      requireStep(
        isSuccessLike(blueprintResult, ["already exists"]),
        `create blueprint failed: ${blueprintResult?.error ?? blueprintResult?.message ?? "unknown error"}`,
      );

      const openBlueprintResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: NAVIGATION_BLUEPRINT_PATH,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        openBlueprintResult?.success === true,
        `open_asset failed for Blueprint helper review fixture: ${openBlueprintResult?.error ?? openBlueprintResult?.message ?? "unknown error"}`,
      );

      const blueprintFunctionResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "add_function",
          blueprintPath: NAVIGATION_BLUEPRINT_PATH,
          functionName: blueprintHelperGraphName,
        }),
      );
      requireStep(
        isSuccessLike(blueprintFunctionResult, ["already exists", "duplicate"]),
        `add_function failed for Blueprint helper graph: ${blueprintFunctionResult?.error ?? blueprintFunctionResult?.message ?? "unknown error"}`,
      );

      const deleteWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: NAVIGATION_WIDGET_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteWidgetResult, ["not found"]),
        `delete widget review fixture failed: ${deleteWidgetResult?.error ?? deleteWidgetResult?.message ?? "unknown error"}`,
      );

      const deleteFallbackWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: NAVIGATION_WIDGET_FALLBACK_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteFallbackWidgetResult, ["not found"]),
        `delete fallback widget review fixture failed: ${deleteFallbackWidgetResult?.error ?? deleteFallbackWidgetResult?.message ?? "unknown error"}`,
      );

      const widgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "create_widget_blueprint",
          name: "WBP_SemanticNavigation",
          folder: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(widgetResult, ["already exists"]),
        `create_widget_blueprint failed: ${widgetResult?.error ?? widgetResult?.message ?? "unknown error"}`,
      );

      if (
        typeof widgetResult?.widgetPath === "string" &&
        widgetResult.widgetPath.length > 0
      ) {
        widgetObjectPath = widgetResult.widgetPath;
        const objectSeparatorIndex = widgetObjectPath.indexOf(
          ".",
          widgetObjectPath.lastIndexOf("/"),
        );
        widgetAssetPath =
          objectSeparatorIndex > 0
            ? widgetObjectPath.slice(0, objectSeparatorIndex)
            : widgetObjectPath;
      }

      const canvasResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "add_canvas_panel",
          widgetPath: widgetObjectPath,
          slotName: "RootCanvas",
        }),
      );
      requireStep(
        canvasResult?.success === true,
        `add_canvas_panel failed for widget review fixture: ${canvasResult?.error ?? canvasResult?.message ?? "unknown error"}`,
      );

      const widgetFunctionResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "add_function",
          blueprintPath: widgetAssetPath,
          functionName: widgetHelperGraphName,
        }),
      );
      requireStep(
        isSuccessLike(widgetFunctionResult, ["already exists", "duplicate"]),
        `add_function failed for widget helper graph: ${widgetFunctionResult?.error ?? widgetFunctionResult?.message ?? "unknown error"}`,
      );

      const openWidgetResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: widgetAssetPath,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        openWidgetResult?.success === true,
        `open_asset failed for widget review fixture: ${openWidgetResult?.error ?? openWidgetResult?.message ?? "unknown error"}`,
      );

      const graphModeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "graph",
        }),
      );
      requireStep(
        graphModeResult?.success === true,
        `set_widget_blueprint_mode graph failed for review fixture: ${graphModeResult?.error ?? graphModeResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Graph review: add a Blueprint helper-graph node for readable capture",
    async (tools) => {
      const nodeResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "create_node",
          blueprintPath: NAVIGATION_BLUEPRINT_PATH,
          graphName: blueprintHelperGraphName,
          nodeType: "PrintString",
          x: 240,
          y: 0,
        }),
      );
      requireStep(
        nodeResult?.success === true,
        `create_node failed for Blueprint helper graph: ${nodeResult?.error ?? nodeResult?.message ?? "unknown error"}`,
      );
      blueprintHelperNodeGuid = nodeResult?.nodeId ?? null;
      requireStep(
        typeof blueprintHelperNodeGuid === "string" &&
          blueprintHelperNodeGuid.length > 0,
        "create_node did not return a Blueprint helper nodeId",
      );

      return true;
    },
  );

  runner.addStep(
    "Graph review: add a Widget Blueprint helper-graph node for readable capture",
    async (tools) => {
      const nodeResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "create_node",
          blueprintPath: widgetAssetPath,
          graphName: widgetHelperGraphName,
          nodeType: "PrintString",
          x: 240,
          y: 0,
        }),
      );
      requireStep(
        nodeResult?.success === true,
        `create_node failed for widget helper graph: ${nodeResult?.error ?? nodeResult?.message ?? "unknown error"}`,
      );
      widgetHelperNodeGuid = nodeResult?.nodeId ?? null;
      requireStep(
        typeof widgetHelperNodeGuid === "string" &&
          widgetHelperNodeGuid.length > 0,
        "create_node did not return a widget helper nodeId",
      );

      return true;
    },
  );

  runner.addStep(
    "Graph review: capture a readable Blueprint helper-graph review in one call",
    async (tools) => {
      requireStep(
        typeof blueprintHelperNodeGuid === "string" &&
          blueprintHelperNodeGuid.length > 0,
        "No Blueprint helper node guid captured for readable review",
      );

      const reviewResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "capture_blueprint_graph_review",
          assetPath: NAVIGATION_BLUEPRINT_PATH,
          graphName: blueprintHelperGraphName,
          nodeGuid: blueprintHelperNodeGuid,
          scope: "selection",
          filename: "graph-review-blueprint.png",
        }),
      );
      requireStep(
        reviewResult?.success === true,
        `capture_blueprint_graph_review failed for Blueprint helper graph: ${reviewResult?.error ?? reviewResult?.message ?? "unknown error"}`,
      );
      requireStep(
        reviewResult?.resolvedGraphName === blueprintHelperGraphName ||
          reviewResult?.graphName === blueprintHelperGraphName,
        `capture_blueprint_graph_review did not resolve the requested Blueprint helper graph: ${JSON.stringify(reviewResult)}`,
      );
      requireStep(
        reviewResult?.matchedNodeId === blueprintHelperNodeGuid,
        `capture_blueprint_graph_review did not preserve the Blueprint helper node match: ${JSON.stringify(reviewResult)}`,
      );
      requireStep(
        reviewResult?.captureTarget === "editor_window",
        `Expected captureTarget=editor_window for Blueprint helper review: ${JSON.stringify(reviewResult)}`,
      );
      requireStep(
        typeof reviewResult?.path === "string" && reviewResult.path.length > 0,
        `capture_blueprint_graph_review did not return a screenshot path for the Blueprint helper graph: ${JSON.stringify(reviewResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Graph review: capture a readable Widget Blueprint helper-graph review in one call",
    async (tools) => {
      requireStep(
        typeof widgetHelperNodeGuid === "string" &&
          widgetHelperNodeGuid.length > 0,
        "No widget helper node guid captured for readable review",
      );

      const graphModeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "graph",
        }),
      );
      requireStep(
        graphModeResult?.success === true,
        `set_widget_blueprint_mode graph failed for widget review capture: ${graphModeResult?.error ?? graphModeResult?.message ?? "unknown error"}`,
      );

      const reviewResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "capture_blueprint_graph_review",
          assetPath: widgetAssetPath,
          graphName: widgetHelperGraphName,
          nodeGuid: widgetHelperNodeGuid,
          scope: "selection",
          filename: "graph-review-widget.png",
        }),
      );
      requireStep(
        reviewResult?.success === true,
        `capture_blueprint_graph_review failed for widget helper graph: ${reviewResult?.error ?? reviewResult?.message ?? "unknown error"}`,
      );
      requireStep(
        reviewResult?.resolvedGraphName === widgetHelperGraphName ||
          reviewResult?.graphName === widgetHelperGraphName,
        `capture_blueprint_graph_review did not resolve the requested widget helper graph: ${JSON.stringify(reviewResult)}`,
      );
      requireStep(
        reviewResult?.matchedNodeId === widgetHelperNodeGuid,
        `capture_blueprint_graph_review did not preserve the widget helper node match: ${JSON.stringify(reviewResult)}`,
      );
      requireStep(
        reviewResult?.captureTarget === "editor_window",
        `Expected captureTarget=editor_window for widget helper review: ${JSON.stringify(reviewResult)}`,
      );
      requireStep(
        typeof reviewResult?.path === "string" && reviewResult.path.length > 0,
        `capture_blueprint_graph_review did not return a screenshot path for the widget helper graph: ${JSON.stringify(reviewResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Graph review: retrieve a bounded Blueprint helper-graph review summary",
    async (tools) => {
      const summaryResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "get_graph_review_summary",
          blueprintPath: NAVIGATION_BLUEPRINT_PATH,
          graphName: blueprintHelperGraphName,
        }),
      );
      requireStep(
        summaryResult?.success === true,
        `get_graph_review_summary failed for Blueprint helper graph: ${summaryResult?.error ?? summaryResult?.message ?? "unknown error"}`,
      );
      requireStep(
        summaryResult?.graphName === blueprintHelperGraphName,
        `Expected graphName=${blueprintHelperGraphName} from get_graph_review_summary: ${JSON.stringify(summaryResult)}`,
      );
      requireStep(
        typeof summaryResult?.connectionCount === "number",
        `Expected numeric connectionCount from get_graph_review_summary: ${JSON.stringify(summaryResult)}`,
      );
      requireStep(
        Array.isArray(summaryResult?.entryNodes),
        `Expected entryNodes array from get_graph_review_summary: ${JSON.stringify(summaryResult)}`,
      );
      requireStep(
        Array.isArray(summaryResult?.reviewTargets),
        `Expected reviewTargets array from get_graph_review_summary: ${JSON.stringify(summaryResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Graph review: retrieve a bounded Widget Blueprint helper-graph review summary",
    async (tools) => {
      const summaryResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "get_graph_review_summary",
          blueprintPath: widgetAssetPath,
          graphName: widgetHelperGraphName,
        }),
      );
      requireStep(
        summaryResult?.success === true,
        `get_graph_review_summary failed for widget helper graph: ${summaryResult?.error ?? summaryResult?.message ?? "unknown error"}`,
      );
      requireStep(
        summaryResult?.graphName === widgetHelperGraphName,
        `Expected graphName=${widgetHelperGraphName} from get_graph_review_summary: ${JSON.stringify(summaryResult)}`,
      );
      requireStep(
        typeof summaryResult?.connectionCount === "number",
        `Expected numeric connectionCount from get_graph_review_summary: ${JSON.stringify(summaryResult)}`,
      );
      requireStep(
        Array.isArray(summaryResult?.entryNodes),
        `Expected entryNodes array from widget get_graph_review_summary: ${JSON.stringify(summaryResult)}`,
      );
      requireStep(
        Array.isArray(summaryResult?.reviewTargets),
        `Expected reviewTargets array from widget get_graph_review_summary: ${JSON.stringify(summaryResult)}`,
      );

      return true;
    },
  );

  await runner.run();
}

async function runPublicSurfaceValidationSuite() {
  const runner = new TestRunner("public-surface-validation");
  let selectedWindow = null;
  let targetPoint = null;
  let graphName = "EventGraph";
  let printStringNodeId = null;
  let rootWidgetName = null;
  let widgetObjectPath = VALIDATION_WIDGET_OBJECT;
  let widgetAssetPath = VALIDATION_WIDGET_PACKAGE;

  runner.addStep(
    "Public surface validation: ensure fixture folders exist",
    async (tools) => {
      const testFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(testFolderResult, ["already exists"]),
        `create_folder failed for ${TEST_FOLDER}: ${testFolderResult?.error ?? testFolderResult?.message ?? "unknown error"}`,
      );

      const advancedFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advancedFolderResult, ["already exists"]),
        `create_folder failed for ${ADV_TEST_FOLDER}: ${advancedFolderResult?.error ?? advancedFolderResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Public surface validation: create and open Blueprint validation fixture",
    async (tools) => {
      const blueprintResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "create",
          name: "BP_PublicSurfaceValidation",
          path: TEST_FOLDER,
          parentClass: "Actor",
        }),
      );
      requireStep(
        isSuccessLike(blueprintResult, ["already exists"]),
        `create blueprint failed: ${blueprintResult?.error ?? blueprintResult?.message ?? "unknown error"}`,
      );

      const openResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: VALIDATION_BLUEPRINT_PATH,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        openResult?.success === true,
        `open_asset failed for blueprint fixture: ${openResult?.error ?? openResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Public surface validation: create node fixture for graph and pin inspection",
    async (tools) => {
      const nodeResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "create_node",
          blueprintPath: VALIDATION_BLUEPRINT_PATH,
          graphName: "EventGraph",
          nodeType: "PrintString",
          x: 320,
          y: 0,
        }),
      );
      requireStep(
        nodeResult?.success === true,
        `create_node failed: ${nodeResult?.error ?? nodeResult?.message ?? "unknown error"}`,
      );
      requireStep(
        typeof nodeResult?.nodeId === "string" && nodeResult.nodeId.length > 0,
        "create_node did not return a nodeId for pin inspection",
      );

      printStringNodeId = nodeResult.nodeId;
      return true;
    },
  );

  runner.addStep(
    "Public surface validation: inspect graph details through the public action",
    async (tools) => {
      const graphResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "get_graph_details",
          blueprintPath: VALIDATION_BLUEPRINT_PATH,
          graphName: "EventGraph",
        }),
      );
      requireStep(
        graphResult?.success === true,
        `get_graph_details failed: ${graphResult?.error ?? graphResult?.message ?? "unknown error"}`,
      );
      requireStep(
        Array.isArray(graphResult?.nodes),
        "get_graph_details did not return a nodes array",
      );
      requireStep(
        typeof graphResult?.graphName === "string" &&
          graphResult.graphName.length > 0,
        "get_graph_details did not return graphName",
      );

      graphName = graphResult.graphName;
      return true;
    },
  );

  runner.addStep(
    "Public surface validation: inspect pin details through the public action",
    async (tools) => {
      requireStep(
        typeof printStringNodeId === "string" && printStringNodeId.length > 0,
        "No nodeId captured for pin inspection",
      );

      const pinResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "get_pin_details",
          blueprintPath: VALIDATION_BLUEPRINT_PATH,
          graphName,
          nodeId: printStringNodeId,
          pinName: "InString",
        }),
      );
      requireStep(
        pinResult?.success === true,
        `get_pin_details failed: ${pinResult?.error ?? pinResult?.message ?? "unknown error"}`,
      );
      requireStep(
        Array.isArray(pinResult?.pins),
        "get_pin_details did not return a pins array",
      );

      return true;
    },
  );

  runner.addStep(
    "Public surface validation: discover visible windows for targeted editor checks",
    async (tools) => {
      const discoveryResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "list_visible_windows",
        }),
      );
      requireStep(
        discoveryResult?.success === true,
        `list_visible_windows failed: ${discoveryResult?.error ?? discoveryResult?.message ?? "unknown error"}`,
      );
      requireStep(
        Array.isArray(discoveryResult?.windows),
        "list_visible_windows did not return a windows array",
      );
      requireStep(
        Number(discoveryResult?.count ?? 0) > 0,
        "list_visible_windows returned no visible windows",
      );

      selectedWindow = pickValidationWindow(discoveryResult.windows);
      requireStep(
        selectedWindow !== null,
        "No suitable visible editor window was discovered for public surface validation",
      );

      targetPoint = buildTargetingPoint(selectedWindow);
      return true;
    },
  );

  runner.addStep(
    "Public surface validation: capture a targeted screenshot through the public contract",
    async (tools) => {
      requireStep(
        selectedWindow !== null,
        "No selected window available for screenshot targeting",
      );

      const screenshotResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "screenshot",
            filename: "public-surface-validation.png",
            mode: "editor",
            windowTitle: selectedWindow.title,
            includeMenus: false,
          },
          { timeoutMs: 20000 },
        ),
      );
      requireStep(
        screenshotResult?.success === true,
        `screenshot failed: ${screenshotResult?.error ?? screenshotResult?.message ?? "unknown error"}`,
      );
      requireStep(
        screenshotResult?.captureTarget === "editor_window",
        `Expected captureTarget=editor_window, got ${screenshotResult?.captureTarget}`,
      );
      requireStep(
        matchesResolvedWindow(
          selectedWindow.title,
          screenshotResult?.windowTitle,
        ),
        `Resolved screenshot windowTitle did not match selected target: ${screenshotResult?.windowTitle}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Public surface validation: send targeted pointer input and verify diagnostics",
    async (tools) => {
      requireStep(
        selectedWindow !== null && targetPoint !== null,
        "No selected target window available for pointer validation",
      );

      const inputResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "simulate_input",
            inputAction: "mouse_move",
            windowTitle: selectedWindow.title,
            captureScreenshots: false,
            ...targetPoint,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        inputResult?.success === true,
        `simulate_input mouse_move failed: ${inputResult?.error ?? inputResult?.message ?? "unknown error"}`,
      );
      requireStep(
        inputResult?.resolvedTargetSource === "window_title",
        `Expected resolvedTargetSource=window_title, got ${inputResult?.resolvedTargetSource}`,
      );
      requireStep(
        typeof inputResult?.clientX === "number" &&
          typeof inputResult?.clientY === "number",
        "simulate_input did not return client coordinates",
      );

      return true;
    },
  );

  runner.addStep(
    "Public surface validation: fit the Blueprint graph semantically",
    async (tools) => {
      const fitResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "fit_blueprint_graph",
          assetPath: VALIDATION_BLUEPRINT_PATH,
          graphName,
          scope: "full",
        }),
      );
      requireStep(
        fitResult?.success === true,
        `fit_blueprint_graph failed: ${fitResult?.error ?? fitResult?.message ?? "unknown error"}`,
      );
      requireStep(
        fitResult?.scope === undefined || fitResult.scope === "full",
        `fit_blueprint_graph returned unexpected scope: ${fitResult?.scope}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Public surface validation: create widget fixture for tree and designer validation",
    async (tools) => {
      const deleteWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: VALIDATION_WIDGET_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteWidgetResult, ["not found"]),
        `delete widget fixture failed: ${deleteWidgetResult?.error ?? deleteWidgetResult?.message ?? "unknown error"}`,
      );

      const deleteFallbackWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: VALIDATION_WIDGET_FALLBACK_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteFallbackWidgetResult, ["not found"]),
        `delete fallback widget fixture failed: ${deleteFallbackWidgetResult?.error ?? deleteFallbackWidgetResult?.message ?? "unknown error"}`,
      );

      const widgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "create_widget_blueprint",
          name: "WBP_PublicSurfaceValidation",
          folder: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(widgetResult, ["already exists"]),
        `create_widget_blueprint failed: ${widgetResult?.error ?? widgetResult?.message ?? "unknown error"}`,
      );

      if (
        typeof widgetResult?.widgetPath === "string" &&
        widgetResult.widgetPath.length > 0
      ) {
        widgetObjectPath = widgetResult.widgetPath;
        const objectSeparatorIndex = widgetObjectPath.indexOf(
          ".",
          widgetObjectPath.lastIndexOf("/"),
        );
        widgetAssetPath =
          objectSeparatorIndex > 0
            ? widgetObjectPath.slice(0, objectSeparatorIndex)
            : widgetObjectPath;
      }

      const canvasResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "add_canvas_panel",
          widgetPath: widgetObjectPath,
          slotName: "RootCanvas",
        }),
      );
      requireStep(
        canvasResult?.success === true,
        `add_canvas_panel failed: ${canvasResult?.error ?? canvasResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Public surface validation: inspect widget tree through the public action",
    async (tools) => {
      const widgetTreeResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "get_widget_tree",
          widgetPath: widgetObjectPath,
        }),
      );
      requireStep(
        widgetTreeResult?.success === true,
        `get_widget_tree failed: ${widgetTreeResult?.error ?? widgetTreeResult?.message ?? "unknown error"}`,
      );
      requireStep(
        widgetTreeResult?.widgetTree &&
          typeof widgetTreeResult.widgetTree === "object",
        "get_widget_tree did not return widgetTree data",
      );

      rootWidgetName =
        widgetTreeResult?.rootWidgetName ??
        widgetTreeResult?.widgetTree?.name ??
        null;
      requireStep(
        typeof rootWidgetName === "string" && rootWidgetName.length > 0,
        "get_widget_tree did not return a rootWidgetName",
      );

      return true;
    },
  );

  runner.addStep(
    "Public surface validation: open widget asset and switch to Designer mode",
    async (tools) => {
      const openResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: widgetAssetPath,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        openResult?.success === true,
        `open_asset failed for widget fixture: ${JSON.stringify(openResult)}`,
      );

      const modeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "designer",
        }),
      );
      requireStep(
        modeResult?.success === true,
        `set_widget_blueprint_mode failed: ${modeResult?.error ?? modeResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Public surface validation: fit and select within the Widget Designer semantically",
    async (tools) => {
      requireStep(
        typeof rootWidgetName === "string" && rootWidgetName.length > 0,
        "No root widget name captured for designer selection",
      );

      const fitResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "fit_widget_designer",
          assetPath: widgetAssetPath,
        }),
      );
      requireStep(
        fitResult?.success === true,
        `fit_widget_designer failed: ${fitResult?.error ?? fitResult?.message ?? "unknown error"}`,
      );
      requireStep(
        typeof fitResult?.fitExecuted === "boolean" ||
          typeof fitResult?.queuedDesignerAction === "boolean" ||
          typeof fitResult?.designerActionDisposition === "string",
        "fit_widget_designer did not report fit diagnostics",
      );

      const selectResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widget_in_designer",
          assetPath: widgetAssetPath,
          widgetName: rootWidgetName,
        }),
      );
      requireStep(
        selectResult?.success === true,
        `select_widget_in_designer failed: ${selectResult?.error ?? selectResult?.message ?? "unknown error"}`,
      );
      requireStep(
        typeof selectResult?.selectionApplied === "boolean" ||
          typeof selectResult?.queuedDesignerAction === "boolean" ||
          typeof selectResult?.resolvedWidgetName === "string" ||
          typeof selectResult?.revealExecuted === "boolean",
        "select_widget_in_designer did not return selection diagnostics",
      );

      return true;
    },
  );

  await runner.run();
}

async function runDesignerMarqueeSuite() {
  const runner = new TestRunner("designer-marquee");
  let selectedWindow = null;
  let designerTabId = null;
  let widgetObjectPath = DESIGNER_MARQUEE_WIDGET_OBJECT;
  let widgetAssetPath = DESIGNER_MARQUEE_WIDGET_PACKAGE;
  let expectedWidgetAPath = null;
  let expectedWidgetBPath = null;
  let expectedWidgetALayout = null;
  let expectedWidgetBLayout = null;
  let lastResolveResult = null;
  let lastViewResult = null;

  async function getDesignerState(tools) {
    const stateResult = unwrapAutomationResult(
      await tools.executeTool(
        "manage_widget_authoring",
        {
          action: "get_widget_designer_state",
          widgetPath: widgetObjectPath,
          openEditorIfNeeded: false,
        },
        { timeoutMs: 15000 },
      ),
    );

    requireStep(
      stateResult?.success === true,
      `get_widget_designer_state failed: ${stateResult?.error ?? stateResult?.message ?? "unknown error"}`,
    );

    if (
      typeof stateResult?.tabId === "string" &&
      stateResult.tabId.length > 0
    ) {
      designerTabId = stateResult.tabId;
    }

    return stateResult;
  }

  runner.addStep(
    "Designer marquee: ensure marquee fixture folder exists",
    async (tools) => {
      const advancedFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advancedFolderResult, ["already exists"]),
        `create_folder failed for ${ADV_TEST_FOLDER}: ${advancedFolderResult?.error ?? advancedFolderResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer marquee: create dedicated widget fixture with deterministic sibling slots",
    async (tools) => {
      const deleteWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: DESIGNER_MARQUEE_WIDGET_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteWidgetResult, ["not found"]),
        `delete widget fixture failed: ${deleteWidgetResult?.error ?? deleteWidgetResult?.message ?? "unknown error"}`,
      );

      const deleteFallbackWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: DESIGNER_MARQUEE_WIDGET_FALLBACK_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteFallbackWidgetResult, ["not found"]),
        `delete fallback widget fixture failed: ${deleteFallbackWidgetResult?.error ?? deleteFallbackWidgetResult?.message ?? "unknown error"}`,
      );

      const widgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "create_widget_blueprint",
          name: "WBP_DesignerMarquee",
          folder: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(widgetResult, ["already exists"]),
        `create_widget_blueprint failed: ${widgetResult?.error ?? widgetResult?.message ?? "unknown error"}`,
      );

      if (
        typeof widgetResult?.widgetPath === "string" &&
        widgetResult.widgetPath.length > 0
      ) {
        widgetObjectPath = widgetResult.widgetPath;
        const objectSeparatorIndex = widgetObjectPath.indexOf(
          ".",
          widgetObjectPath.lastIndexOf("/"),
        );
        widgetAssetPath =
          objectSeparatorIndex > 0
            ? widgetObjectPath.slice(0, objectSeparatorIndex)
            : widgetObjectPath;
      }

      const fixtureRequests = [
        {
          action: "add_canvas_panel",
          widgetPath: widgetObjectPath,
          slotName: "RootCanvas",
        },
        {
          action: "add_border",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          parentSlot: "RootCanvas",
        },
        {
          action: "set_position",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          position: { x: 120, y: 120 },
        },
        {
          action: "set_size",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          size: { x: 180, y: 120 },
        },
        {
          action: "add_border",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          parentSlot: "RootCanvas",
        },
        {
          action: "set_position",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          position: { x: 420, y: 120 },
        },
        {
          action: "set_size",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          size: { x: 180, y: 120 },
        },
      ];

      for (const request of fixtureRequests) {
        const requestResult = unwrapAutomationResult(
          await tools.executeTool("manage_widget_authoring", request),
        );
        requireStep(
          requestResult?.success === true,
          `${request.action} failed: ${requestResult?.error ?? requestResult?.message ?? "unknown error"}`,
        );
      }

      const widgetTreeResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "get_widget_tree",
          widgetPath: widgetObjectPath,
        }),
      );
      requireStep(
        widgetTreeResult?.success === true,
        `get_widget_tree failed: ${widgetTreeResult?.error ?? widgetTreeResult?.message ?? "unknown error"}`,
      );

      const foundNames = findNamedWidgets(
        widgetTreeResult?.widgetTree,
        new Set([MARQUEE_WIDGET_A_SLOT, MARQUEE_WIDGET_B_SLOT]),
      );
      requireStep(
        foundNames.has(MARQUEE_WIDGET_A_SLOT) &&
          foundNames.has(MARQUEE_WIDGET_B_SLOT),
        `get_widget_tree did not expose both marquee fixture widgets: ${JSON.stringify(widgetTreeResult?.widgetTree ?? null)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer marquee: open the widget fixture and establish a live Designer surface",
    async (tools) => {
      const openResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: widgetAssetPath,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        openResult?.success === true,
        `open_asset failed for marquee fixture: ${openResult?.error ?? openResult?.message ?? "unknown error"}`,
      );

      const modeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "designer",
        }),
      );
      requireStep(
        modeResult?.success === true,
        `set_widget_blueprint_mode failed: ${modeResult?.error ?? modeResult?.message ?? "unknown error"}`,
      );

      const fitResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "fit_widget_designer",
          assetPath: widgetAssetPath,
        }),
      );
      requireStep(
        fitResult?.success === true,
        `fit_widget_designer failed: ${fitResult?.error ?? fitResult?.message ?? "unknown error"}`,
      );

      const viewResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_designer_view",
          assetPath: widgetAssetPath,
          viewLocation: { x: 0, y: 0 },
        }),
      );
      requireStep(
        viewResult?.success === true,
        `set_widget_designer_view failed: ${viewResult?.error ?? viewResult?.message ?? "unknown error"}`,
      );
      lastViewResult = viewResult;

      const stateResult = await getDesignerState(tools);
      requireStep(
        stateResult?.designerTabFound === true,
        `Expected designerTabFound=true, got ${stateResult?.designerTabFound}`,
      );
      requireStep(
        stateResult?.designerViewFound === true,
        `Expected designerViewFound=true, got ${stateResult?.designerViewFound}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer marquee: capture semantic widget paths and a single-selection baseline",
    async (tools) => {
      for (const widgetName of [MARQUEE_WIDGET_A_SLOT, MARQUEE_WIDGET_B_SLOT]) {
        const selectResult = unwrapAutomationResult(
          await tools.executeTool("control_editor", {
            action: "select_widget_in_designer",
            assetPath: widgetAssetPath,
            widgetName,
          }),
        );
        requireStep(
          selectResult?.success === true,
          `select_widget_in_designer failed for ${widgetName}: ${selectResult?.error ?? selectResult?.message ?? "unknown error"}`,
        );

        const stateResult = await getDesignerState(tools);
        requireStep(
          stateResult?.designerTabFound === true &&
            stateResult?.designerViewFound === true,
          `Designer state was not live after selecting ${widgetName}: ${JSON.stringify(stateResult)}`,
        );

        const selectedPaths = getSelectedWidgetPaths(stateResult);
        requireStep(
          getSelectedWidgetCount(stateResult) === 1 &&
            selectedPaths.length === 1,
          `Expected a single selected widget after selecting ${widgetName}: ${JSON.stringify(stateResult)}`,
        );

        if (widgetName === MARQUEE_WIDGET_A_SLOT) {
          expectedWidgetAPath = selectedPaths[0];
          expectedWidgetALayout = getWidgetLayoutByPath(
            stateResult,
            selectedPaths[0],
          );
        } else {
          expectedWidgetBPath = selectedPaths[0];
          expectedWidgetBLayout = getWidgetLayoutByPath(
            stateResult,
            selectedPaths[0],
          );
        }
      }

      requireStep(
        typeof expectedWidgetAPath === "string" &&
          expectedWidgetAPath.length > 0 &&
          typeof expectedWidgetBPath === "string" &&
          expectedWidgetBPath.length > 0,
        "Failed to resolve both marquee widget paths through semantic selection",
      );
      requireStep(
        expectedWidgetALayout !== null && expectedWidgetBLayout !== null,
        `Failed to capture geometry-aware marquee layouts during semantic selection: ${JSON.stringify(
          {
            expectedWidgetAPath,
            expectedWidgetALayout,
            expectedWidgetBPath,
            expectedWidgetBLayout,
          },
          null,
          2,
        )}`,
      );

      const reselectResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widget_in_designer",
          assetPath: widgetAssetPath,
          widgetName: MARQUEE_WIDGET_A_SLOT,
        }),
      );
      requireStep(
        reselectResult?.success === true,
        `Failed to restore the marquee baseline selection: ${reselectResult?.error ?? reselectResult?.message ?? "unknown error"}`,
      );

      const baselineState = await getDesignerState(tools);
      requireStep(
        baselineState?.designerTabFound === true &&
          baselineState?.designerViewFound === true,
        `Baseline designer state is not live: ${JSON.stringify(baselineState)}`,
      );

      const baselinePaths = getSelectedWidgetPaths(baselineState);
      requireStep(
        getSelectedWidgetCount(baselineState) === 1 &&
          baselinePaths.length === 1 &&
          baselinePaths.includes(expectedWidgetAPath),
        `Baseline selection did not narrow back to ${expectedWidgetAPath}: ${JSON.stringify(baselineState)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer marquee: discover the live Widget Blueprint window bounds",
    async (tools) => {
      const discoveryResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "list_visible_windows",
        }),
      );
      requireStep(
        discoveryResult?.success === true,
        `list_visible_windows failed: ${discoveryResult?.error ?? discoveryResult?.message ?? "unknown error"}`,
      );
      requireStep(
        Array.isArray(discoveryResult?.windows),
        "list_visible_windows did not return a windows array",
      );
      requireStep(
        Number(discoveryResult?.count ?? 0) > 0,
        "list_visible_windows returned no visible windows",
      );

      selectedWindow = pickDesignerMarqueeWindow(discoveryResult.windows);
      requireStep(
        selectedWindow !== null,
        "No suitable visible editor window was discovered for the designer marquee marquee fixture",
      );

      return true;
    },
  );

  runner.addStep(
    "Designer marquee: resolve the live designer target before low-level input",
    async (tools) => {
      requireStep(
        selectedWindow !== null,
        "No selected window available for live designer target resolution",
      );
      requireStep(
        typeof designerTabId === "string" && designerTabId.length > 0,
        "No live designer tab id was captured before low-level input",
      );

      const resolveResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "resolve_ui_target",
          tabId: designerTabId,
          windowTitle: selectedWindow.title,
        }),
      );
      requireStep(
        resolveResult?.success === true,
        `resolve_ui_target failed for marquee validation: ${resolveResult?.error ?? resolveResult?.message ?? "unknown error"}`,
      );
      requireStep(
        resolveResult?.targetStatus === "resolved",
        `resolve_ui_target did not resolve the designer tab cleanly: ${JSON.stringify(resolveResult)}`,
      );
      lastResolveResult = resolveResult;

      if (
        typeof resolveResult?.resolvedTabId === "string" &&
        resolveResult.resolvedTabId.length > 0
      ) {
        designerTabId = resolveResult.resolvedTabId;
      }

      selectedWindow = {
        ...(selectedWindow ?? {}),
        title:
          (typeof resolveResult?.resolvedWindowTitle === "string" &&
          resolveResult.resolvedWindowTitle.length > 0
            ? resolveResult.resolvedWindowTitle
            : typeof resolveResult?.windowTitle === "string" &&
                resolveResult.windowTitle.length > 0
              ? resolveResult.windowTitle
              : selectedWindow?.title) ?? null,
        x: Number.isFinite(Number(resolveResult?.x))
          ? Number(resolveResult.x)
          : Number(selectedWindow?.x ?? 0),
        y: Number.isFinite(Number(resolveResult?.y))
          ? Number(resolveResult.y)
          : Number(selectedWindow?.y ?? 0),
        width: Number.isFinite(Number(resolveResult?.width))
          ? Number(resolveResult.width)
          : Number(selectedWindow?.width ?? 0),
        height: Number.isFinite(Number(resolveResult?.height))
          ? Number(resolveResult.height)
          : Number(selectedWindow?.height ?? 0),
        clientX: Number.isFinite(Number(resolveResult?.clientX))
          ? Number(resolveResult.clientX)
          : Number(selectedWindow?.clientX ?? 0),
        clientY: Number.isFinite(Number(resolveResult?.clientY))
          ? Number(resolveResult.clientY)
          : Number(selectedWindow?.clientY ?? 0),
        clientWidth: Number.isFinite(Number(resolveResult?.clientWidth))
          ? Number(resolveResult.clientWidth)
          : Number(selectedWindow?.clientWidth ?? 0),
        clientHeight: Number.isFinite(Number(resolveResult?.clientHeight))
          ? Number(resolveResult.clientHeight)
          : Number(selectedWindow?.clientHeight ?? 0),
      };

      return true;
    },
  );

  runner.addStep(
    "Designer marquee: focus the widget designer surface before marquee drag",
    async (tools) => {
      requireStep(
        selectedWindow !== null,
        "No selected window available for widget designer focus",
      );

      const focusResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "focus_editor_surface",
          surface: "widget_designer",
          assetPath: widgetAssetPath,
          tabId: designerTabId ?? undefined,
          windowTitle: selectedWindow.title,
        }),
      );
      requireStep(
        focusResult?.success === true,
        `focus_editor_surface failed for marquee validation: ${focusResult?.error ?? focusResult?.message ?? "unknown error"}`,
      );
      requireStep(
        focusResult?.focusApplied === true,
        `focus_editor_surface did not report focusApplied=true: ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        focusResult?.focusTargetSurface === "widget_designer",
        `focus_editor_surface returned an unexpected surface: ${JSON.stringify(focusResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer marquee: drag a marquee and verify semantic multi-selection or bounded native ceiling",
    async (tools) => {
      requireStep(
        selectedWindow !== null,
        "No selected window available for marquee drag validation",
      );
      requireStep(
        typeof expectedWidgetAPath === "string" &&
          typeof expectedWidgetBPath === "string",
        "Marquee widget paths were not captured before the drag step",
      );

      const baselineState = await getDesignerState(tools);
      const baselinePaths = getSelectedWidgetPaths(baselineState);
      requireStep(
        getSelectedWidgetCount(baselineState) === 1 &&
          baselinePaths.includes(expectedWidgetAPath),
        `Expected the marquee baseline to remain a single ${expectedWidgetAPath} selection before dragging: ${JSON.stringify(baselineState)}`,
      );

      const dragTarget = buildDesignerMarqueeDrag(
        expectedWidgetALayout,
        expectedWidgetBLayout,
        selectedWindow,
      );
      requireStep(
        dragTarget !== null,
        `Geometry-aware marquee targeting could not derive drag bounds from get_widget_designer_state layout metadata. ${JSON.stringify(
          {
            geometrySource: "layout_metadata",
            widgetALayout: expectedWidgetALayout,
            widgetBLayout: expectedWidgetBLayout,
            dragWindowTitle: selectedWindow?.title,
          },
          null,
          2,
        )}`,
      );

      const dragResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "simulate_input",
            inputAction: "mouse_drag",
            assetPath: widgetAssetPath,
            tabId: designerTabId ?? undefined,
            windowTitle: selectedWindow.title,
            button: "left",
            captureScreenshots: false,
            start: dragTarget.start,
            end: dragTarget.end,
            durationMs: 220,
            holdBeforeMoveMs: 40,
            holdAfterMoveMs: 80,
            steps: 8,
          },
          { timeoutMs: 20000 },
        ),
      );
      const dragErrorDetail =
        typeof dragResult?.error === "string"
          ? dragResult.error
          : JSON.stringify(
              dragResult?.error ?? dragResult?.message ?? dragResult ?? null,
            );
      requireStep(
        dragResult?.success === true,
        `simulate_input mouse_drag failed: ${dragErrorDetail}`,
      );
      requireStep(
        dragResult?.resolvedTargetSource === "window_title" ||
          dragResult?.resolvedTargetSource === "tab_id",
        `Expected resolvedTargetSource to stay on a live window_title or tab_id target, got ${dragResult?.resolvedTargetSource}`,
      );
      requireStep(
        dragResult?.targetWidgetPathValid === true,
        `Expected targetWidgetPathValid=true after the marquee drag: ${JSON.stringify(dragResult)}`,
      );
      requireStep(
        dragResult?.targetWidgetPathSource === "preferred_widget",
        `Expected the marquee drag to stay on preferred_widget routing: ${JSON.stringify(
          {
            targetWidgetPathSource: dragResult?.targetWidgetPathSource,
            targetWidgetPath: dragResult?.targetWidgetPath,
            targetClientLeft: dragResult?.targetClientLeft,
            targetClientTop: dragResult?.targetClientTop,
            targetClientRight: dragResult?.targetClientRight,
            targetClientBottom: dragResult?.targetClientBottom,
            selectedWindow,
            dragStart: dragTarget.start,
            dragEnd: dragTarget.end,
          },
          null,
          2,
        )}`,
      );
      requireStep(
        typeof dragResult?.targetWidgetPath === "string" &&
          dragResult.targetWidgetPath.includes("SDesignerView"),
        `Expected the marquee drag to target SDesignerView: ${JSON.stringify(
          {
            dragResult,
            resolveResult: lastResolveResult,
            viewResult: lastViewResult,
            selectedWindow,
            dragStart: dragTarget.start,
            dragEnd: dragTarget.end,
            targetWidgetPathSource: dragResult?.targetWidgetPathSource,
            targetClientLeft: dragResult?.targetClientLeft,
            targetClientTop: dragResult?.targetClientTop,
            targetClientRight: dragResult?.targetClientRight,
            targetClientBottom: dragResult?.targetClientBottom,
            designerSurfaceDiagnostics:
              baselineState?.designerSurfaceDiagnostics ?? null,
            widgetALayout: dragTarget.widgetALayout,
            widgetBLayout: dragTarget.widgetBLayout,
          },
          null,
          2,
        )}`,
      );

      const postDragState = await getDesignerState(tools);
      const postDragPaths = getSelectedWidgetPaths(postDragState);
      const postDragCount = getSelectedWidgetCount(postDragState);
      const semanticPass =
        postDragCount > getSelectedWidgetCount(baselineState) &&
        postDragPaths.includes(expectedWidgetAPath) &&
        postDragPaths.includes(expectedWidgetBPath);

      if (semanticPass) {
        return true;
      }

      requireStep(
        postDragState?.designerTabFound === true &&
          postDragState?.designerViewFound === true,
        `Expected the bounded marquee branch to preserve live designer diagnostics: ${JSON.stringify(
          postDragState,
          null,
          2,
        )}`,
      );
      requireStep(
        dragTarget.geometrySource === "layout_metadata",
        `Expected the bounded marquee branch to stay geometry-backed: ${JSON.stringify(
          dragTarget,
          null,
          2,
        )}`,
      );
      requireStep(
        postDragCount === 1 &&
          postDragPaths.length === 1 &&
          postDragPaths[0] === "RootCanvas",
        `Expected the bounded marquee branch to collapse semantically to RootCanvas: ${JSON.stringify(
          {
            baselineSelectedWidgetCount: getSelectedWidgetCount(baselineState),
            baselineSelectedWidgets: baselinePaths,
            expectedSelectedWidgets: [expectedWidgetAPath, expectedWidgetBPath],
            postDragSelectedWidgetCount: postDragCount,
            postDragSelectedWidgets: postDragPaths,
            geometrySource: dragTarget.geometrySource,
            widgetALayout: dragTarget.widgetALayout,
            widgetBLayout: dragTarget.widgetBLayout,
            designerTabFound: postDragState?.designerTabFound,
            designerViewFound: postDragState?.designerViewFound,
            dragWindowTitle: selectedWindow?.title,
            dragStart: dragTarget.start,
            dragEnd: dragTarget.end,
            resolvedTargetSource: dragResult?.resolvedTargetSource,
            targetWidgetPathValid: dragResult?.targetWidgetPathValid,
            targetWidgetPathSource: dragResult?.targetWidgetPathSource,
            targetWidgetPath: dragResult?.targetWidgetPath,
            targetClientLeft: dragResult?.targetClientLeft,
            targetClientTop: dragResult?.targetClientTop,
            targetClientRight: dragResult?.targetClientRight,
            targetClientBottom: dragResult?.targetClientBottom,
            keyboardFocusedWidgetType: dragResult?.keyboardFocusedWidgetType,
            userFocusedWidgetType: dragResult?.userFocusedWidgetType,
          },
          null,
          2,
        )}`,
      );

      return {
        passed: true,
        detail: `Bounded native Designer ceiling verified. ${JSON.stringify(
          {
            baselineSelectedWidgetCount: getSelectedWidgetCount(baselineState),
            baselineSelectedWidgets: baselinePaths,
            expectedSelectedWidgets: [expectedWidgetAPath, expectedWidgetBPath],
            postDragSelectedWidgetCount: postDragCount,
            postDragSelectedWidgets: postDragPaths,
            geometrySource: dragTarget.geometrySource,
            targetWidgetPathSource: dragResult?.targetWidgetPathSource,
            targetWidgetPathValid: dragResult?.targetWidgetPathValid,
            targetReachedDesignerView:
              typeof dragResult?.targetWidgetPath === "string" &&
              dragResult.targetWidgetPath.includes("SDesignerView"),
            designerTabFound: postDragState?.designerTabFound,
            designerViewFound: postDragState?.designerViewFound,
            dragWindowTitle: selectedWindow?.title,
            dragStart: dragTarget.start,
            dragEnd: dragTarget.end,
          },
          null,
          2,
        )}`,
      };

      return true;
    },
  );

  runner.addStep(
    "Designer marquee: reset simulated input state",
    async (tools) => {
      const resetResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "simulate_input",
          inputAction: "reset",
        }),
      );

      requireStep(
        resetResult?.success === true,
        `simulate_input reset failed: ${resetResult?.error ?? resetResult?.message ?? "unknown error"}`,
      );
      return true;
    },
  );

  await runner.run();
}

async function runDesignerSelectionSuite() {
  const runner = new TestRunner("designer-selection");
  let widgetObjectPath = DESIGNER_MARQUEE_WIDGET_OBJECT;
  let widgetAssetPath = DESIGNER_MARQUEE_WIDGET_PACKAGE;
  let expectedWidgetAPath = null;
  let expectedWidgetBPath = null;

  async function getDesignerState(tools) {
    const stateResult = unwrapAutomationResult(
      await tools.executeTool(
        "manage_widget_authoring",
        {
          action: "get_widget_designer_state",
          widgetPath: widgetObjectPath,
          openEditorIfNeeded: false,
        },
        { timeoutMs: 15000 },
      ),
    );

    requireStep(
      stateResult?.success === true,
      `get_widget_designer_state failed: ${stateResult?.error ?? stateResult?.message ?? "unknown error"}`,
    );

    return stateResult;
  }

  runner.addStep(
    "Designer selection: ensure designer selection fixture folder exists",
    async (tools) => {
      const advancedFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advancedFolderResult, ["already exists"]),
        `create_folder failed for ${ADV_TEST_FOLDER}: ${advancedFolderResult?.error ?? advancedFolderResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer selection: create deterministic marquee-selection fixture",
    async (tools) => {
      const deleteWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: DESIGNER_MARQUEE_WIDGET_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteWidgetResult, ["not found"]),
        `delete widget fixture failed: ${deleteWidgetResult?.error ?? deleteWidgetResult?.message ?? "unknown error"}`,
      );

      const deleteFallbackWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: DESIGNER_MARQUEE_WIDGET_FALLBACK_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteFallbackWidgetResult, ["not found"]),
        `delete fallback widget fixture failed: ${deleteFallbackWidgetResult?.error ?? deleteFallbackWidgetResult?.message ?? "unknown error"}`,
      );

      const widgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "create_widget_blueprint",
          name: "WBP_DesignerMarquee",
          folder: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(widgetResult, ["already exists"]),
        `create_widget_blueprint failed: ${widgetResult?.error ?? widgetResult?.message ?? "unknown error"}`,
      );

      if (
        typeof widgetResult?.widgetPath === "string" &&
        widgetResult.widgetPath.length > 0
      ) {
        widgetObjectPath = widgetResult.widgetPath;
        const objectSeparatorIndex = widgetObjectPath.indexOf(
          ".",
          widgetObjectPath.lastIndexOf("/"),
        );
        widgetAssetPath =
          objectSeparatorIndex > 0
            ? widgetObjectPath.slice(0, objectSeparatorIndex)
            : widgetObjectPath;
      }

      const fixtureRequests = [
        {
          action: "add_canvas_panel",
          widgetPath: widgetObjectPath,
          slotName: "RootCanvas",
        },
        {
          action: "add_border",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          parentSlot: "RootCanvas",
        },
        {
          action: "set_position",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          position: { x: 120, y: 120 },
        },
        {
          action: "set_size",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          size: { x: 180, y: 120 },
        },
        {
          action: "add_border",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          parentSlot: "RootCanvas",
        },
        {
          action: "set_position",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          position: { x: 420, y: 120 },
        },
        {
          action: "set_size",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          size: { x: 180, y: 120 },
        },
      ];

      for (const request of fixtureRequests) {
        const requestResult = unwrapAutomationResult(
          await tools.executeTool("manage_widget_authoring", request),
        );
        requireStep(
          requestResult?.success === true,
          `${request.action} failed: ${requestResult?.error ?? requestResult?.message ?? "unknown error"}`,
        );
      }

      return true;
    },
  );

  runner.addStep(
    "Designer selection: open fixture in Designer mode and resolve widget paths",
    async (tools) => {
      const openResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: widgetAssetPath,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        openResult?.success === true,
        `open_asset failed: ${openResult?.error ?? openResult?.message ?? "unknown error"}`,
      );

      const modeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "designer",
        }),
      );
      requireStep(
        modeResult?.success === true,
        `set_widget_blueprint_mode failed: ${modeResult?.error ?? modeResult?.message ?? "unknown error"}`,
      );

      const fitResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "fit_widget_designer",
          assetPath: widgetAssetPath,
        }),
      );
      requireStep(
        fitResult?.success === true,
        `fit_widget_designer failed: ${fitResult?.error ?? fitResult?.message ?? "unknown error"}`,
      );

      for (const widgetName of [MARQUEE_WIDGET_A_SLOT, MARQUEE_WIDGET_B_SLOT]) {
        const selectResult = unwrapAutomationResult(
          await tools.executeTool("control_editor", {
            action: "select_widget_in_designer",
            assetPath: widgetAssetPath,
            widgetName,
          }),
        );
        requireStep(
          selectResult?.success === true,
          `select_widget_in_designer failed for ${widgetName}: ${selectResult?.error ?? selectResult?.message ?? "unknown error"}`,
        );

        const stateResult = await getDesignerState(tools);
        const selectedPaths = getSelectedWidgetPaths(stateResult);
        requireStep(
          getSelectedWidgetCount(stateResult) === 1 &&
            selectedPaths.length === 1,
          `Expected single selection after selecting ${widgetName}: ${JSON.stringify(stateResult)}`,
        );

        if (widgetName === MARQUEE_WIDGET_A_SLOT) {
          expectedWidgetAPath = selectedPaths[0];
        } else {
          expectedWidgetBPath = selectedPaths[0];
        }
      }

      requireStep(
        typeof expectedWidgetAPath === "string" &&
          typeof expectedWidgetBPath === "string",
        `Could not resolve semantic widget paths: ${JSON.stringify({ expectedWidgetAPath, expectedWidgetBPath })}`,
      );

      const baselineSelectResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widget_in_designer",
          assetPath: widgetAssetPath,
          widgetName: MARQUEE_WIDGET_A_SLOT,
        }),
      );
      requireStep(
        baselineSelectResult?.success === true,
        `Failed to restore baseline widget A selection: ${baselineSelectResult?.error ?? baselineSelectResult?.message ?? "unknown error"}`,
      );

      const baselineState = await getDesignerState(tools);
      const baselinePaths = getSelectedWidgetPaths(baselineState);
      requireStep(
        getSelectedWidgetCount(baselineState) === 1 &&
          baselinePaths.includes(expectedWidgetAPath),
        `Baseline selection did not narrow back to widget A: ${JSON.stringify(baselineState)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer selection: append widget B semantically without ctrl-click",
    async (tools) => {
      const appendResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widget_in_designer",
          assetPath: widgetAssetPath,
          widgetName: MARQUEE_WIDGET_B_SLOT,
          appendOrToggle: true,
        }),
      );
      requireStep(
        appendResult?.success === true,
        `append selection failed: ${appendResult?.error ?? appendResult?.message ?? "unknown error"}`,
      );
      requireStep(
        appendResult?.appendOrToggle === true,
        `append selection did not preserve appendOrToggle diagnostics: ${JSON.stringify(appendResult)}`,
      );
      requireStep(
        appendResult?.targetStillSelected === true,
        `append selection did not leave the target selected: ${JSON.stringify(appendResult)}`,
      );

      const appendState = await getDesignerState(tools);
      const appendPaths = getSelectedWidgetPaths(appendState);
      requireStep(
        getSelectedWidgetCount(appendState) === 2 &&
          appendPaths.includes(expectedWidgetAPath) &&
          appendPaths.includes(expectedWidgetBPath),
        `Semantic append selection did not produce the expected multi-selection: ${JSON.stringify(appendState)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer selection: toggle widget B off semantically and keep widget A selected",
    async (tools) => {
      const toggleResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widget_in_designer",
          assetPath: widgetAssetPath,
          widgetName: MARQUEE_WIDGET_B_SLOT,
          appendOrToggle: true,
        }),
      );
      requireStep(
        toggleResult?.success === true,
        `toggle selection failed: ${toggleResult?.error ?? toggleResult?.message ?? "unknown error"}`,
      );
      requireStep(
        toggleResult?.appendOrToggle === true,
        `toggle selection did not preserve appendOrToggle diagnostics: ${JSON.stringify(toggleResult)}`,
      );
      requireStep(
        toggleResult?.targetStillSelected === false,
        `toggle selection did not clear the target widget: ${JSON.stringify(toggleResult)}`,
      );
      requireStep(
        Number(toggleResult?.selectedWidgetCount ?? NaN) === 1,
        `toggle selection did not report the narrowed selection count: ${JSON.stringify(toggleResult)}`,
      );

      const toggleState = await getDesignerState(tools);
      const togglePaths = getSelectedWidgetPaths(toggleState);
      requireStep(
        getSelectedWidgetCount(toggleState) === 1 &&
          togglePaths.includes(expectedWidgetAPath) &&
          !togglePaths.includes(expectedWidgetBPath),
        `Semantic toggle did not leave only widget A selected: ${JSON.stringify(toggleState)}`,
      );

      return true;
    },
  );

  await runner.run();
}

async function runDesignerGeometryReadbackSuite() {
  const runner = new TestRunner("designer-geometry-readback");
  let widgetObjectPath = DESIGNER_MARQUEE_WIDGET_OBJECT;
  let widgetAssetPath = DESIGNER_MARQUEE_WIDGET_PACKAGE;

  async function getDesignerState(tools) {
    const stateResult = unwrapAutomationResult(
      await tools.executeTool(
        "manage_widget_authoring",
        {
          action: "get_widget_designer_state",
          widgetPath: widgetObjectPath,
          openEditorIfNeeded: false,
        },
        { timeoutMs: 15000 },
      ),
    );

    requireStep(
      stateResult?.success === true,
      `get_widget_designer_state failed: ${stateResult?.error ?? stateResult?.message ?? "unknown error"}`,
    );

    return stateResult;
  }

  runner.addStep(
    "Designer tooling: ensure geometry fixture folder exists",
    async (tools) => {
      const advancedFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advancedFolderResult, ["already exists"]),
        `create_folder failed for ${ADV_TEST_FOLDER}: ${advancedFolderResult?.error ?? advancedFolderResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer tooling: create deterministic marquee fixture for full-tree geometry readback",
    async (tools) => {
      const deleteWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: DESIGNER_MARQUEE_WIDGET_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteWidgetResult, ["not found"]),
        `delete widget fixture failed: ${deleteWidgetResult?.error ?? deleteWidgetResult?.message ?? "unknown error"}`,
      );

      const deleteFallbackWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: DESIGNER_MARQUEE_WIDGET_FALLBACK_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteFallbackWidgetResult, ["not found"]),
        `delete fallback widget fixture failed: ${deleteFallbackWidgetResult?.error ?? deleteFallbackWidgetResult?.message ?? "unknown error"}`,
      );

      const widgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "create_widget_blueprint",
          name: "WBP_DesignerMarquee",
          folder: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(widgetResult, ["already exists"]),
        `create_widget_blueprint failed: ${widgetResult?.error ?? widgetResult?.message ?? "unknown error"}`,
      );

      if (
        typeof widgetResult?.widgetPath === "string" &&
        widgetResult.widgetPath.length > 0
      ) {
        widgetObjectPath = widgetResult.widgetPath;
        const objectSeparatorIndex = widgetObjectPath.indexOf(
          ".",
          widgetObjectPath.lastIndexOf("/"),
        );
        widgetAssetPath =
          objectSeparatorIndex > 0
            ? widgetObjectPath.slice(0, objectSeparatorIndex)
            : widgetObjectPath;
      }

      const fixtureRequests = [
        {
          action: "add_canvas_panel",
          widgetPath: widgetObjectPath,
          slotName: "RootCanvas",
        },
        {
          action: "add_border",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          parentSlot: "RootCanvas",
        },
        {
          action: "set_position",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          position: { x: 120, y: 120 },
        },
        {
          action: "set_size",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          size: { x: 180, y: 120 },
        },
        {
          action: "add_border",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          parentSlot: "RootCanvas",
        },
        {
          action: "set_position",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          position: { x: 420, y: 120 },
        },
        {
          action: "set_size",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          size: { x: 180, y: 120 },
        },
      ];

      for (const request of fixtureRequests) {
        const requestResult = unwrapAutomationResult(
          await tools.executeTool("manage_widget_authoring", request),
        );
        requireStep(
          requestResult?.success === true,
          `${request.action} failed: ${requestResult?.error ?? requestResult?.message ?? "unknown error"}`,
        );
      }

      return true;
    },
  );

  runner.addStep(
    "Designer tooling: expose live designer bounds on widgetTree without preselecting the marquee widgets",
    async (tools) => {
      const openResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: widgetAssetPath,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        openResult?.success === true,
        `open_asset failed: ${openResult?.error ?? openResult?.message ?? "unknown error"}`,
      );

      const modeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "designer",
        }),
      );
      requireStep(
        modeResult?.success === true,
        `set_widget_blueprint_mode failed: ${modeResult?.error ?? modeResult?.message ?? "unknown error"}`,
      );

      const fitResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "fit_widget_designer",
          assetPath: widgetAssetPath,
        }),
      );
      requireStep(
        fitResult?.success === true,
        `fit_widget_designer failed: ${fitResult?.error ?? fitResult?.message ?? "unknown error"}`,
      );

      const stateResult = await getDesignerState(tools);
      const widgetATreeLayout = getWidgetTreeLayoutByPath(
        stateResult,
        "RootCanvas/MarqueeA",
      );
      const widgetBTreeLayout = getWidgetTreeLayoutByPath(
        stateResult,
        "RootCanvas/MarqueeB",
      );

      requireStep(
        getWidgetLayoutBounds(widgetATreeLayout),
        `widgetTree did not expose live designerBounds for MarqueeA: ${JSON.stringify(stateResult?.widgetTree ?? null)}`,
      );
      requireStep(
        getWidgetLayoutBounds(widgetBTreeLayout),
        `widgetTree did not expose live designerBounds for MarqueeB: ${JSON.stringify(stateResult?.widgetTree ?? null)}`,
      );

      return true;
    },
  );

  await runner.run();
}

async function runDesignerRectangleSelectionSuite() {
  const runner = new TestRunner("designer-rectangle-selection");
  let widgetObjectPath = DESIGNER_MARQUEE_WIDGET_OBJECT;
  let widgetAssetPath = DESIGNER_MARQUEE_WIDGET_PACKAGE;
  let expectedWidgetAPath = null;
  let expectedWidgetBPath = null;
  let unionRect = null;
  let widgetBRect = null;

  async function getDesignerState(tools) {
    const stateResult = unwrapAutomationResult(
      await tools.executeTool(
        "manage_widget_authoring",
        {
          action: "get_widget_designer_state",
          widgetPath: widgetObjectPath,
          openEditorIfNeeded: false,
        },
        { timeoutMs: 15000 },
      ),
    );

    requireStep(
      stateResult?.success === true,
      `get_widget_designer_state failed: ${stateResult?.error ?? stateResult?.message ?? "unknown error"}`,
    );

    return stateResult;
  }

  runner.addStep(
    "Designer tooling: ensure rectangle-selection fixture folder exists",
    async (tools) => {
      const advancedFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advancedFolderResult, ["already exists"]),
        `create_folder failed for ${ADV_TEST_FOLDER}: ${advancedFolderResult?.error ?? advancedFolderResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer tooling: create deterministic marquee-selection fixture",
    async (tools) => {
      const deleteWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: DESIGNER_MARQUEE_WIDGET_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteWidgetResult, ["not found"]),
        `delete widget fixture failed: ${deleteWidgetResult?.error ?? deleteWidgetResult?.message ?? "unknown error"}`,
      );

      const deleteFallbackWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: DESIGNER_MARQUEE_WIDGET_FALLBACK_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteFallbackWidgetResult, ["not found"]),
        `delete fallback widget fixture failed: ${deleteFallbackWidgetResult?.error ?? deleteFallbackWidgetResult?.message ?? "unknown error"}`,
      );

      const widgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "create_widget_blueprint",
          name: "WBP_DesignerMarquee",
          folder: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(widgetResult, ["already exists"]),
        `create_widget_blueprint failed: ${widgetResult?.error ?? widgetResult?.message ?? "unknown error"}`,
      );

      if (
        typeof widgetResult?.widgetPath === "string" &&
        widgetResult.widgetPath.length > 0
      ) {
        widgetObjectPath = widgetResult.widgetPath;
        const objectSeparatorIndex = widgetObjectPath.indexOf(
          ".",
          widgetObjectPath.lastIndexOf("/"),
        );
        widgetAssetPath =
          objectSeparatorIndex > 0
            ? widgetObjectPath.slice(0, objectSeparatorIndex)
            : widgetObjectPath;
      }

      const fixtureRequests = [
        {
          action: "add_canvas_panel",
          widgetPath: widgetObjectPath,
          slotName: "RootCanvas",
        },
        {
          action: "add_border",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          parentSlot: "RootCanvas",
        },
        {
          action: "set_position",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          position: { x: 120, y: 120 },
        },
        {
          action: "set_size",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_A_SLOT,
          size: { x: 180, y: 120 },
        },
        {
          action: "add_border",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          parentSlot: "RootCanvas",
        },
        {
          action: "set_position",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          position: { x: 420, y: 120 },
        },
        {
          action: "set_size",
          widgetPath: widgetObjectPath,
          slotName: MARQUEE_WIDGET_B_SLOT,
          size: { x: 180, y: 120 },
        },
      ];

      for (const request of fixtureRequests) {
        const requestResult = unwrapAutomationResult(
          await tools.executeTool("manage_widget_authoring", request),
        );
        requireStep(
          requestResult?.success === true,
          `${request.action} failed: ${requestResult?.error ?? requestResult?.message ?? "unknown error"}`,
        );
      }

      return true;
    },
  );

  runner.addStep(
    "Designer tooling: open fixture in Designer mode and derive rectangle geometry",
    async (tools) => {
      const openResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "open_asset",
            assetPath: widgetAssetPath,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        openResult?.success === true,
        `open_asset failed: ${openResult?.error ?? openResult?.message ?? "unknown error"}`,
      );

      const modeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "designer",
        }),
      );
      requireStep(
        modeResult?.success === true,
        `set_widget_blueprint_mode failed: ${modeResult?.error ?? modeResult?.message ?? "unknown error"}`,
      );

      const fitResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "fit_widget_designer",
          assetPath: widgetAssetPath,
        }),
      );
      requireStep(
        fitResult?.success === true,
        `fit_widget_designer failed: ${fitResult?.error ?? fitResult?.message ?? "unknown error"}`,
      );

      for (const widgetName of [MARQUEE_WIDGET_A_SLOT, MARQUEE_WIDGET_B_SLOT]) {
        const selectResult = unwrapAutomationResult(
          await tools.executeTool("control_editor", {
            action: "select_widget_in_designer",
            assetPath: widgetAssetPath,
            widgetName,
          }),
        );
        requireStep(
          selectResult?.success === true,
          `select_widget_in_designer failed for ${widgetName}: ${selectResult?.error ?? selectResult?.message ?? "unknown error"}`,
        );

        const stateResult = await getDesignerState(tools);
        const selectedPaths = getSelectedWidgetPaths(stateResult);
        requireStep(
          getSelectedWidgetCount(stateResult) === 1 &&
            selectedPaths.length === 1,
          `Expected single selection after selecting ${widgetName}: ${JSON.stringify(stateResult)}`,
        );

        if (widgetName === MARQUEE_WIDGET_A_SLOT) {
          expectedWidgetAPath = selectedPaths[0];
        } else {
          expectedWidgetBPath = selectedPaths[0];
        }
      }

      const stateResult = await getDesignerState(tools);
      const widgetALayout = getWidgetTreeLayoutByPath(
        stateResult,
        expectedWidgetAPath,
      );
      const widgetBLayout = getWidgetTreeLayoutByPath(
        stateResult,
        expectedWidgetBPath,
      );

      requireStep(
        widgetALayout && widgetBLayout,
        `Could not resolve widgetTree layouts for rectangle selection: ${JSON.stringify(stateResult?.widgetTree ?? null)}`,
      );

      unionRect = buildDesignerRectFromLayouts(widgetALayout, widgetBLayout);
      widgetBRect = buildDesignerRectFromLayout(widgetBLayout);

      requireStep(
        unionRect && widgetBRect,
        `Could not derive semantic selection rectangles: ${JSON.stringify({ widgetALayout, widgetBLayout })}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer tooling: replace selection with one semantic rectangle covering both marquee widgets",
    async (tools) => {
      requireStep(
        unionRect,
        "No union rectangle prepared for rectangle selection proof",
      );

      const baselineSelectResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widget_in_designer",
          assetPath: widgetAssetPath,
          widgetName: MARQUEE_WIDGET_A_SLOT,
        }),
      );
      requireStep(
        baselineSelectResult?.success === true,
        `Failed to restore baseline widget A selection: ${baselineSelectResult?.error ?? baselineSelectResult?.message ?? "unknown error"}`,
      );

      const rectResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widgets_in_designer_rect",
          assetPath: widgetAssetPath,
          rect: unionRect,
        }),
      );
      requireStep(
        rectResult?.success === true,
        `select_widgets_in_designer_rect failed: ${rectResult?.error ?? rectResult?.message ?? "unknown error"}`,
      );
      requireStep(
        Number(rectResult?.matchedWidgetCount ?? NaN) === 2,
        `Rectangle selection did not report both marquee widgets: ${JSON.stringify(rectResult)}`,
      );
      requireStep(
        Array.isArray(rectResult?.matchedWidgetPaths) &&
          rectResult.matchedWidgetPaths.includes(expectedWidgetAPath) &&
          rectResult.matchedWidgetPaths.includes(expectedWidgetBPath),
        `Rectangle selection did not report the expected matched widget paths: ${JSON.stringify(rectResult)}`,
      );

      const stateResult = await getDesignerState(tools);
      const selectedPaths = getSelectedWidgetPaths(stateResult);
      requireStep(
        getSelectedWidgetCount(stateResult) === 2 &&
          selectedPaths.includes(expectedWidgetAPath) &&
          selectedPaths.includes(expectedWidgetBPath),
        `Rectangle replace selection did not produce both marquee widgets: ${JSON.stringify(stateResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Designer tooling: append and then toggle widget B with a single-widget semantic rectangle",
    async (tools) => {
      requireStep(
        widgetBRect,
        "No single-widget rectangle prepared for widget B",
      );

      const baselineSelectResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widget_in_designer",
          assetPath: widgetAssetPath,
          widgetName: MARQUEE_WIDGET_A_SLOT,
        }),
      );
      requireStep(
        baselineSelectResult?.success === true,
        `Failed to restore baseline widget A selection: ${baselineSelectResult?.error ?? baselineSelectResult?.message ?? "unknown error"}`,
      );

      const appendResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widgets_in_designer_rect",
          assetPath: widgetAssetPath,
          rect: widgetBRect,
          appendOrToggle: true,
        }),
      );
      requireStep(
        appendResult?.success === true,
        `append rectangle selection failed: ${appendResult?.error ?? appendResult?.message ?? "unknown error"}`,
      );
      requireStep(
        Number(appendResult?.matchedWidgetCount ?? NaN) === 1 &&
          Array.isArray(appendResult?.matchedWidgetPaths) &&
          appendResult.matchedWidgetPaths.includes(expectedWidgetBPath),
        `append rectangle selection did not isolate widget B: ${JSON.stringify(appendResult)}`,
      );

      const appendState = await getDesignerState(tools);
      const appendPaths = getSelectedWidgetPaths(appendState);
      requireStep(
        getSelectedWidgetCount(appendState) === 2 &&
          appendPaths.includes(expectedWidgetAPath) &&
          appendPaths.includes(expectedWidgetBPath),
        `append rectangle selection did not produce additive selection: ${JSON.stringify(appendState)}`,
      );

      const toggleResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "select_widgets_in_designer_rect",
          assetPath: widgetAssetPath,
          rect: widgetBRect,
          appendOrToggle: true,
        }),
      );
      requireStep(
        toggleResult?.success === true,
        `toggle rectangle selection failed: ${toggleResult?.error ?? toggleResult?.message ?? "unknown error"}`,
      );

      const toggleState = await getDesignerState(tools);
      const togglePaths = getSelectedWidgetPaths(toggleState);
      requireStep(
        getSelectedWidgetCount(toggleState) === 1 &&
          togglePaths.includes(expectedWidgetAPath) &&
          !togglePaths.includes(expectedWidgetBPath),
        `toggle rectangle selection did not leave only widget A selected: ${JSON.stringify(toggleState)}`,
      );

      return true;
    },
  );

  await runner.run();
}

async function runUiTargetingSuite() {
  const runner = new TestRunner("ui-targeting");
  let widgetObjectPath = UI_TARGETING_WIDGET_OBJECT;
  let widgetAssetPath = UI_TARGETING_WIDGET_PACKAGE;
  let selectedWindow = null;
  let resolveResult = null;

  runner.addStep(
    "UI targeting: ensure the targeting fixture exists",
    async (tools) => {
      const advancedFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advancedFolderResult, ["already exists"]),
        `create_folder failed for ${ADV_TEST_FOLDER}: ${advancedFolderResult?.error ?? advancedFolderResult?.message ?? "unknown error"}`,
      );

      const deleteWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: UI_TARGETING_WIDGET_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteWidgetResult, ["not found"]),
        `delete widget fixture failed: ${deleteWidgetResult?.error ?? deleteWidgetResult?.message ?? "unknown error"}`,
      );

      const deleteFallbackWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: UI_TARGETING_WIDGET_FALLBACK_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteFallbackWidgetResult, ["not found"]),
        `delete fallback widget fixture failed: ${deleteFallbackWidgetResult?.error ?? deleteFallbackWidgetResult?.message ?? "unknown error"}`,
      );

      const widgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "create_widget_blueprint",
          name: "WBP_UiTargeting",
          folder: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(widgetResult, ["already exists"]),
        `create_widget_blueprint failed: ${widgetResult?.error ?? widgetResult?.message ?? "unknown error"}`,
      );

      if (
        typeof widgetResult?.widgetPath === "string" &&
        widgetResult.widgetPath.length > 0
      ) {
        widgetObjectPath = widgetResult.widgetPath;
        const objectSeparatorIndex = widgetObjectPath.indexOf(
          ".",
          widgetObjectPath.lastIndexOf("/"),
        );
        widgetAssetPath =
          objectSeparatorIndex > 0
            ? widgetObjectPath.slice(0, objectSeparatorIndex)
            : widgetObjectPath;
      }

      const canvasResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "add_canvas_panel",
          widgetPath: widgetObjectPath,
          slotName: "RootCanvas",
        }),
      );
      requireStep(
        canvasResult?.success === true,
        `add_canvas_panel failed: ${canvasResult?.error ?? canvasResult?.message ?? "unknown error"}`,
      );

      const commandResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "register_editor_command",
          name: UI_TARGETING_WIDGET_COMMAND,
          label: "UI Targeting Widget Designer",
          kind: "open_asset",
          assetPath: widgetAssetPath,
          tabId: UI_TARGETING_WIDGET_TAB_ID,
        }),
      );
      requireStep(
        commandResult?.success === true,
        `register_editor_command failed: ${commandResult?.error ?? commandResult?.message ?? "unknown error"}`,
      );
      requireStep(
        commandResult?.name === UI_TARGETING_WIDGET_COMMAND,
        `register_editor_command did not preserve the targeting command name: ${JSON.stringify(commandResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI targeting: prove recovery before the designer surface is live",
    async (tools) => {
      resolveResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "resolve_ui_target",
          identifier: UI_TARGETING_WIDGET_COMMAND,
          tabId: UI_TARGETING_WIDGET_TAB_ID,
          windowTitle: UI_TARGETING_PREOPEN_WINDOW_HINT,
        }),
      );

      requireStep(
        resolveResult?.success === true,
        `pre-open resolve_ui_target failed: ${resolveResult?.error ?? resolveResult?.message ?? "unknown error"}`,
      );
      requireStep(
        resolveResult?.targetStatus === "needs_open" ||
          resolveResult?.targetStatus === "stale",
        `Expected a recovery status before opening the target, got ${JSON.stringify(resolveResult)}`,
      );
      requireStep(
        resolveResult?.targetStatus !== "resolved",
        `Pre-open resolve_ui_target unexpectedly reported a live target: ${JSON.stringify(resolveResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI targeting: open the target through the public UI seam",
    async (tools) => {
      const openTargetResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "open_ui_target",
          identifier: UI_TARGETING_WIDGET_COMMAND,
        }),
      );

      requireStep(
        openTargetResult?.success === true,
        `open_ui_target failed: ${openTargetResult?.error ?? openTargetResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI targeting: switch the widget asset into Designer mode",
    async (tools) => {
      const modeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "designer",
        }),
      );
      requireStep(
        modeResult?.success === true,
        `set_widget_blueprint_mode failed: ${modeResult?.error ?? modeResult?.message ?? "unknown error"}`,
      );

      const designerState = unwrapAutomationResult(
        await tools.executeTool(
          "manage_widget_authoring",
          {
            action: "get_widget_designer_state",
            widgetPath: widgetObjectPath,
            openEditorIfNeeded: false,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        designerState?.success === true,
        `get_widget_designer_state failed: ${designerState?.error ?? designerState?.message ?? "unknown error"}`,
      );
      requireStep(
        designerState?.designerTabFound === true &&
          designerState?.designerViewFound === true,
        `Widget Designer did not become live after switching modes: ${JSON.stringify(designerState)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI targeting: discover the live widget window and re-resolve the target",
    async (tools) => {
      const discoveryResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "list_visible_windows",
        }),
      );
      requireStep(
        discoveryResult?.success === true,
        `list_visible_windows failed: ${discoveryResult?.error ?? discoveryResult?.message ?? "unknown error"}`,
      );
      requireStep(
        Array.isArray(discoveryResult?.windows),
        "list_visible_windows did not return a windows array",
      );

      selectedWindow = pickUiTargetingWindow(discoveryResult.windows);
      requireStep(
        selectedWindow !== null,
        `No suitable live UI targeting widget window was discovered: ${JSON.stringify(discoveryResult?.windows ?? [])}`,
      );

      const liveResolveResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "resolve_ui_target",
          identifier: UI_TARGETING_WIDGET_COMMAND,
          tabId: UI_TARGETING_WIDGET_TAB_ID,
          windowTitle: selectedWindow.title,
        }),
      );
      requireStep(
        liveResolveResult?.success === true,
        `live resolve_ui_target failed: ${liveResolveResult?.error ?? liveResolveResult?.message ?? "unknown error"}`,
      );
      requireStep(
        liveResolveResult?.targetStatus === "resolved",
        `Expected the live widget target to resolve cleanly, got ${JSON.stringify(liveResolveResult)}`,
      );
      requireStep(
        matchesResolvedWindow(
          selectedWindow.title,
          liveResolveResult?.windowTitle ??
            liveResolveResult?.resolvedWindowTitle,
        ),
        `Resolved target window did not match the discovered widget window: ${JSON.stringify(liveResolveResult)}`,
      );

      resolveResult = liveResolveResult;
      return true;
    },
  );

  runner.addStep(
    "UI targeting: focus the widget designer surface explicitly",
    async (tools) => {
      requireStep(
        selectedWindow !== null,
        "No selected window available for focus_editor_surface verification",
      );

      const focusResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "focus_editor_surface",
          surface: "widget_designer",
          assetPath: widgetAssetPath,
          tabId:
            resolveResult?.resolvedTabId ??
            resolveResult?.tabId ??
            UI_TARGETING_WIDGET_TAB_ID,
          windowTitle: selectedWindow.title,
        }),
      );

      requireStep(
        focusResult?.success === true,
        `focus_editor_surface failed: ${focusResult?.error ?? focusResult?.message ?? "unknown error"}`,
      );
      requireStep(
        focusResult?.focusApplied === true,
        `focus_editor_surface did not report focusApplied=true: ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        focusResult?.focusTargetSurface === "widget_designer",
        `focus_editor_surface returned an unexpected surface: ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        focusResult?.targetStatus === "resolved",
        `focus_editor_surface did not preserve resolved target diagnostics: ${JSON.stringify(focusResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI targeting: capture an explicitly targeted editor screenshot without warnings",
    async (tools) => {
      requireStep(
        selectedWindow !== null,
        "No selected window available for explicit screenshot verification",
      );

      const screenshotResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "screenshot",
            filename: "ui-targeting-window.png",
            mode: "editor",
            windowTitle: selectedWindow.title,
            includeMenus: false,
          },
          { timeoutMs: 20000 },
        ),
      );
      requireStep(
        screenshotResult?.success === true,
        `targeted screenshot failed: ${screenshotResult?.error ?? screenshotResult?.message ?? "unknown error"}`,
      );
      requireStep(
        screenshotResult?.captureTarget === "editor_window",
        `Expected captureTarget=editor_window, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotResult?.captureIntentSource === "requested_window_title",
        `Expected captureIntentSource=requested_window_title, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        typeof screenshotResult?.captureIntentWarning !== "string" ||
          screenshotResult.captureIntentWarning.length === 0,
        `Explicit screenshot unexpectedly emitted a warning: ${JSON.stringify(screenshotResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI targeting: preserve screenshot ambiguity diagnostics after re-resolution when only tabId is retried",
    async (tools) => {
      const screenshotResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "screenshot",
            filename: "ui-targeting-ambiguous-warning.png",
            tabId:
              resolveResult?.resolvedTabId ??
              resolveResult?.tabId ??
              UI_TARGETING_WIDGET_TAB_ID,
          },
          { timeoutMs: 20000 },
        ),
      );
      const screenshotErrorDetails =
        screenshotResult?.error && typeof screenshotResult.error === "object"
          ? screenshotResult.error
          : screenshotResult;

      requireStep(
        screenshotResult?.success === false,
        `Expected tabId-only screenshot retry to fail explicitly, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotResult?.errorCode === "AMBIGUOUS_CAPTURE_TARGET",
        `Expected tabId-only screenshot retry to return AMBIGUOUS_CAPTURE_TARGET, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        typeof screenshotErrorDetails?.captureIntentWarning === "string" &&
          /tabid/i.test(screenshotErrorDetails.captureIntentWarning) &&
          /windowtitle|resolve_ui_target/i.test(
            screenshotErrorDetails.captureIntentWarning,
          ),
        `Ambiguous screenshot did not surface the expected warning: ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.captureIntentSource === "viewport_default",
        `TabId-only screenshot retry did not preserve captureIntentSource=viewport_default: ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.suggestedPreflightAction ===
          "resolve_ui_target",
        `TabId-only screenshot retry did not surface resolve_ui_target guidance: ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.targetStatus === "stale",
        `TabId-only screenshot retry did not preserve stale target diagnostics: ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.requestedTargetStillLive === false,
        `TabId-only screenshot retry did not preserve requestedTargetStillLive=false: ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.captureTarget !== "viewport" &&
          screenshotErrorDetails?.captureTarget !== "editor_window",
        `TabId-only screenshot retry must not claim a capture target: ${JSON.stringify(screenshotResult)}`,
      );

      return true;
    },
  );

  await runner.run();
}

async function runUiTargetPolicySuite() {
  const runner = new TestRunner("ui-target-policy");
  let widgetObjectPath = UI_TARGETING_WIDGET_OBJECT;
  let widgetAssetPath = UI_TARGETING_WIDGET_PACKAGE;
  let selectedWindow = null;

  runner.addStep(
    "UI target policy: ensure the targeting fixture exists",
    async (tools) => {
      const advancedFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advancedFolderResult, ["already exists"]),
        `create_folder failed for ${ADV_TEST_FOLDER}: ${advancedFolderResult?.error ?? advancedFolderResult?.message ?? "unknown error"}`,
      );

      const deleteWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: UI_TARGETING_WIDGET_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteWidgetResult, ["not found"]),
        `delete widget fixture failed: ${deleteWidgetResult?.error ?? deleteWidgetResult?.message ?? "unknown error"}`,
      );

      const deleteFallbackWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: UI_TARGETING_WIDGET_FALLBACK_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteFallbackWidgetResult, ["not found"]),
        `delete fallback widget fixture failed: ${deleteFallbackWidgetResult?.error ?? deleteFallbackWidgetResult?.message ?? "unknown error"}`,
      );

      const widgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "create_widget_blueprint",
          name: "WBP_UiTargeting",
          folder: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(widgetResult, ["already exists"]),
        `create_widget_blueprint failed: ${widgetResult?.error ?? widgetResult?.message ?? "unknown error"}`,
      );

      if (
        typeof widgetResult?.widgetPath === "string" &&
        widgetResult.widgetPath.length > 0
      ) {
        widgetObjectPath = widgetResult.widgetPath;
        const objectSeparatorIndex = widgetObjectPath.indexOf(
          ".",
          widgetObjectPath.lastIndexOf("/"),
        );
        widgetAssetPath =
          objectSeparatorIndex > 0
            ? widgetObjectPath.slice(0, objectSeparatorIndex)
            : widgetObjectPath;
      }

      const canvasResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "add_canvas_panel",
          widgetPath: widgetObjectPath,
          slotName: "RootCanvas",
        }),
      );
      requireStep(
        canvasResult?.success === true,
        `add_canvas_panel failed: ${canvasResult?.error ?? canvasResult?.message ?? "unknown error"}`,
      );

      const commandResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "register_editor_command",
          name: UI_TARGETING_WIDGET_COMMAND,
          label: "UI Targeting Widget Designer",
          kind: "open_asset",
          assetPath: widgetAssetPath,
          tabId: UI_TARGETING_WIDGET_TAB_ID,
        }),
      );
      requireStep(
        commandResult?.success === true,
        `register_editor_command failed: ${commandResult?.error ?? commandResult?.message ?? "unknown error"}`,
      );
      requireStep(
        commandResult?.name === UI_TARGETING_WIDGET_COMMAND,
        `register_editor_command did not preserve the targeting command name: ${JSON.stringify(commandResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI target policy: prove recovery before the designer surface is live",
    async (tools) => {
      const preOpenResolveResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "resolve_ui_target",
          identifier: UI_TARGETING_WIDGET_COMMAND,
          tabId: UI_TARGETING_WIDGET_TAB_ID,
          windowTitle: UI_TARGETING_PREOPEN_WINDOW_HINT,
        }),
      );

      requireStep(
        preOpenResolveResult?.success === true,
        `pre-open resolve_ui_target failed: ${preOpenResolveResult?.error ?? preOpenResolveResult?.message ?? "unknown error"}`,
      );
      requireStep(
        preOpenResolveResult?.targetStatus === "needs_open" ||
          preOpenResolveResult?.targetStatus === "stale",
        `Expected a recovery status before opening the target, got ${JSON.stringify(preOpenResolveResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI target policy: open the target through the public UI seam",
    async (tools) => {
      const openTargetResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "open_ui_target",
          identifier: UI_TARGETING_WIDGET_COMMAND,
        }),
      );

      requireStep(
        openTargetResult?.success === true,
        `open_ui_target failed: ${openTargetResult?.error ?? openTargetResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI target policy: switch the widget asset into Designer mode",
    async (tools) => {
      const modeResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "set_widget_blueprint_mode",
          assetPath: widgetAssetPath,
          mode: "designer",
        }),
      );
      requireStep(
        modeResult?.success === true,
        `set_widget_blueprint_mode failed: ${modeResult?.error ?? modeResult?.message ?? "unknown error"}`,
      );

      const designerState = unwrapAutomationResult(
        await tools.executeTool(
          "manage_widget_authoring",
          {
            action: "get_widget_designer_state",
            widgetPath: widgetObjectPath,
            openEditorIfNeeded: false,
          },
          { timeoutMs: 15000 },
        ),
      );
      requireStep(
        designerState?.success === true,
        `get_widget_designer_state failed: ${designerState?.error ?? designerState?.message ?? "unknown error"}`,
      );
      requireStep(
        designerState?.designerTabFound === true &&
          designerState?.designerViewFound === true,
        `Widget Designer did not become live after switching modes: ${JSON.stringify(designerState)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI target policy: discover the live widget window and resolve the healthy target",
    async (tools) => {
      const discoveryResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "list_visible_windows",
        }),
      );
      requireStep(
        discoveryResult?.success === true,
        `list_visible_windows failed: ${discoveryResult?.error ?? discoveryResult?.message ?? "unknown error"}`,
      );
      requireStep(
        Array.isArray(discoveryResult?.windows),
        "list_visible_windows did not return a windows array",
      );

      selectedWindow = pickUiTargetingWindow(discoveryResult.windows);
      requireStep(
        selectedWindow !== null,
        `No suitable live UI target policy widget window was discovered: ${JSON.stringify(discoveryResult?.windows ?? [])}`,
      );

      const liveResolveResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "resolve_ui_target",
          identifier: UI_TARGETING_WIDGET_COMMAND,
          tabId: UI_TARGETING_WIDGET_TAB_ID,
          windowTitle: selectedWindow.title,
        }),
      );
      requireStep(
        liveResolveResult?.success === true,
        `live resolve_ui_target failed: ${liveResolveResult?.error ?? liveResolveResult?.message ?? "unknown error"}`,
      );
      requireStep(
        liveResolveResult?.targetStatus === "resolved",
        `Expected the healthy live widget target to resolve cleanly, got ${JSON.stringify(liveResolveResult)}`,
      );
      requireStep(
        matchesResolvedWindow(
          selectedWindow.title,
          liveResolveResult?.windowTitle ??
            liveResolveResult?.resolvedWindowTitle,
        ),
        `Resolved target window did not match the discovered widget window: ${JSON.stringify(liveResolveResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI target policy: re-resolve a drifted tab handle by identifier instead of trusting stale handles",
    async (tools) => {
      requireStep(
        selectedWindow !== null,
        "No selected window available for stale re-resolution verification",
      );

      const driftedResolveResult = unwrapAutomationResult(
        await tools.executeTool("manage_ui", {
          action: "resolve_ui_target",
          identifier: UI_TARGETING_WIDGET_COMMAND,
          tabId: TARGET_POLICY_STALE_TAB_ID,
          windowTitle: selectedWindow.title,
        }),
      );

      requireStep(
        driftedResolveResult?.success === true,
        `drifted resolve_ui_target failed: ${driftedResolveResult?.error ?? driftedResolveResult?.message ?? "unknown error"}`,
      );
      requireStep(
        driftedResolveResult?.targetStatus === "stale",
        `Expected drifted resolve_ui_target to classify the handle as stale, got ${JSON.stringify(driftedResolveResult)}`,
      );
      requireStep(
        driftedResolveResult?.reResolved === true,
        `Expected drifted resolve_ui_target to mark the result as re-resolved: ${JSON.stringify(driftedResolveResult)}`,
      );
      requireStep(
        driftedResolveResult?.requestedIdentifier ===
          UI_TARGETING_WIDGET_COMMAND,
        `Drifted resolve_ui_target did not preserve requestedIdentifier: ${JSON.stringify(driftedResolveResult)}`,
      );
      requireStep(
        driftedResolveResult?.requestedTabId === TARGET_POLICY_STALE_TAB_ID,
        `Drifted resolve_ui_target did not preserve requestedTabId: ${JSON.stringify(driftedResolveResult)}`,
      );
      requireStep(
        driftedResolveResult?.requestedWindowTitle === selectedWindow.title,
        `Drifted resolve_ui_target did not preserve the live requestedWindowTitle: ${JSON.stringify(driftedResolveResult)}`,
      );
      requireStep(
        matchesResolvedWindow(
          selectedWindow.title,
          driftedResolveResult?.resolvedWindowTitle ??
            driftedResolveResult?.windowTitle,
        ),
        `Drifted resolve_ui_target did not recover the live widget window: ${JSON.stringify(driftedResolveResult)}`,
      );
      requireStep(
        driftedResolveResult?.staleReason === "missing_live_tab",
        `Drifted resolve_ui_target did not surface missing_live_tab for the stale tab handle: ${JSON.stringify(driftedResolveResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI target policy: stale focus fails explicitly when preflight is skipped",
    async (tools) => {
      requireStep(
        selectedWindow !== null,
        "No selected window available for stale focus verification",
      );

      const focusResult = unwrapAutomationResult(
        await tools.executeTool("control_editor", {
          action: "focus_editor_surface",
          surface: "editor_window",
          tabId: TARGET_POLICY_STALE_TAB_ID,
          windowTitle: selectedWindow.title,
        }),
      );
      const focusErrorDetails =
        focusResult?.error && typeof focusResult.error === "object"
          ? focusResult.error
          : focusResult;

      requireStep(
        focusResult?.success === false,
        `Expected stale focus to fail explicitly, got ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        focusResult?.errorCode === "FOCUS_FAILED",
        `Expected stale focus to return FOCUS_FAILED, got ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        focusErrorDetails?.focusApplied === false,
        `Expected stale focus to keep focusApplied=false, got ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        focusErrorDetails?.requestedTabId === TARGET_POLICY_STALE_TAB_ID,
        `Expected stale focus to preserve requestedTabId, got ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        focusErrorDetails?.requestedWindowTitle === selectedWindow.title,
        `Expected stale focus to preserve requestedWindowTitle, got ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        focusErrorDetails?.requestedTargetStillLive === false,
        `Expected stale focus to report requestedTargetStillLive=false, got ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        typeof focusErrorDetails?.focusFailureReason === "string" &&
          focusErrorDetails.focusFailureReason.length > 0,
        `Expected stale focus to surface a focusFailureReason, got ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        focusErrorDetails?.recoveryAction === "resolve_ui_target",
        `Expected stale focus to recommend resolve_ui_target, got ${JSON.stringify(focusResult)}`,
      );
      requireStep(
        focusErrorDetails?.staleReason === "missing_live_tab",
        `Expected stale focus to surface missing_live_tab, got ${JSON.stringify(focusResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI target policy: tabId-only screenshots fail fast instead of falling back to the viewport",
    async (tools) => {
      const screenshotResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "screenshot",
            filename: "ui-target-policy-ambiguous-target.png",
            tabId: TARGET_POLICY_STALE_TAB_ID,
          },
          { timeoutMs: 20000 },
        ),
      );
      const screenshotErrorDetails =
        screenshotResult?.error && typeof screenshotResult.error === "object"
          ? screenshotResult.error
          : screenshotResult;

      requireStep(
        screenshotResult?.success === false,
        `Expected tabId-only screenshot intent to fail explicitly, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotResult?.errorCode === "AMBIGUOUS_CAPTURE_TARGET",
        `Expected tabId-only screenshot intent to return AMBIGUOUS_CAPTURE_TARGET, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.requestedTabId === TARGET_POLICY_STALE_TAB_ID,
        `Expected the screenshot ambiguity branch to preserve requestedTabId, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        typeof screenshotErrorDetails?.captureIntentWarning === "string" &&
          /tabid/i.test(screenshotErrorDetails.captureIntentWarning),
        `Expected the screenshot ambiguity branch to surface a tabId warning, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.suggestedPreflightAction ===
          "resolve_ui_target",
        `Expected the screenshot ambiguity branch to recommend resolve_ui_target, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.requestedTargetStillLive === false,
        `Expected the screenshot ambiguity branch to preserve requestedTargetStillLive=false, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.captureTarget !== "viewport",
        `tabId-only screenshot intent must not claim a viewport capture, got ${JSON.stringify(screenshotResult)}`,
      );

      return true;
    },
  );

  runner.addStep(
    "UI target policy: editor-mode tabId-only screenshots fail before editor-window fallback",
    async (tools) => {
      const screenshotResult = unwrapAutomationResult(
        await tools.executeTool(
          "control_editor",
          {
            action: "screenshot",
            filename: "ui-target-policy-ambiguous-editor-target.png",
            mode: "editor",
            tabId: TARGET_POLICY_STALE_TAB_ID,
          },
          { timeoutMs: 20000 },
        ),
      );
      const screenshotErrorDetails =
        screenshotResult?.error && typeof screenshotResult.error === "object"
          ? screenshotResult.error
          : screenshotResult;

      requireStep(
        screenshotResult?.success === false,
        `Expected editor-mode tabId-only screenshot intent to fail explicitly, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotResult?.errorCode === "AMBIGUOUS_CAPTURE_TARGET",
        `Expected editor-mode tabId-only screenshot intent to return AMBIGUOUS_CAPTURE_TARGET, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.requestedCaptureMode === "editor",
        `Expected the editor-mode screenshot ambiguity branch to preserve requestedCaptureMode=editor, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.requestedTabId === TARGET_POLICY_STALE_TAB_ID,
        `Expected the editor-mode screenshot ambiguity branch to preserve requestedTabId, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        typeof screenshotErrorDetails?.captureIntentWarning === "string" &&
          /tabid/i.test(screenshotErrorDetails.captureIntentWarning),
        `Expected the editor-mode screenshot ambiguity branch to surface a tabId warning, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.suggestedPreflightAction ===
          "resolve_ui_target",
        `Expected the editor-mode screenshot ambiguity branch to recommend resolve_ui_target, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.requestedTargetStillLive === false,
        `Expected the editor-mode screenshot ambiguity branch to preserve requestedTargetStillLive=false, got ${JSON.stringify(screenshotResult)}`,
      );
      requireStep(
        screenshotErrorDetails?.captureTarget !== "editor_window" &&
          screenshotErrorDetails?.captureTarget !== "viewport",
        `editor-mode tabId-only screenshot intent must not claim a capture target, got ${JSON.stringify(screenshotResult)}`,
      );

      return true;
    },
  );

  await runner.run();
}

async function runGraphBatchingSuite() {
  const runner = new TestRunner("graph-batching");
  const createdNodeIds = [];
  let requestedNodeIds = [];

  runner.addStep(
    "Graph batching: ensure the blueprint fixture exists",
    async (tools) => {
      const advancedFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advancedFolderResult, ["already exists"]),
        `create_folder failed for ${ADV_TEST_FOLDER}: ${advancedFolderResult?.error ?? advancedFolderResult?.message ?? "unknown error"}`,
      );

      const blueprintResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "create",
          name: "BP_GraphBatching",
          path: ADV_TEST_FOLDER,
          parentClass: "Actor",
        }),
      );
      requireStep(
        isSuccessLike(blueprintResult, ["already exists"]),
        `create blueprint failed: ${blueprintResult?.error ?? blueprintResult?.message ?? "unknown error"}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Graph batching: add several nodes through the public seam",
    async (tools) => {
      const nodePositions = [240, 480, 720, 960];

      for (const x of nodePositions) {
        const nodeResult = unwrapAutomationResult(
          await tools.executeTool("manage_blueprint", {
            action: "create_node",
            blueprintPath: GRAPH_BATCHING_BLUEPRINT,
            graphName: "EventGraph",
            nodeType: "PrintString",
            x,
            y: 0,
          }),
        );
        requireStep(
          nodeResult?.success === true,
          `create_node failed at x=${x}: ${nodeResult?.error ?? nodeResult?.message ?? "unknown error"}`,
        );
        requireStep(
          typeof nodeResult?.nodeId === "string" &&
            nodeResult.nodeId.length > 0,
          `create_node did not return a nodeId at x=${x}`,
        );
        createdNodeIds.push(nodeResult.nodeId);
      }

      return createdNodeIds.length === nodePositions.length;
    },
  );

  runner.addStep(
    "Graph batching: collect requested ids from get_graph_details",
    async (tools) => {
      const graphResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "get_graph_details",
          blueprintPath: GRAPH_BATCHING_BLUEPRINT,
          graphName: "EventGraph",
        }),
      );
      requireStep(
        graphResult?.success === true,
        `get_graph_details failed: ${graphResult?.error ?? graphResult?.message ?? "unknown error"}`,
      );
      requireStep(
        Array.isArray(graphResult?.nodes),
        `get_graph_details did not return nodes: ${JSON.stringify(graphResult)}`,
      );

      requestedNodeIds = graphResult.nodes
        .map((nodeInfo) =>
          typeof nodeInfo?.nodeId === "string" ? nodeInfo.nodeId : null,
        )
        .filter(
          (nodeId) =>
            typeof nodeId === "string" && createdNodeIds.includes(nodeId),
        )
        .reverse();

      requireStep(
        requestedNodeIds.length >= 4,
        `Expected at least 4 created node ids from get_graph_details, got ${JSON.stringify(graphResult?.nodes ?? [])}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Graph batching: retrieve the first bounded page",
    async (tools) => {
      const batchResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "get_node_details_batch",
          blueprintPath: GRAPH_BATCHING_BLUEPRINT,
          graphName: "EventGraph",
          nodeIds: requestedNodeIds,
          limit: 2,
        }),
      );
      requireStep(
        batchResult?.success === true,
        `get_node_details_batch first page failed: ${batchResult?.error ?? batchResult?.message ?? "unknown error"}`,
      );
      requireStep(
        batchResult?.shown === 2,
        `Expected shown=2 on the first batch page: ${JSON.stringify(batchResult)}`,
      );
      requireStep(
        batchResult?.totalRequested === requestedNodeIds.length,
        `Expected totalRequested=${requestedNodeIds.length}: ${JSON.stringify(batchResult)}`,
      );
      requireStep(
        batchResult?.truncated === true,
        `Expected the first batch page to be truncated: ${JSON.stringify(batchResult)}`,
      );
      requireStep(
        typeof batchResult?.nextCursor === "string" &&
          batchResult.nextCursor.length > 0,
        `Expected a nextCursor on the first batch page: ${JSON.stringify(batchResult)}`,
      );
      requireStep(
        Array.isArray(batchResult?.nodes) && batchResult.nodes.length === 2,
        `Expected 2 nodes on the first batch page: ${JSON.stringify(batchResult)}`,
      );

      const returnedNodeIds = batchResult.nodes.map(
        (nodeInfo) => nodeInfo?.nodeId,
      );
      requireStep(
        JSON.stringify(returnedNodeIds) ===
          JSON.stringify(requestedNodeIds.slice(0, 2)),
        `The first batch page did not preserve the caller supplied nodeIds order: ${JSON.stringify({ requestedNodeIds, returnedNodeIds, batchResult }, null, 2)}`,
      );

      requestedNodeIds = [...requestedNodeIds, batchResult.nextCursor];
      return true;
    },
  );

  runner.addStep(
    "Graph batching: retrieve the remaining bounded page",
    async (tools) => {
      const cursor = requestedNodeIds[requestedNodeIds.length - 1];
      const expectedNodeIds = requestedNodeIds.slice(
        0,
        requestedNodeIds.length - 1,
      );

      const batchResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "get_node_details_batch",
          blueprintPath: GRAPH_BATCHING_BLUEPRINT,
          graphName: "EventGraph",
          nodeIds: expectedNodeIds,
          limit: 2,
          cursor,
        }),
      );
      requireStep(
        batchResult?.success === true,
        `get_node_details_batch second page failed: ${batchResult?.error ?? batchResult?.message ?? "unknown error"}`,
      );
      requireStep(
        batchResult?.shown === expectedNodeIds.length - 2,
        `Expected the second batch page to return the remaining nodes: ${JSON.stringify(batchResult)}`,
      );
      requireStep(
        batchResult?.truncated === false,
        `Expected the second batch page to finish the request: ${JSON.stringify(batchResult)}`,
      );
      requireStep(
        batchResult?.nextCursor === "",
        `Expected nextCursor to be empty after the last batch page: ${JSON.stringify(batchResult)}`,
      );

      const returnedNodeIds = batchResult.nodes.map(
        (nodeInfo) => nodeInfo?.nodeId,
      );
      requireStep(
        JSON.stringify(returnedNodeIds) ===
          JSON.stringify(expectedNodeIds.slice(2)),
        `The second batch page did not preserve the caller supplied nodeIds order: ${JSON.stringify({ expectedNodeIds, returnedNodeIds, batchResult }, null, 2)}`,
      );

      return true;
    },
  );

  await runner.run();
}

async function runWidgetBindingsSuite() {
  const runner = new TestRunner("widget-bindings");
  let widgetObjectPath = WIDGET_BINDINGS_WIDGET_OBJECT;
  let widgetAssetPath = WIDGET_BINDINGS_WIDGET_PACKAGE;

  runner.addStep(
    "Widget bindings: create a fresh widget fixture",
    async (tools) => {
      const advancedFolderResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "create_folder",
          path: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(advancedFolderResult, ["already exists"]),
        `create_folder failed for ${ADV_TEST_FOLDER}: ${advancedFolderResult?.error ?? advancedFolderResult?.message ?? "unknown error"}`,
      );

      const deleteWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: WIDGET_BINDINGS_WIDGET_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteWidgetResult, ["not found"]),
        `delete widget fixture failed: ${deleteWidgetResult?.error ?? deleteWidgetResult?.message ?? "unknown error"}`,
      );

      const deleteFallbackWidgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_asset", {
          action: "delete",
          path: WIDGET_BINDINGS_WIDGET_FALLBACK_PACKAGE,
          force: true,
        }),
      );
      requireStep(
        isSuccessLike(deleteFallbackWidgetResult, ["not found"]),
        `delete fallback widget fixture failed: ${deleteFallbackWidgetResult?.error ?? deleteFallbackWidgetResult?.message ?? "unknown error"}`,
      );

      const widgetResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "create_widget_blueprint",
          name: "WBP_WidgetBindings",
          folder: ADV_TEST_FOLDER,
        }),
      );
      requireStep(
        isSuccessLike(widgetResult, ["already exists"]),
        `create_widget_blueprint failed: ${widgetResult?.error ?? widgetResult?.message ?? "unknown error"}`,
      );

      if (
        typeof widgetResult?.widgetPath === "string" &&
        widgetResult.widgetPath.length > 0
      ) {
        widgetObjectPath = widgetResult.widgetPath;
        const objectSeparatorIndex = widgetObjectPath.indexOf(
          ".",
          widgetObjectPath.lastIndexOf("/"),
        );
        widgetAssetPath =
          objectSeparatorIndex > 0
            ? widgetObjectPath.slice(0, objectSeparatorIndex)
            : widgetObjectPath;
      }

      const fixtureRequests = [
        {
          action: "add_canvas_panel",
          widgetPath: widgetObjectPath,
          slotName: "RootCanvas",
        },
        {
          action: "add_text_block",
          widgetPath: widgetObjectPath,
          slotName: "StatusLabel",
          parentSlot: "RootCanvas",
          text: "Ready",
        },
        {
          action: "add_button",
          widgetPath: widgetObjectPath,
          slotName: "StartButton",
          parentSlot: "RootCanvas",
        },
      ];

      for (const request of fixtureRequests) {
        const response = unwrapAutomationResult(
          await tools.executeTool("manage_widget_authoring", request),
        );
        requireStep(
          response?.success === true,
          `${request.action} failed: ${response?.error ?? response?.message ?? "unknown error"}`,
        );
      }

      const addVariableResult = unwrapAutomationResult(
        await tools.executeTool("manage_blueprint", {
          action: "add_variable",
          blueprintPath: widgetAssetPath,
          variableName: "StatusText",
          variableType: "Text",
          defaultValue: "Ready",
        }),
      );
      requireStep(
        addVariableResult?.success === true,
        `add_variable failed for StatusText: ${addVariableResult?.error ?? addVariableResult?.message ?? "unknown error"}`,
      );

      const treeResult = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "get_widget_tree",
          widgetPath: widgetObjectPath,
        }),
      );
      requireStep(
        treeResult?.success === true,
        `get_widget_tree failed: ${treeResult?.error ?? treeResult?.message ?? "unknown error"}`,
      );

      const expectedNames = new Set([
        "RootCanvas",
        "StatusLabel",
        "StartButton",
      ]);
      const foundNames = findNamedWidgets(
        treeResult?.widgetTree,
        expectedNames,
      );
      requireStep(
        foundNames.size === expectedNames.size,
        `Widget fixture tree did not contain all expected widgets: ${JSON.stringify(treeResult?.widgetTree ?? {})}`,
      );

      return true;
    },
  );

  runner.addStep(
    "Widget bindings: reject event binding without explicit variable promotion",
    async (tools) => {
      const response = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "bind_on_clicked",
          widgetPath: widgetObjectPath,
          slotName: "StartButton",
          functionName: "OnStartClicked",
        }),
      );

      requireStep(
        response?.success === false,
        `bind_on_clicked unexpectedly succeeded: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.bindingApplied === false,
        `bind_on_clicked should not apply a binding before promotion: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.errorCode === "WIDGET_NOT_VARIABLE",
        `bind_on_clicked returned the wrong error code: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.requiresBlueprintVariable === true,
        `bind_on_clicked should explain the Blueprint variable requirement: ${JSON.stringify(response)}`,
      );
      requireStep(
        typeof response?.suggestedFix === "string" &&
          response.suggestedFix.includes("ensureVariable"),
        `bind_on_clicked did not suggest the explicit ensureVariable retry: ${JSON.stringify(response)}`,
      );
      return true;
    },
  );

  runner.addStep(
    "Widget bindings: create the component-bound click event with ensureVariable",
    async (tools) => {
      const response = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "bind_on_clicked",
          widgetPath: widgetObjectPath,
          slotName: "StartButton",
          functionName: "OnStartClicked",
          ensureVariable: true,
        }),
      );

      requireStep(
        response?.success === true,
        `bind_on_clicked with ensureVariable failed: ${response?.error ?? response?.message ?? "unknown error"}`,
      );
      requireStep(
        response?.bindingApplied === true,
        `bind_on_clicked did not report bindingApplied: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.widgetIsVariable === true,
        `bind_on_clicked did not leave the widget as a Blueprint variable: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.widgetWasMadeVariable === true,
        `bind_on_clicked did not report the explicit variable promotion: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.eventNodeCreated === true &&
          response?.eventNodeExisted === false,
        `bind_on_clicked did not report the created event-node branch: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.handlerNamingMode === "engine_generated",
        `bind_on_clicked should report engine_generated handler naming: ${JSON.stringify(response)}`,
      );
      requireStep(
        typeof response?.bindingFunctionName === "string" &&
          response.bindingFunctionName.startsWith("BndEvt__"),
        `bind_on_clicked did not return the engine-generated binding function name: ${JSON.stringify(response)}`,
      );
      return true;
    },
  );

  runner.addStep(
    "Widget bindings: keep repeated click binding idempotent",
    async (tools) => {
      const response = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "bind_on_clicked",
          widgetPath: widgetObjectPath,
          slotName: "StartButton",
          functionName: "OnStartClicked",
          ensureVariable: true,
        }),
      );

      requireStep(
        response?.success === true,
        `repeat bind_on_clicked failed: ${response?.error ?? response?.message ?? "unknown error"}`,
      );
      requireStep(
        response?.bindingApplied === true,
        `repeat bind_on_clicked should still report the binding as applied: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.eventNodeCreated === false &&
          response?.eventNodeExisted === true,
        `repeat bind_on_clicked did not take the idempotent event-node branch: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.widgetWasMadeVariable === false,
        `repeat bind_on_clicked should not promote the widget again: ${JSON.stringify(response)}`,
      );
      return true;
    },
  );

  runner.addStep(
    "Widget bindings: author the first property binding",
    async (tools) => {
      const response = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "bind_text",
          widgetPath: widgetObjectPath,
          slotName: "StatusLabel",
          bindingSource: "StatusText",
        }),
      );

      requireStep(
        response?.success === true,
        `bind_text failed: ${response?.error ?? response?.message ?? "unknown error"}`,
      );
      requireStep(
        response?.bindingApplied === true,
        `bind_text did not report bindingApplied: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.requiresManualFollowThrough === false,
        `bind_text should not require manual follow-through: ${JSON.stringify(response)}`,
      );
      requireStep(
        typeof response?.bindingFunctionName === "string" &&
          response.bindingFunctionName.length > 0,
        `bind_text did not return a concrete binding function name: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.bindingFunctionCreated === true &&
          response?.bindingFunctionExisted === false,
        `bind_text did not report the created binding-function branch: ${JSON.stringify(response)}`,
      );
      return true;
    },
  );

  runner.addStep(
    "Widget bindings: keep repeated property binding idempotent",
    async (tools) => {
      const response = unwrapAutomationResult(
        await tools.executeTool("manage_widget_authoring", {
          action: "bind_text",
          widgetPath: widgetObjectPath,
          slotName: "StatusLabel",
          bindingSource: "StatusText",
        }),
      );

      requireStep(
        response?.success === true,
        `repeat bind_text failed: ${response?.error ?? response?.message ?? "unknown error"}`,
      );
      requireStep(
        response?.bindingApplied === true,
        `repeat bind_text should still report the binding as applied: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.bindingFunctionCreated === false &&
          response?.bindingFunctionExisted === true,
        `repeat bind_text did not take the idempotent binding-function branch: ${JSON.stringify(response)}`,
      );
      requireStep(
        response?.requiresManualFollowThrough === false,
        `repeat bind_text should not introduce manual follow-through: ${JSON.stringify(response)}`,
      );
      return true;
    },
  );

  await runner.run();
}

const testCases = [
  {
    scenario: "System: execute safe console command (log)",
    toolName: "system_control",
    arguments: {
      action: "execute_command",
      command: "Log Integration test started",
    },
    expected: "success|handled|blocked",
  },
  {
    scenario: "Lighting: list available light types",
    toolName: "manage_lighting",
    arguments: { action: "list_light_types" },
    expected: "success",
  },
  {
    scenario: "Effects: list available debug shapes",
    toolName: "manage_effect",
    arguments: { action: "list_debug_shapes" },
    expected: "success",
  },
  {
    scenario: "Sequencer: list available track types",
    toolName: "manage_sequence",
    arguments: { action: "list_track_types" },
    expected: "success",
  },
  {
    scenario: "UI: list visible Slate windows",
    toolName: "manage_ui",
    arguments: { action: "list_visible_windows" },
    expected: "listed visible slate windows",
  },
  {
    scenario: "Asset: create test folder",
    toolName: "manage_asset",
    arguments: { action: "create_folder", path: TEST_FOLDER },
    expected: "success|already exists",
  },
  {
    scenario: "Asset: create material",
    toolName: "manage_asset",
    arguments: {
      action: "create_material",
      name: "M_IntegrationTest",
      path: TEST_FOLDER,
    },
    expected: "success|already exists",
  },
  {
    scenario: "Actor: spawn StaticMeshActor (cube)",
    toolName: "control_actor",
    arguments: {
      action: "spawn",
      classPath: "/Engine/BasicShapes/Cube",
      actorName: "IT_Cube",
      location: { x: 0, y: 0, z: 200 },
    },
    expected: "success",
  },
  {
    scenario: "Actor: set transform",
    toolName: "control_actor",
    arguments: {
      action: "set_transform",
      actorName: "IT_Cube",
      location: { x: 100, y: 100, z: 300 },
    },
    expected: "success|not found",
  },
  {
    scenario: "Blueprint: create Actor blueprint",
    toolName: "manage_blueprint",
    arguments: {
      action: "create",
      name: "BP_IntegrationTest",
      path: TEST_FOLDER,
      parentClass: "Actor",
    },
    expected: "success|already exists",
  },
  {
    scenario: "Blueprint: create PrintString node for inspection",
    toolName: "manage_blueprint",
    arguments: {
      action: "create_node",
      blueprintPath: `${TEST_FOLDER}/BP_IntegrationTest`,
      graphName: "EventGraph",
      nodeType: "PrintString",
      x: 240,
      y: 0,
    },
    expected: "node created",
    captureResult: { key: "integrationPrintStringNodeId", fromField: "nodeId" },
  },
  {
    scenario: "Blueprint: inspect graph details via public action",
    toolName: "manage_blueprint",
    arguments: {
      action: "get_graph_details",
      blueprintPath: `${TEST_FOLDER}/BP_IntegrationTest`,
      graphName: "EventGraph",
    },
    expected: "graph details retrieved",
    captureResult: { key: "integrationEventGraphName", fromField: "graphName" },
  },
  {
    scenario: "Blueprint: inspect pin details via public action",
    toolName: "manage_blueprint",
    arguments: {
      action: "get_pin_details",
      blueprintPath: `${TEST_FOLDER}/BP_IntegrationTest`,
      graphName: "${captured:integrationEventGraphName}",
      nodeId: "${captured:integrationPrintStringNodeId}",
      pinName: "InString",
    },
    expected: "pin details retrieved",
  },
  {
    scenario: "Geometry: Create box primitive",
    toolName: "manage_geometry",
    arguments: {
      action: "create_box",
      actorName: "GeoTest_Box",
      dimensions: [100, 100, 100],
      location: { x: 0, y: 0, z: 100 },
    },
    expected: "success|already exists",
  },
  {
    scenario: "Skeleton: Get skeleton info",
    toolName: "manage_skeleton",
    arguments: {
      action: "get_skeleton_info",
      skeletonPath: "/Engine/EngineMeshes/SkeletalCube_Skeleton",
    },
    expected: "success|not found",
  },
  {
    scenario: "Material Authoring: Create material",
    toolName: "manage_material_authoring",
    arguments: {
      action: "create_material",
      name: "M_AdvTest",
      path: ADV_TEST_FOLDER,
    },
    expected: "success|already exists",
  },
  {
    scenario: "Texture: Create noise texture",
    toolName: "manage_texture",
    arguments: {
      action: "create_noise_texture",
      name: "T_TestNoise",
      path: ADV_TEST_FOLDER,
    },
    expected: "success|already exists",
  },
  {
    scenario: "Animation: Create anim blueprint",
    toolName: "manage_animation_authoring",
    arguments: {
      action: "create_anim_blueprint",
      name: "ABP_Test",
      path: ADV_TEST_FOLDER,
      skeletonPath: "/Engine/EngineMeshes/SkeletalCube_Skeleton",
    },
    expected: "success|already exists|not found",
  },
  {
    scenario: "Niagara: Create niagara system",
    toolName: "manage_niagara_authoring",
    arguments: {
      action: "create_niagara_system",
      name: "NS_Test",
      path: ADV_TEST_FOLDER,
    },
    expected: "success|already exists",
  },
  {
    scenario: "GAS: Create attribute set",
    toolName: "manage_gas",
    arguments: {
      action: "create_attribute_set",
      name: "AS_TestAttributes",
      path: ADV_TEST_FOLDER,
    },
    expected: "success|already exists",
  },
  {
    scenario: "Combat: Create weapon blueprint",
    toolName: "manage_combat",
    arguments: {
      action: "create_weapon_blueprint",
      name: "BP_TestWeapon",
      path: ADV_TEST_FOLDER,
      weaponType: "Rifle",
    },
    expected: "success|already exists",
  },
  {
    scenario: "AI: Create AI controller",
    toolName: "manage_ai",
    arguments: {
      action: "create_ai_controller",
      name: "AIC_Test",
      path: ADV_TEST_FOLDER,
    },
    expected: "success|already exists",
  },
  {
    scenario: "Interaction: Create door actor",
    toolName: "manage_interaction",
    arguments: {
      action: "create_door_actor",
      name: "BP_TestDoor",
      path: ADV_TEST_FOLDER,
    },
    expected: "success|already exists",
  },
  {
    scenario: "Widget: delete prior widget blueprint fixture",
    toolName: "manage_asset",
    arguments: {
      action: "delete",
      path: WIDGET_FIXTURE_PACKAGE,
      force: true,
    },
    expected: "not found|success",
  },
  {
    scenario: "Widget: Create widget blueprint",
    toolName: "manage_widget_authoring",
    arguments: {
      action: "create_widget_blueprint",
      name: "WBP_TestWidget",
      folder: "/Game/UI",
    },
    expected: "created widget blueprint|success",
  },
  {
    scenario: "Widget: add canvas root for tree inspection",
    toolName: "manage_widget_authoring",
    arguments: {
      action: "add_canvas_panel",
      widgetPath: WIDGET_FIXTURE_OBJECT,
      slotName: "RootCanvas",
    },
    expected: "added canvas panel|success",
  },
  {
    scenario: "Widget: inspect widget tree via public action",
    toolName: "manage_widget_authoring",
    arguments: {
      action: "get_widget_tree",
      widgetPath: WIDGET_FIXTURE_OBJECT,
    },
    expected: "retrieved widget tree",
  },
  {
    scenario: "Networking: Set property replicated",
    toolName: "manage_networking",
    arguments: {
      action: "set_property_replicated",
      blueprintPath: `${ADV_TEST_FOLDER}/BP_TestCharacter`,
      propertyName: "Health",
      replicated: true,
    },
    expected: "success|not found",
  },
  {
    scenario: "Game Framework: Create game mode",
    toolName: "manage_game_framework",
    arguments: {
      action: "create_game_mode",
      name: "GM_Test",
      path: ADV_TEST_FOLDER,
    },
    expected: "success|already exists",
  },
  {
    scenario: "Game Framework: Get info",
    toolName: "manage_game_framework",
    arguments: {
      action: "get_game_framework_info",
      gameModeBlueprint: `${ADV_TEST_FOLDER}/GM_Test`,
    },
    expected: "success|not found",
  },
  {
    scenario: "Sessions: Configure local session",
    toolName: "manage_sessions",
    arguments: {
      action: "configure_local_session_settings",
      maxPlayers: 4,
      sessionName: "TestSession",
    },
    expected: "success",
  },
  {
    scenario: "Sessions: Configure split screen",
    toolName: "manage_sessions",
    arguments: {
      action: "configure_split_screen",
      enabled: true,
      splitScreenType: "TwoPlayer_Horizontal",
    },
    expected: "success",
  },
  {
    scenario: "Sessions: Get info",
    toolName: "manage_sessions",
    arguments: { action: "get_sessions_info" },
    expected: "success",
  },
  // Level Structure
  {
    scenario: "Level Structure: Get info",
    toolName: "manage_level_structure",
    arguments: { action: "get_level_structure_info" },
    expected: "success",
  },
  {
    scenario: "Level Structure: Enable World Partition",
    toolName: "manage_level_structure",
    arguments: {
      action: "enable_world_partition",
      bEnableWorldPartition: true,
    },
    expected: "success",
  },
  {
    scenario: "Level Structure: Configure grid size",
    toolName: "manage_level_structure",
    arguments: {
      action: "configure_grid_size",
      gridCellSize: 12800,
      loadingRange: 25600,
    },
    expected: "success|not enabled",
  },
  {
    scenario: "Level Structure: Create data layer",
    toolName: "manage_level_structure",
    arguments: {
      action: "create_data_layer",
      dataLayerName: "TestLayer",
      dataLayerType: "Runtime",
    },
    expected: "success|not available",
  },
  {
    scenario: "Level Structure: Configure HLOD",
    toolName: "manage_level_structure",
    arguments: {
      action: "configure_hlod_layer",
      hlodLayerName: "DefaultHLOD",
      cellSize: 25600,
    },
    expected: "success",
  },
  {
    scenario: "Level Structure: Open Level Blueprint",
    toolName: "manage_level_structure",
    arguments: { action: "open_level_blueprint" },
    expected: "success",
  },
  // Volumes & Zones
  {
    scenario: "Volumes: Create trigger box",
    toolName: "manage_volumes",
    arguments: {
      action: "create_trigger_box",
      volumeName: "IT_TriggerBox",
      location: { x: 500, y: 0, z: 100 },
      extent: { x: 100, y: 100, z: 100 },
    },
    expected: "success",
  },
  {
    scenario: "Volumes: Create blocking volume",
    toolName: "manage_volumes",
    arguments: {
      action: "create_blocking_volume",
      volumeName: "IT_BlockingVol",
      location: { x: 600, y: 0, z: 100 },
      extent: { x: 200, y: 200, z: 200 },
    },
    expected: "success",
  },
  {
    scenario: "Volumes: Create physics volume",
    toolName: "manage_volumes",
    arguments: {
      action: "create_physics_volume",
      volumeName: "IT_PhysicsVol",
      location: { x: 700, y: 0, z: 100 },
      bWaterVolume: true,
      fluidFriction: 0.5,
    },
    expected: "success",
  },
  {
    scenario: "Volumes: Create audio volume",
    toolName: "manage_volumes",
    arguments: {
      action: "create_audio_volume",
      volumeName: "IT_AudioVol",
      location: { x: 800, y: 0, z: 100 },
      bEnabled: true,
    },
    expected: "success",
  },
  {
    scenario: "Volumes: Create nav mesh bounds",
    toolName: "manage_volumes",
    arguments: {
      action: "create_nav_mesh_bounds_volume",
      volumeName: "IT_NavBoundsVol",
      location: { x: 0, y: 500, z: 100 },
      extent: { x: 2000, y: 2000, z: 500 },
    },
    expected: "success",
  },
  {
    scenario: "Volumes: Get volumes info",
    toolName: "manage_volumes",
    arguments: { action: "get_volumes_info", volumeType: "Trigger" },
    expected: "success",
  },
  {
    scenario: "Volumes: Set volume properties",
    toolName: "manage_volumes",
    arguments: {
      action: "set_volume_properties",
      volumeName: "IT_PhysicsVol",
      bWaterVolume: false,
      fluidFriction: 0.3,
    },
    expected: "success|not found",
  },
  // Navigation System
  {
    scenario: "Navigation: Get navigation info",
    toolName: "manage_navigation",
    arguments: { action: "get_navigation_info" },
    expected: "success",
  },
  {
    scenario: "Navigation: Set nav agent properties",
    toolName: "manage_navigation",
    arguments: {
      action: "set_nav_agent_properties",
      agentRadius: 35,
      agentHeight: 144,
      agentStepHeight: 35,
    },
    expected: "success",
  },
  {
    scenario: "Navigation: Configure nav mesh settings",
    toolName: "manage_navigation",
    arguments: {
      action: "configure_nav_mesh_settings",
      cellSize: 19,
      cellHeight: 10,
      tileSizeUU: 1000,
    },
    expected: "success",
  },
  {
    scenario: "Navigation: Create nav link proxy",
    toolName: "manage_navigation",
    arguments: {
      action: "create_nav_link_proxy",
      actorName: "IT_NavLink",
      location: { x: 0, y: 0, z: 100 },
      startPoint: { x: -100, y: 0, z: 0 },
      endPoint: { x: 100, y: 0, z: 0 },
      direction: "BothWays",
    },
    expected: "success",
  },
  {
    scenario: "Navigation: Configure nav link",
    toolName: "manage_navigation",
    arguments: {
      action: "configure_nav_link",
      actorName: "IT_NavLink",
      snapRadius: 30,
    },
    expected: "success|not found",
  },
  {
    scenario: "Navigation: Set nav link type",
    toolName: "manage_navigation",
    arguments: {
      action: "set_nav_link_type",
      actorName: "IT_NavLink",
      linkType: "smart",
    },
    expected: "success|not found",
  },
  // Spline System
  {
    scenario: "Splines: Create spline actor",
    toolName: "manage_splines",
    arguments: {
      action: "create_spline_actor",
      actorName: "IT_SplineActor",
      location: { x: 0, y: 0, z: 100 },
      bClosedLoop: false,
    },
    expected: "success",
  },
  {
    scenario: "Splines: Add spline point",
    toolName: "manage_splines",
    arguments: {
      action: "add_spline_point",
      actorName: "IT_SplineActor",
      position: { x: 500, y: 0, z: 100 },
    },
    expected: "success|not found",
  },
  {
    scenario: "Splines: Set spline point position",
    toolName: "manage_splines",
    arguments: {
      action: "set_spline_point_position",
      actorName: "IT_SplineActor",
      pointIndex: 1,
      position: { x: 600, y: 100, z: 150 },
    },
    expected: "success|not found",
  },
  {
    scenario: "Splines: Set spline type",
    toolName: "manage_splines",
    arguments: {
      action: "set_spline_type",
      actorName: "IT_SplineActor",
      splineType: "linear",
    },
    expected: "success|not found",
  },
  {
    scenario: "Splines: Create road spline",
    toolName: "manage_splines",
    arguments: {
      action: "create_road_spline",
      actorName: "IT_RoadSpline",
      location: { x: 1000, y: 0, z: 0 },
      width: 400,
    },
    expected: "success",
  },
  {
    scenario: "Splines: Get splines info",
    toolName: "manage_splines",
    arguments: { action: "get_splines_info" },
    expected: "success",
  },
  {
    scenario: "Splines: Get specific spline info",
    toolName: "manage_splines",
    arguments: { action: "get_splines_info", actorName: "IT_SplineActor" },
    expected: "success|not found",
  },
  {
    scenario: "Cleanup: delete spline actors",
    toolName: "control_actor",
    arguments: { action: "delete", actorName: "IT_SplineActor" },
    expected: "success|not found",
  },
  {
    scenario: "Cleanup: delete road spline",
    toolName: "control_actor",
    arguments: { action: "delete", actorName: "IT_RoadSpline" },
    expected: "success|not found",
  },
  // search_assets: searchText filtering (fix for Issue #233)
  {
    scenario: "Asset: search by text (exact name)",
    toolName: "manage_asset",
    arguments: { action: "search_assets", searchText: "BP_IntegrationTest" },
    expected: "success",
  },
  {
    scenario: "Asset: search by text (partial, case-insensitive)",
    toolName: "manage_asset",
    arguments: { action: "search_assets", searchText: "integrationtest" },
    expected: "success",
  },
  {
    scenario: "Asset: search by text + class filter",
    toolName: "manage_asset",
    arguments: {
      action: "search_assets",
      searchText: "IntegrationTest",
      classNames: ["Blueprint"],
    },
    expected: "success",
  },
  {
    scenario: "Asset: search by text + path filter",
    toolName: "manage_asset",
    arguments: {
      action: "search_assets",
      searchText: "IntegrationTest",
      packagePaths: ["/Game/IntegrationTest"],
      recursivePaths: true,
    },
    expected: "success",
  },
  {
    scenario: "Asset: search with no matches",
    toolName: "manage_asset",
    arguments: {
      action: "search_assets",
      searchText: "ZZZZZ_NonExistent_Asset_12345",
    },
    expected: "success",
  },
  {
    scenario: "Asset: search without searchText (structured query)",
    toolName: "manage_asset",
    arguments: {
      action: "search_assets",
      classNames: ["Blueprint"],
      packagePaths: ["/Game/IntegrationTest"],
    },
    expected: "success",
  },
  {
    scenario: "Cleanup: delete test actor",
    toolName: "control_actor",
    arguments: { action: "delete", actorName: "IT_Cube" },
    expected: "success|not found",
  },
  {
    scenario: "Cleanup: delete test folder",
    toolName: "manage_asset",
    arguments: { action: "delete", path: TEST_FOLDER, force: true },
    expected: "success|not found",
  },
  {
    scenario: "Cleanup: delete advanced test folder",
    toolName: "manage_asset",
    arguments: { action: "delete", path: ADV_TEST_FOLDER, force: true },
    expected: "success|not found",
  },
];

function getPublicInspectionCases() {
  const scenarioNames = new Set([
    "Asset: create test folder",
    "Blueprint: create Actor blueprint",
    "Blueprint: create PrintString node for inspection",
    "Blueprint: inspect graph details via public action",
    "Blueprint: inspect pin details via public action",
    "Widget: delete prior widget blueprint fixture",
    "Widget: Create widget blueprint",
    "Widget: add canvas root for tree inspection",
    "Widget: inspect widget tree via public action",
  ]);
  const selectedCases = testCases.filter((testCase) =>
    scenarioNames.has(testCase.scenario),
  );

  requireStep(
    selectedCases.length === scenarioNames.size,
    `public inspection inspection suite is out of sync: expected ${scenarioNames.size} scenarios, found ${selectedCases.length}`,
  );

  return selectedCases;
}

function getCapabilityHonestyCases() {
  return [
    {
      scenario:
        "Capability honesty: cube texture placeholder must report NOT_IMPLEMENTED",
      toolName: "manage_texture",
      arguments: {
        action: "create_cube_texture",
        name: "T_CubeCapabilityPlaceholder",
        path: ADV_TEST_FOLDER,
        size: 128,
      },
      expected: {
        condition: "error",
        errorPattern: "not_implemented",
      },
    },
    {
      scenario:
        "Capability honesty: preview_physics must report runtime-only NOT_IMPLEMENTED",
      toolName: "manage_skeleton",
      arguments: {
        action: "preview_physics",
        skeletalMeshPath:
          "/Game/AdvancedIntegrationTest/SK_RuntimeCapabilityPlaceholder.SK_RuntimeCapabilityPlaceholder",
        enable: true,
      },
      expected: {
        condition: "error",
        errorPattern: "not_implemented",
      },
    },
    {
      scenario:
        "Capability honesty: legacy impact effect must require explicit systemPath",
      toolName: "manage_effect",
      arguments: {
        action: "create_impact_effect",
        location: [0, 0, 0],
      },
      expected: {
        condition: "error",
        errorPattern: "invalid_argument",
      },
    },
  ];
}

if (process.env.UNREAL_MCP_INTEGRATION_SUITE === "public-inspection") {
  await runToolTests("public-inspection", getPublicInspectionCases());
} else if (
  process.env.UNREAL_MCP_INTEGRATION_SUITE === "targeted-window-input"
) {
  await runTargetedWindowInputSuite();
} else if (process.env.UNREAL_MCP_INTEGRATION_SUITE === "semantic-navigation") {
  await runSemanticNavigationSuite();
} else if (process.env.UNREAL_MCP_INTEGRATION_SUITE === "graph-review") {
  await runGraphReviewSuite();
} else if (
  process.env.UNREAL_MCP_INTEGRATION_SUITE === "public-surface-validation"
) {
  await runPublicSurfaceValidationSuite();
} else if (process.env.UNREAL_MCP_INTEGRATION_SUITE === "designer-marquee") {
  await runDesignerMarqueeSuite();
} else if (process.env.UNREAL_MCP_INTEGRATION_SUITE === "designer-selection") {
  await runDesignerSelectionSuite();
} else if (
  process.env.UNREAL_MCP_INTEGRATION_SUITE === "designer-geometry-readback"
) {
  await runDesignerGeometryReadbackSuite();
} else if (
  process.env.UNREAL_MCP_INTEGRATION_SUITE === "designer-rectangle-selection"
) {
  await runDesignerRectangleSelectionSuite();
} else if (process.env.UNREAL_MCP_INTEGRATION_SUITE === "ui-targeting") {
  await runUiTargetingSuite();
} else if (process.env.UNREAL_MCP_INTEGRATION_SUITE === "ui-target-policy") {
  await runUiTargetPolicySuite();
} else if (process.env.UNREAL_MCP_INTEGRATION_SUITE === "graph-batching") {
  await runGraphBatchingSuite();
} else if (process.env.UNREAL_MCP_INTEGRATION_SUITE === "widget-bindings") {
  await runWidgetBindingsSuite();
} else if (process.env.UNREAL_MCP_INTEGRATION_SUITE === "capability-honesty") {
  await runToolTests("capability-honesty", getCapabilityHonestyCases());
} else {
  await runToolTests("integration", testCases);
}
