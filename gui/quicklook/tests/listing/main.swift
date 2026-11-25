import Foundation

// Writes the HTML the extension would show for each image named, so the
// page can be checked without Quick Look:  build/listing image... > page.html
for path in CommandLine.arguments.dropFirst() {
    do {
        print(try Listing.html(for: URL(fileURLWithPath: path)))
    } catch {
        FileHandle.standardError.write(Data("\(path): \(error)\n".utf8))
        exit(1)
    }
}
