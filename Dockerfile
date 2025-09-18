# Multi-stage build for efficient image size
FROM node:22-alpine AS builder

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
FROM cgr.dev/chainguard/node:latest

ENV NODE_ENV=production

# Chainguard node runs as nonroot by default (user: node)
WORKDIR /app

# Copy only package manifests and install production deps in a clean layer
COPY --chown=node:node package*.json ./
RUN npm ci --omit=dev --ignore-scripts && npm cache clean --force

# Copy built application from builder stage
COPY --chown=node:node --from=builder /app/dist ./dist

# No shell, no init needed; run as provided nonroot user
ENTRYPOINT ["/usr/bin/node"]
CMD ["dist/cli.js"]
