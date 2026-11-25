import Foundation
import OSLog
import QuickLookUI
import UniformTypeIdentifiers

/// Quick Look preview of a CoCo or Dragon disk image: its format and track
/// layout, then a directory listing for each filesystem it could hold.
@objc(TSPreviewProvider)
final class PreviewProvider: QLPreviewProvider, QLPreviewingController {
    private static let log = Logger(subsystem: "org.nitros9.toolshed.quicklook", category: "preview")

    func providePreview(for request: QLFilePreviewRequest) async throws -> QLPreviewReply {
        let html: String
        do {
            html = try Listing.html(for: request.fileURL)
        } catch {
            Self.log.info("No preview of \(request.fileURL.lastPathComponent, privacy: .public): \(String(describing: error), privacy: .public)")
            throw error
        }
        let data = Data(html.utf8)
        Self.log.info("Previewed \(request.fileURL.lastPathComponent, privacy: .public) in \(data.count) bytes")

        return QLPreviewReply(dataOfContentType: .html, contentSize: CGSize(width: 760, height: 900)) { reply in
            reply.stringEncoding = .utf8
            return data
        }
    }
}

enum ListingError: Error {
    case notDiskImage
    case captureFailed
}

enum Listing {
    private struct Section {
        let title: String
        let command: tsql_command
        let filesystem: UInt32
    }

    private static let sections = [
        Section(title: "OS-9", command: TSQL_DIR_OS9, filesystem: UInt32(TSQL_FS_OS9)),
        Section(title: "Disk BASIC", command: TSQL_DIR_DECB, filesystem: UInt32(TSQL_FS_DECB)),
        Section(title: "Dragon DOS", command: TSQL_DIR_DRAGONDOS, filesystem: UInt32(TSQL_FS_DRAGONDOS)),
        Section(title: "FLEX", command: TSQL_DIR_FLEX, filesystem: UInt32(TSQL_FS_FLEX)),
    ]

    static func html(for url: URL) throws -> String {
        let name = url.lastPathComponent
        let image = try ImagePath(url)
        defer { image.cleanUp() }

        var filesystems: UInt32 = 0
        guard tsql_detect(image.path, &filesystems) == 0 else {
            throw ListingError.notDiskImage
        }

        let tidy = { (text: String) in text.replacingOccurrences(of: image.path, with: name) }
        let info = try run(TSQL_INFO, image.path)

        var body = "<header><h1>\(escape(name))</h1></header>\n"
        body += "<pre class=\"info\">\(escape(tidy(info.output)))</pre>\n"

        let found = sections.filter { filesystems & $0.filesystem != 0 }
        if found.isEmpty {
            body += "<p class=\"note\">No OS-9, Disk BASIC, Dragon DOS or FLEX filesystem was found.</p>\n"
        }
        for section in found {
            let result = try run(section.command, image.path)
            body += "<section><h2>\(section.title)</h2>\n"
            if !result.output.isEmpty {
                body += "<pre>\(escape(tidy(result.output)))</pre>\n"
            }
            if let errors = result.errors {
                body += "<pre class=\"errors\">\(escape(tidy(errors)))</pre>\n"
            }
            body += "</section>\n"
        }

        return page(title: name, body: body)
    }

    private static func run(_ command: tsql_command, _ path: String) throws -> (output: String, errors: String?) {
        var errors: UnsafeMutablePointer<CChar>?
        var status: Int32 = 0

        guard let output = tsql_run(command, path, &errors, &status) else {
            throw ListingError.captureFailed
        }
        defer {
            free(output)
            free(errors)
        }

        let text = String(decoding: Data(bytes: output, count: strlen(output)), as: UTF8.self)
        let errorText = errors.map { String(decoding: Data(bytes: $0, count: strlen($0)), as: UTF8.self) }

        return (text.trimmingCharacters(in: .newlines), errorText?.trimmingCharacters(in: .newlines))
    }

    private static func escape(_ text: String) -> String {
        var escaped = ""
        escaped.reserveCapacity(text.utf8.count)
        for character in text {
            switch character {
            case "&": escaped += "&amp;"
            case "<": escaped += "&lt;"
            case ">": escaped += "&gt;"
            case "\"": escaped += "&quot;"
            default: escaped.append(character)
            }
        }
        return escaped
    }

    private static func page(title: String, body: String) -> String {
        """
        <!doctype html>
        <html><head><meta charset="utf-8"><title>\(escape(title))</title>
        <style>
        :root { color-scheme: light dark; --fg: #1d1d1f; --muted: #6e6e73; --bg: #ffffff;
                --panel: #f5f5f7; --rule: #d2d2d7; --error: #c4281c; }
        @media (prefers-color-scheme: dark) {
          :root { --fg: #f5f5f7; --muted: #98989d; --bg: #1e1e1e; --panel: #2a2a2c;
                  --rule: #3a3a3c; --error: #ff6b5e; }
        }
        body { margin: 0; padding: 16px 20px 24px; background: var(--bg); color: var(--fg);
               font: 13px -apple-system, system-ui, sans-serif; }
        h1 { font-size: 17px; font-weight: 600; margin: 0 0 8px; word-break: break-all; }
        h2 { font-size: 13px; font-weight: 600; color: var(--muted); text-transform: uppercase;
             letter-spacing: 0.04em; margin: 20px 0 6px; padding-bottom: 4px;
             border-bottom: 1px solid var(--rule); }
        pre { font: 12px ui-monospace, Menlo, monospace; margin: 0; white-space: pre;
              overflow-x: auto; }
        pre.info { color: var(--muted); white-space: pre-wrap; }
        pre.errors { color: var(--error); margin-top: 8px; white-space: pre-wrap; }
        p.note { color: var(--muted); }
        </style></head>
        <body>
        \(body)</body></html>
        """
    }
}

/// The image's path as ToolShed can take it.  A pathlist ends the image name
/// at the first comma, so an image whose path has one is reached through a
/// link that doesn't.
private struct ImagePath {
    let path: String
    private let linkDirectory: URL?

    init(_ url: URL) throws {
        let original = url.path
        guard original.contains(",") else {
            path = original
            linkDirectory = nil
            return
        }

        let directory = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let link = directory.appendingPathComponent("image" + (url.pathExtension.isEmpty ? "" : "." + url.pathExtension))
        try FileManager.default.createSymbolicLink(atPath: link.path, withDestinationPath: original)

        path = link.path
        linkDirectory = directory
    }

    func cleanUp() {
        if let linkDirectory {
            try? FileManager.default.removeItem(at: linkDirectory)
        }
    }
}
