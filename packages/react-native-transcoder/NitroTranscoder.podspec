require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "NitroTranscoder"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported, :visionos => 1.0, :tvos => "15.0", :osx => "11.0" }
  s.source       = { :git => "https://github.com/riteshshukla04/react-native-transcoder.git", :tag => "#{s.version}" }

  s.source_files = [
    # Autolinking/Registration (Objective-C++)
    "ios/**/*.{m,mm}",
    # Implementation (C++ objects + engine)
    "cpp/**/*.{hpp,cpp}",
  ]

  # LGPL FFmpeg ships as dynamically linked, app-embedded XCFrameworks so the
  # components stay replaceable.
  s.vendored_frameworks = "dependencies/prebuilt/ios/*.xcframework"

  s.resource_bundles = {
    "NitroTranscoderPrivacy" => ["ios/PrivacyInfo.xcprivacy"]
  }

  s.compiler_flags = '-x objective-c++'
  s.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
    "HEADER_SEARCH_PATHS" => [
      "\"$(PODS_TARGET_SRCROOT)/cpp\"",
      "\"$(PODS_TARGET_SRCROOT)/cpp/engine/include\"",
      "\"$(PODS_TARGET_SRCROOT)/cpp/hybrid\"",
      "\"$(PODS_TARGET_SRCROOT)/dependencies/prebuilt/ios/include\"",
    ].join(" "),
    "GCC_PREPROCESSOR_DEFINITIONS" => "$(inherited) TRANSCODER_VERSION=\\\"#{package["version"]}\\\"",
  }

  s.frameworks = "AudioToolbox"

  load 'nitrogen/generated/ios/NitroTranscoder+autolinking.rb'
  add_nitrogen_files(s)

  s.dependency 'React-jsi'
  s.dependency 'React-callinvoker'
  install_modules_dependencies(s)
end
