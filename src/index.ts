import {
  createServer,
  routeStdoutLogsToStderr,
} from './server/server-factory.js';
import { startStdioServer } from './server/stdio-lifecycle.js';

routeStdoutLogsToStderr();

export {
  createServer,
  startStdioServer,
};
