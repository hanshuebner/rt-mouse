// swift-tools-version:5.5
import PackageDescription

let package = Package(
    name: "MouseEmulator",
    platforms: [
        .macOS(.v10_15)
    ],
    dependencies: [
        .package(url: "https://github.com/yeokm1/SwiftSerial.git", from: "0.1.1")
    ],
    targets: [
        .executableTarget(
            name: "MouseEmulator",
            dependencies: [.product(name: "SwiftSerial", package: "SwiftSerial")]
        )
    ]
) 
