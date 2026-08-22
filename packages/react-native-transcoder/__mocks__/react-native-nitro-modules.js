module.exports = {
  NitroModules: {
    createHybridObject: () => {
      throw new Error(
        'react-native-transcoder requires a native build — mock it in unit tests, or run it through react-native-harness on a device.'
      )
    },
  },
}
