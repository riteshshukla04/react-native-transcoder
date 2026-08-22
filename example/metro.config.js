const { getDefaultConfig, mergeConfig } = require('@react-native/metro-config')
const path = require('path')

const projectRoot = __dirname
const workspaceRoot = path.resolve(projectRoot, '..')
const rootNodeModules = path.resolve(workspaceRoot, 'node_modules')

// Everything is hoisted to the repo root, so singletons must resolve there.
const SINGLETONS = ['react', 'react-native', 'react-native-nitro-modules']

/** @type {import('@react-native/metro-config').MetroConfig} */
const overrides = {
  watchFolders: [workspaceRoot],
  resolver: {
    nodeModulesPaths: [rootNodeModules],
  },
}

const config = mergeConfig(getDefaultConfig(projectRoot), overrides)

const upstreamResolveRequest = config.resolver.resolveRequest
config.resolver.resolveRequest = (context, moduleName, platform) => {
  for (const name of SINGLETONS) {
    if (moduleName === name || moduleName.startsWith(name + '/')) {
      try {
        return {
          type: 'sourceFile',
          filePath: require.resolve(moduleName, { paths: [rootNodeModules] }),
        }
      } catch {
        // fall through to default resolution
      }
    }
  }
  if (upstreamResolveRequest) {
    return upstreamResolveRequest(context, moduleName, platform)
  }
  return context.resolveRequest(context, moduleName, platform)
}

module.exports = config
