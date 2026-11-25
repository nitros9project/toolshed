import AppKit

// The app only carries the Quick Look extension and the disk image type, and
// opening it once makes macOS find both.  It says so, then quits.

let app = NSApplication.shared
app.setActivationPolicy(.regular)
app.activate(ignoringOtherApps: true)

let alert = NSAlert()
alert.messageText = "ToolShed Quick Look is installed"
alert.informativeText = """
    Select a CoCo or Dragon disk image (.dsk, .dmk, .vdk, .sdf, .jvc or .os9) \
    in the Finder and press Space to see its directory.

    If a preview doesn't appear, check that "ToolShed Disk Image Preview" is \
    turned on in System Settings > General > Login Items & Extensions > Quick Look.
    """
alert.addButton(withTitle: "OK")
alert.runModal()
