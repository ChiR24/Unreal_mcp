# Multi-stage build for efficient image size
FROM node:20-alpine AS builder

# Set working directory
WORKDIR /app

# Copy package files
COPY package*.json ./

# Install ALL dependencies (including dev) for building - skip prepare script
RUN npm ci --ignore-scripts

# Copy TypeScript config and source files
COPY tsconfig.json ./
COPY src ./src

# Build the TypeScript project
RUN npm run build

# Production stage
FROM node:20-alpine

# Install dumb-init for proper signal handling
RUN apk add --no-cache dumb-init

# Create non-root user
RUN addgroup -g 1001 -S nodejs && \
    adduser -S nodejs -u 1001

# Set working directory
WORKDIR /app

# Copy package files
COPY package*.json ./

# Install production dependencies only (skip prepare script)
RUN npm ci --only=production --ignore-scripts && \
    npm cache clean --force

# Copy built application from builder stage
COPY --from=builder /app/dist ./dist

# Change ownership to nodejs user
RUN chown -R nodejs:nodejs /app

# Switch to non-root user
USER nodejs

# MCP servers use stdio, no port exposure needed
# Use dumb-init to handle signals properly
ENTRYPOINT ["dumb-init", "--"]

# Run the MCP server
CMD ["node", "dist/cli.js"]
