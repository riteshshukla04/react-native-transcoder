import {
  androidPlatform,
  androidEmulator,
  physicalAndroidDevice,
} from '@react-native-harness/platform-android'
import {
  applePlatform,
  appleSimulator,
} from '@react-native-harness/platform-apple'

const config = {
  entryPoint: './index.js',
  appRegistryComponentName: 'TranscoderExample',

  runners: [
    androidPlatform({
      name: 'android',
      device: androidEmulator(
        process.env.HARNESS_ANDROID_AVD ?? 'Pixel_8_API_35',
        {
          apiLevel: Number(process.env.HARNESS_ANDROID_API_LEVEL ?? 35),
          profile: process.env.HARNESS_ANDROID_PROFILE ?? 'pixel_6',
          diskSize: '2G',
          heapSize: '1G',
        }
      ),
      bundleId: 'com.transcoderexample',
    }),
    androidPlatform({
      name: 'android-device',
      device: physicalAndroidDevice(
        process.env.HARNESS_ANDROID_MANUFACTURER ?? 'samsung',
        process.env.HARNESS_ANDROID_MODEL ?? 'SM-E146B'
      ),
      bundleId: 'com.transcoderexample',
    }),
    applePlatform({
      name: 'ios',
      device: appleSimulator(
        process.env.HARNESS_IOS_DEVICE ?? 'iPhone 17 Pro',
        process.env.HARNESS_IOS_VERSION ?? '26.5'
      ),
      bundleId: 'org.reactjs.native.example.TranscoderExample',
    }),
  ],
  defaultRunner: 'ios',
  bridgeTimeout: 120000,

  resetEnvironmentBetweenTestFiles: true,
}

export default config
