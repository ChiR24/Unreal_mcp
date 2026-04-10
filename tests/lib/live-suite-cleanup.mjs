const DEFAULT_RETURN_LEVEL_PATH = "/Game/NewMap";

const DEFAULT_LIVE_SUITE_CONTENT_PATHS = [
  "/Game/IntegrationTest",
  "/Game/AdvancedIntegrationTest",
  "/Game/UI/WBP_TestWidget",
  "/Game/UI/WBP_SemanticNavigation",
  "/Game/UI/WBP_PublicSurfaceValidation",
  "/Game/UI/WBP_DesignerMarquee",
  "/Game/UI/WBP_UiTargeting",
  "/Game/UI/WBP_WidgetBindings",
];

function sanitizePathToken(value) {
  return typeof value === "string" && value.trim().length > 0
    ? value.trim()
    : null;
}

function uniquePaths(values) {
  return Array.from(
    new Set(
      (Array.isArray(values) ? values : [])
        .map((value) => sanitizePathToken(value))
        .filter((value) => value !== null),
    ),
  );
}

function buildOperation(name, argumentsObject, timeoutMs) {
  return {
    name,
    arguments: argumentsObject,
    timeoutMs,
  };
}

function tryParseStructuredText(response) {
  if (!Array.isArray(response?.content)) {
    return null;
  }

  for (const entry of response.content) {
    if (entry?.type !== "text" || typeof entry.text !== "string") {
      continue;
    }

    try {
      return JSON.parse(entry.text);
    } catch {}
  }

  return null;
}

function getStructuredResponse(response) {
  if (
    response?.structuredContent &&
    typeof response.structuredContent === "object"
  ) {
    return response.structuredContent;
  }

  return tryParseStructuredText(response);
}

export { DEFAULT_LIVE_SUITE_CONTENT_PATHS, DEFAULT_RETURN_LEVEL_PATH };

export function buildLiveSuiteCleanupOperations(options = {}) {
  const restoreLevelPath =
    sanitizePathToken(options.restoreLevelPath) ?? DEFAULT_RETURN_LEVEL_PATH;
  const extraPaths = Array.isArray(options.extraPaths)
    ? options.extraPaths
    : [];
  const contentPaths = uniquePaths([
    ...DEFAULT_LIVE_SUITE_CONTENT_PATHS,
    ...extraPaths,
  ]);

  const operations = [
    buildOperation(
      "control_editor",
      {
        action: "open_level",
        levelPath: restoreLevelPath,
      },
      20000,
    ),
  ];

  for (const path of contentPaths) {
    operations.push(
      buildOperation(
        "manage_asset",
        {
          action: "delete",
          path,
          force: true,
        },
        15000,
      ),
    );
  }

  return operations;
}

export function buildManagedRunnerCleanupOperations(options = {}) {
  const extraPaths = Array.isArray(options.extraPaths)
    ? options.extraPaths
    : [];
  const operations = buildLiveSuiteCleanupOperations({
    ...options,
    extraPaths: ["/Game/MCPTest", "/Game/SplineBP", ...extraPaths],
  });

  operations.splice(
    1,
    0,
    buildOperation(
      "manage_level",
      {
        action: "unload",
        levelName: "MainLevel",
      },
      10000,
    ),
    buildOperation(
      "manage_level",
      {
        action: "unload",
        levelName: "TestLevel",
      },
      10000,
    ),
  );

  return operations;
}

export async function runBestEffortCleanup(
  callToolOnce,
  operations,
  logger = console,
) {
  if (typeof callToolOnce !== "function") {
    return;
  }

  for (const operation of Array.isArray(operations) ? operations : []) {
    try {
      const response = await callToolOnce(
        {
          name: operation.name,
          arguments: operation.arguments,
        },
        operation.timeoutMs,
      );

      const structuredResponse = getStructuredResponse(response);
      const succeeded =
        response?.isError !== true &&
        (typeof structuredResponse?.success !== "boolean" ||
          structuredResponse.success === true);

      if (!succeeded) {
        throw new Error(
          structuredResponse?.error ||
            structuredResponse?.message ||
            response?.message ||
            "cleanup operation reported success=false",
        );
      }
    } catch (error) {
      if (logger && typeof logger.warn === "function") {
        const scope =
          typeof operation?.arguments?.path === "string"
            ? operation.arguments.path
            : typeof operation?.arguments?.levelName === "string"
              ? operation.arguments.levelName
              : (operation?.name ?? "cleanup-operation");

        logger.warn(
          `⚠️  Cleanup skipped for ${scope}: ${error?.message || error}`,
        );
      }
    }
  }
}
