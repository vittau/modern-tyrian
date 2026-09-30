// Compose the real 16:10 capture into the supplied Valve frame, without AI.
// Usage: swift docs/screenshots/compose-deck.swift FRAME.png CAPTURE.png OUTPUT.png
import AppKit

if CommandLine.arguments.count != 4 {
    fatalError("Usage: compose-deck.swift FRAME.png CAPTURE.png OUTPUT.png")
}
func load(_ path: String) throws -> NSBitmapImageRep {
    guard let image = NSBitmapImageRep(data: try Data(contentsOf: URL(fileURLWithPath: path))) else {
        fatalError("Cannot read image: \(path)")
    }
    return image
}
let frame = try load(CommandLine.arguments[1])
let capture = try load(CommandLine.arguments[2])
precondition(frame.pixelsWide == 2500 && frame.pixelsHigh == 980, "Expected the supplied Valve frame")
precondition(capture.pixelsWide == 1280 && capture.pixelsHigh == 800, "Expected the native Deck capture")

// Active display only, in top-left pixel coordinates. Every bezel pixel stays intact.
let screenX = 759, screenY = 235, screenWidth = 982, screenHeight = 614
for y in 0..<screenHeight {
    for x in 0..<screenWidth {
        let sourceX = x * capture.pixelsWide / screenWidth
        let sourceY = y * capture.pixelsHigh / screenHeight
        frame.setColor(capture.colorAt(x: sourceX, y: sourceY)!, atX: screenX + x, y: screenY + y)
    }
}
guard let png = frame.representation(using: .png, properties: [:]) else {
    fatalError("Cannot encode PNG")
}
try png.write(to: URL(fileURLWithPath: CommandLine.arguments[3]))
print("Composed screenshot into the active display; original hardware and bezels preserved")
