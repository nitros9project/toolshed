#include "disk_image.hpp"

#include <wx/artprov.h>
#include <wx/dataobj.h>
#include <wx/dataview.h>
#include <wx/datetime.h>
#include <wx/dnd.h>
#include <wx/dialog.h>
#include <wx/ffile.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/platinfo.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/toolbar.h>
#include <wx/wx.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {

enum CommandId {
    id_back = wxID_HIGHEST + 1,
    id_up,
    id_export,
    id_refresh,
    id_import,
    id_new_folder,
    id_rename,
    id_delete,
    id_open_entry,
    id_ident,
};

wxString display_path(const std::string& path)
{
    return path.empty() ? "/" : wxString::FromUTF8("/" + path);
}

std::string parent_path(const std::string& path)
{
    const auto slash = path.rfind('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

class HostFileDropTarget final : public wxFileDropTarget {
public:
    explicit HostFileDropTarget(std::function<bool(const wxArrayString&)> callback)
        : callback_(std::move(callback))
    {
    }

    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override
    {
        return callback_(filenames);
    }

private:
    std::function<bool(const wxArrayString&)> callback_;
};

class MainFrame final : public wxFrame {
public:
    MainFrame()
        : wxFrame(nullptr, wxID_ANY, "DiskShed", wxDefaultPosition,
                  wxSize(940, 590))
    {
        BuildMenus();
        BuildToolbar();
        BuildContent();

        CreateStatusBar();
        SetStatusText("No disk image open");
        UpdateCommands();

        Bind(wxEVT_MENU, &MainFrame::OnOpen, this, wxID_OPEN);
        Bind(wxEVT_MENU, &MainFrame::OnBack, this, id_back);
        Bind(wxEVT_MENU, &MainFrame::OnUp, this, id_up);
        Bind(wxEVT_MENU, &MainFrame::OnImport, this, id_import);
        Bind(wxEVT_MENU, &MainFrame::OnExport, this, id_export);
        Bind(wxEVT_MENU, &MainFrame::OnNewFolder, this, id_new_folder);
        Bind(wxEVT_MENU, &MainFrame::OnRename, this, id_rename);
        Bind(wxEVT_MENU, &MainFrame::OnDelete, this, id_delete);
        Bind(wxEVT_MENU, &MainFrame::OnOpenEntry, this, id_open_entry);
        Bind(wxEVT_MENU, &MainFrame::OnIdent, this, id_ident);
        Bind(wxEVT_MENU, &MainFrame::OnRefresh, this, id_refresh);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
        list_->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &MainFrame::OnActivate, this);
        list_->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &MainFrame::OnSelection, this);
        list_->Bind(wxEVT_DATAVIEW_ITEM_BEGIN_DRAG, &MainFrame::OnBeginDrag, this);
        list_->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, &MainFrame::OnContextMenu, this);
        list_->SetDropTarget(new HostFileDropTarget(
            [this](const wxArrayString& files) { return ImportHostFiles(files); }));

        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        drag_directory_ = std::filesystem::temp_directory_path() /
            ("diskshed-drag-" + std::to_string(unique));
    }

    ~MainFrame() override
    {
        std::error_code ignored;
        std::filesystem::remove_all(drag_directory_, ignored);
    }

    bool HasImage() const { return static_cast<bool>(image_); }

    bool OpenImage(const wxString& filename)
    {
        try {
            auto image = std::make_unique<toolshed::DiskImage>(
                toolshed::DiskImage::open(filename.ToStdWstring()));
            image_ = std::move(image);
            current_directory_.clear();
            history_.clear();
            heading_->SetLabel(wxFileName(filename).GetFullName());
            SetTitle(heading_->GetLabel() + " — DiskShed");
            ConfigureColumns();
            LoadDirectory();
            return true;
        } catch (const std::exception& error) {
            ShowError("Unable to open disk image", error);
            return false;
        }
    }

private:
    void BuildMenus()
    {
        auto* file_menu = new wxMenu;
        file_menu->Append(wxID_OPEN, "&Open Disk Image...\tCtrl+O");
        file_menu->Append(id_import, "&Import Files...\tCtrl+I");
        file_menu->Append(id_export, "&Export Selected File...\tCtrl+E");
        file_menu->AppendSeparator();
        file_menu->Append(wxID_EXIT);

        auto* view_menu = new wxMenu;
        view_menu->Append(id_back, "&Back\tAlt+Left");
        view_menu->Append(id_up, "&Up One Level\tCtrl+Up");
        view_menu->Append(id_refresh, "&Refresh\tCtrl+R");

        auto* edit_menu = new wxMenu;
        edit_menu->Append(id_new_folder, "New &Folder...\tCtrl+Shift+N");
        edit_menu->Append(id_rename, "&Rename...\tCtrl+Shift+R");
        edit_menu->Append(id_delete, "&Delete...\tCtrl+Backspace");

        auto* menu_bar = new wxMenuBar;
        menu_bar->Append(file_menu, "&File");
        menu_bar->Append(edit_menu, "&Edit");
        menu_bar->Append(view_menu, "&View");
        SetMenuBar(menu_bar);
    }

    void BuildToolbar()
    {
        toolbar_ = CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT);
        toolbar_->AddTool(wxID_OPEN, "Open", wxArtProvider::GetBitmap(wxART_FILE_OPEN));
        toolbar_->AddSeparator();
        toolbar_->AddTool(id_back, "Back", wxArtProvider::GetBitmap(wxART_GO_BACK));
        toolbar_->AddTool(id_up, "Up", wxArtProvider::GetBitmap(wxART_GO_DIR_UP));
        toolbar_->AddTool(id_refresh, "Refresh", wxArtProvider::GetBitmap(wxART_REDO));
        toolbar_->AddSeparator();
        toolbar_->AddTool(id_import, "Import", wxArtProvider::GetBitmap(wxART_PLUS));
        toolbar_->AddTool(id_export, "Export", wxArtProvider::GetBitmap(wxART_FILE_SAVE));
        toolbar_->AddTool(id_new_folder, "New Folder", wxArtProvider::GetBitmap(wxART_NEW_DIR));
        toolbar_->AddSeparator();
        toolbar_->AddTool(id_rename, "Rename", wxArtProvider::GetBitmap(wxART_EDIT));
        toolbar_->AddTool(id_delete, "Delete", wxArtProvider::GetBitmap(wxART_DELETE));
        toolbar_->Realize();
    }

    void BuildContent()
    {
        auto* panel = new wxPanel(this);
        auto* outer = new wxBoxSizer(wxVERTICAL);

        heading_ = new wxStaticText(panel, wxID_ANY, "Open an OS-9 or Disk BASIC image");
        auto heading_font = heading_->GetFont();
        heading_font.MakeLarger();
        heading_font.MakeBold();
        heading_->SetFont(heading_font);
        outer->Add(heading_, 0, wxLEFT | wxRIGHT | wxTOP, 14);

        path_label_ = new wxStaticText(panel, wxID_ANY, "/");
        path_label_->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
        outer->Add(path_label_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 14);

        long list_style = wxDV_MULTIPLE | wxDV_ROW_LINES | wxDV_VERT_RULES;
#ifdef __WXOSX__
        // Sequoia's private NSTableBackgroundView can crash while redrawing a
        // scrolled table that uses native grid-line background drawing. Avoid
        // this path on macOS 15 and earlier, retaining it on newer releases.
        if (wxPlatformInfo::Get().GetOSMajorVersion() <= 15) {
            list_style &= ~(wxDV_ROW_LINES | wxDV_VERT_RULES);
        }
#endif
        list_ = new wxDataViewListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       list_style);
        outer->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);
        panel->SetSizer(outer);
    }

    void ConfigureColumns()
    {
        list_->DeleteAllItems();
        if (image_->format() == toolshed::ImageFormat::disk_basic) {
            list_->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 330, wxALIGN_LEFT,
                                    wxDATAVIEW_COL_RESIZABLE);
            list_->AppendTextColumn("Size", wxDATAVIEW_CELL_INERT, 100, wxALIGN_RIGHT,
                                    wxDATAVIEW_COL_RESIZABLE);
            list_->AppendTextColumn("File Type", wxDATAVIEW_CELL_INERT, 220, wxALIGN_LEFT,
                                    wxDATAVIEW_COL_RESIZABLE);
            list_->AppendTextColumn("Encoding", wxDATAVIEW_CELL_INERT, 120, wxALIGN_LEFT,
                                    wxDATAVIEW_COL_RESIZABLE);
            return;
        }

        list_->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 275, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
        list_->AppendTextColumn("Size", wxDATAVIEW_CELL_INERT, 90, wxALIGN_RIGHT,
                                wxDATAVIEW_COL_RESIZABLE);
        list_->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 145, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
        list_->AppendTextColumn("Attributes", wxDATAVIEW_CELL_INERT, 105, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
        list_->AppendTextColumn("Owner", wxDATAVIEW_CELL_INERT, 70, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
        list_->AppendTextColumn("Modified", wxDATAVIEW_CELL_INERT, 160, wxALIGN_LEFT,
                                wxDATAVIEW_COL_RESIZABLE);
    }

    void OnOpen(wxCommandEvent&)
    {
        wxFileDialog dialog(this, "Open a CoCo disk image", wxEmptyString, wxEmptyString,
                            "Disk images (*.dsk;*.os9)|*.dsk;*.os9|All files|*.*",
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() == wxID_OK) {
            if (!image_) {
                OpenImage(dialog.GetPath());
                return;
            }

            auto* frame = new MainFrame;
            frame->Show();
            if (!frame->OpenImage(dialog.GetPath())) {
                frame->Destroy();
            }
        }
    }

    void OnBack(wxCommandEvent&)
    {
        if (history_.empty()) {
            return;
        }
        current_directory_ = history_.back();
        history_.pop_back();
        LoadDirectory();
    }

    void OnUp(wxCommandEvent&)
    {
        if (current_directory_.empty()) {
            return;
        }
        history_.push_back(current_directory_);
        current_directory_ = parent_path(current_directory_);
        LoadDirectory();
    }

    void OnRefresh(wxCommandEvent&)
    {
        if (image_) {
            LoadDirectory();
        }
    }

    void OnImport(wxCommandEvent&)
    {
        if (!image_) {
            return;
        }
        wxFileDialog dialog(this, "Import files into the disk image", wxEmptyString,
                            wxEmptyString, "All files|*.*",
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
        if (dialog.ShowModal() != wxID_OK) {
            return;
        }
        wxArrayString files;
        dialog.GetPaths(files);
        ImportHostFiles(files);
    }

    bool ImportHostFiles(const wxArrayString& files)
    {
        if (!image_ || files.empty()) {
            return false;
        }

        unsigned int imported = 0;
        for (const auto& filename : files) {
            if (!wxFileName::FileExists(filename)) {
                continue;
            }

            const wxString leaf = wxFileName(filename).GetFullName();
            bool replace = false;
            const auto collision = std::find_if(entries_.begin(), entries_.end(),
                [&leaf](const toolshed::Entry& entry) {
                    return wxString::FromUTF8(entry.name).CmpNoCase(leaf) == 0;
                });
            if (collision != entries_.end()) {
                const int choice = wxMessageBox(
                    wxString::Format("“%s” already exists in this image. Replace it?", leaf),
                    "Replace existing file", wxYES_NO | wxCANCEL | wxICON_WARNING, this);
                if (choice == wxCANCEL) {
                    break;
                }
                if (choice == wxNO) {
                    continue;
                }
                replace = true;
            }

            try {
                image_->import_file(current_directory_, filename.ToStdWstring(), replace);
                ++imported;
            } catch (const std::exception& error) {
                ShowError(wxString::Format("Unable to import %s", leaf), error);
            }
        }

        if (imported != 0) {
            LoadDirectory();
            ShowMutationStatus(wxString::Format("Imported %u file(s)", imported));
            return true;
        }
        return false;
    }

    void OnNewFolder(wxCommandEvent&)
    {
        if (!image_ || image_->format() != toolshed::ImageFormat::os9) {
            return;
        }
        wxTextEntryDialog dialog(this, "Name for the new OS-9 directory:", "New Folder");
        if (dialog.ShowModal() != wxID_OK || dialog.GetValue().empty()) {
            return;
        }
        try {
            image_->make_directory(current_directory_, dialog.GetValue().ToStdString());
            LoadDirectory();
            ShowMutationStatus("Created directory " + dialog.GetValue());
        } catch (const std::exception& error) {
            ShowError("Unable to create directory", error);
        }
    }

    void OnRename(wxCommandEvent&)
    {
        const int row = list_->GetSelectedRow();
        if (!image_ || row < 0 || static_cast<std::size_t>(row) >= entries_.size()) {
            return;
        }
        const auto entry = entries_[row];
        wxTextEntryDialog dialog(this, "New name:", "Rename Image Entry",
                                 wxString::FromUTF8(entry.name));
        if (dialog.ShowModal() != wxID_OK || dialog.GetValue().empty() ||
            dialog.GetValue() == wxString::FromUTF8(entry.name)) {
            return;
        }
        try {
            image_->rename_entry(entry.image_path, dialog.GetValue().ToStdString());
            LoadDirectory();
            ShowMutationStatus("Renamed " + wxString::FromUTF8(entry.name));
        } catch (const std::exception& error) {
            ShowError("Unable to rename entry", error);
        }
    }

    void OnDelete(wxCommandEvent&)
    {
        const int row = list_->GetSelectedRow();
        if (!image_ || row < 0 || static_cast<std::size_t>(row) >= entries_.size()) {
            return;
        }
        const auto entry = entries_[row];
        wxString question = wxString::Format("Delete “%s” from the image?",
                                              wxString::FromUTF8(entry.name));
        if (entry.directory) {
            question += "\n\nThis also deletes everything inside the directory.";
        }
        question += "\n\nDiskShed will preserve a backup of the original image.";
        if (wxMessageBox(question, "Delete Image Entry",
                         wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) {
            return;
        }
        try {
            image_->delete_entry(entry.image_path, entry.directory);
            LoadDirectory();
            ShowMutationStatus("Deleted " + wxString::FromUTF8(entry.name));
        } catch (const std::exception& error) {
            ShowError("Unable to delete entry", error);
        }
    }

    void NavigateTo(const toolshed::Entry& entry)
    {
        if (!entry.directory || navigation_pending_) {
            return;
        }
        const auto previous_directory = current_directory_;
        const auto next_directory = entry.image_path;
        navigation_pending_ = true;
        CallAfter([this, previous_directory, next_directory] {
            navigation_pending_ = false;
            if (!image_) {
                return;
            }
            history_.push_back(previous_directory);
            current_directory_ = next_directory;
            LoadDirectory();
        });
    }

    void OnOpenEntry(wxCommandEvent&)
    {
        const auto rows = SelectedRows();
        if (rows.size() == 1) {
            NavigateTo(entries_[rows.front()]);
        }
    }

    void OnIdent(wxCommandEvent&)
    {
        const auto rows = SelectedRows();
        if (!image_ || rows.size() != 1 || entries_[rows.front()].directory ||
            !image_->has_module_signature(entries_[rows.front()].image_path)) {
            return;
        }

        const auto& entry = entries_[rows.front()];
        try {
            const auto details = image_->identify_file(entry.image_path);
            wxDialog dialog(this, wxID_ANY,
                            "Ident — " + wxString::FromUTF8(entry.name),
                            wxDefaultPosition, wxSize(650, 520),
                            wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
            auto* layout = new wxBoxSizer(wxVERTICAL);
            auto* text = new wxTextCtrl(&dialog, wxID_ANY, wxString::FromUTF8(details),
                                        wxDefaultPosition, wxDefaultSize,
                                        wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
            text->SetFont(wxFontInfo(13).Family(wxFONTFAMILY_TELETYPE));
            layout->Add(text, 1, wxEXPAND | wxALL, 12);
            layout->Add(dialog.CreateSeparatedButtonSizer(wxOK), 0,
                        wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
            dialog.SetSizer(layout);
            dialog.ShowModal();
        } catch (const std::exception& error) {
            ShowError("Unable to identify OS-9 module", error);
        }
    }

    void OnContextMenu(wxDataViewEvent& event)
    {
        const auto item = event.GetItem();
        if (item.IsOk() && !list_->IsSelected(item)) {
            list_->UnselectAll();
            list_->Select(item);
        }

        const auto rows = SelectedRows();
        const bool single = rows.size() == 1;
        const bool folder = single && entries_[rows.front()].directory;
        const bool file = single && !folder;
        const bool identifiable_file = file && image_ &&
            image_->has_module_signature(entries_[rows.front()].image_path);

        wxMenu menu;
        if (folder) {
            menu.Append(id_open_entry, "&Open");
            menu.AppendSeparator();
        } else if (file) {
            if (identifiable_file) {
                menu.Append(id_ident, "&Ident…");
            }
            menu.Append(id_export, "&Export…");
            menu.AppendSeparator();
        }

        menu.Append(id_rename, "&Rename…");
        menu.Append(id_delete, "&Delete…");
        menu.Enable(id_rename, single);
        menu.Enable(id_delete, single);
        menu.AppendSeparator();
        menu.Append(id_import, "&Import Files…");
        if (image_ && image_->format() == toolshed::ImageFormat::os9) {
            menu.Append(id_new_folder, "New &Folder…");
        }
        menu.Append(id_refresh, "&Refresh");
        PopupMenu(&menu);
    }

    void OnActivate(wxDataViewEvent& event)
    {
        if (navigation_pending_) {
            return;
        }
        const int row = list_->ItemToRow(event.GetItem());
        if (row < 0 || static_cast<std::size_t>(row) >= entries_.size()) {
            return;
        }
        const auto& entry = entries_[row];
        if (entry.directory) {
            // Keep directory model replacement outside the activation callback.
            NavigateTo(entry);
        } else {
            ExportEntry(entry);
        }
    }

    void OnSelection(wxDataViewEvent&)
    {
        if (reloading_list_) {
            return;
        }
        // A native drag must begin promptly. Prepare selected image files before
        // the user starts moving the mouse so multi-file extraction cannot stall it.
        try {
            MaterializeRows(SelectedRows());
        } catch (const std::exception&) {
            // OnBeginDrag reports preparation errors if the selection is dragged.
        }
        UpdateCommands();
    }

    void OnBeginDrag(wxDataViewEvent& event)
    {
        auto rows = SelectedRows();
        const int event_row = list_->ItemToRow(event.GetItem());
        if (event_row >= 0 && static_cast<std::size_t>(event_row) < entries_.size() &&
            std::find(rows.begin(), rows.end(), static_cast<std::size_t>(event_row)) ==
                rows.end()) {
            rows.push_back(static_cast<std::size_t>(event_row));
        }
        if (!image_ || rows.empty()) {
            event.Veto();
            return;
        }

        try {
            const auto paths = MaterializeRows(rows);
            if (paths.empty()) {
                event.Veto();
                return;
            }

            auto files = std::make_unique<wxFileDataObject>();
            for (const auto& path : paths) {
                files->AddFile(wxString(path.wstring()));
            }
            event.SetDataObject(files.release());
            event.SetDragFlags(wxDrag_CopyOnly);
            event.Allow();
        } catch (const std::exception& error) {
            event.Veto();
            ShowError("Unable to drag file", error);
        }
    }

    void OnExport(wxCommandEvent&)
    {
        const int row = list_->GetSelectedRow();
        if (row < 0 || static_cast<std::size_t>(row) >= entries_.size()) {
            return;
        }
        ExportEntry(entries_[row]);
    }

    void ExportEntry(const toolshed::Entry& entry)
    {
        if (!image_ || entry.directory) {
            return;
        }

        wxFileDialog dialog(this, "Export file", wxEmptyString,
                            wxString::FromUTF8(entry.name), "All files|*.*",
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() != wxID_OK) {
            return;
        }

        try {
            const auto bytes = image_->read_file(entry.image_path);
            wxFFile output(dialog.GetPath(), "wb");
            if (!output.IsOpened() ||
                (bytes.size() != 0 && output.Write(bytes.data(), bytes.size()) != bytes.size())) {
                throw std::runtime_error("Could not write the exported file");
            }
            SetStatusText(wxString::Format("Exported %s • %s",
                                            wxString::FromUTF8(entry.name),
                                            wxFileName::GetHumanReadableSize(wxULongLong(
                                                static_cast<unsigned long long>(bytes.size())))));
        } catch (const std::exception& error) {
            ShowError("Unable to export file", error);
        }
    }

    void LoadDirectory()
    {
        try {
            auto next_entries = image_->list_directory(current_directory_);
            std::error_code ignored;
            std::filesystem::remove_all(drag_directory_, ignored);

            // DeleteAllItems() sends selection notifications on some ports. Keep
            // those callbacks from observing a mixture of the old native model
            // and the new entry vector, and avoid intermediate Cocoa redraws.
            reloading_list_ = true;
            list_->Freeze();
            list_->DeleteAllItems();
            entries_ = std::move(next_entries);
            for (const auto& entry : entries_) {
                wxVector<wxVariant> row;
                row.push_back(wxVariant(wxString::FromUTF8(entry.name)));
                row.push_back(wxVariant(entry.directory ? wxString() :
                    wxFileName::GetHumanReadableSize(wxULongLong(
                        static_cast<unsigned long long>(entry.size)))));
                row.push_back(wxVariant(wxString::FromUTF8(entry.type)));
                if (image_->format() == toolshed::ImageFormat::disk_basic) {
                    row.push_back(wxVariant(wxString::FromUTF8(entry.encoding)));
                } else {
                    row.push_back(wxVariant(wxString::FromUTF8(entry.attributes)));
                    row.push_back(wxVariant(wxString::Format(
                        "%u.%u", entry.group_id, entry.user_id)));
                    row.push_back(wxVariant(entry.modified_time > 0
                        ? wxDateTime(static_cast<time_t>(entry.modified_time)).Format(
                            "%b %e, %Y  %H:%M")
                        : wxString()));
                }
                list_->AppendItem(row);
            }
            list_->Thaw();
            reloading_list_ = false;

            path_label_->SetLabel(display_path(current_directory_));
            SetStatusText(wxString::Format("%s • %zu entries",
                                            wxString::FromUTF8(image_->format_name()),
                                            entries_.size()));
            UpdateCommands();
        } catch (const std::exception& error) {
            if (reloading_list_) {
                list_->Thaw();
                reloading_list_ = false;
            }
            ShowError("Unable to read directory", error);
        }
    }

    std::vector<std::size_t> SelectedRows() const
    {
        wxDataViewItemArray selections;
        list_->GetSelections(selections);
        std::vector<std::size_t> rows;
        rows.reserve(selections.size());
        for (const auto& item : selections) {
            const int row = list_->ItemToRow(item);
            if (row >= 0 && static_cast<std::size_t>(row) < entries_.size()) {
                rows.push_back(static_cast<std::size_t>(row));
            }
        }
        return rows;
    }

    std::vector<std::filesystem::path> MaterializeRows(
        const std::vector<std::size_t>& rows)
    {
        std::filesystem::create_directories(drag_directory_);
        std::vector<std::filesystem::path> paths;
        paths.reserve(rows.size());
        for (const auto row : rows) {
            if (row >= entries_.size() || entries_[row].directory) {
                continue;
            }

            const auto& entry = entries_[row];
            const auto extracted = drag_directory_ / entry.name;
            if (!std::filesystem::is_regular_file(extracted)) {
                const auto bytes = image_->read_file(entry.image_path);
                std::ofstream output(extracted, std::ios::binary | std::ios::trunc);
                output.write(reinterpret_cast<const char*>(bytes.data()),
                             static_cast<std::streamsize>(bytes.size()));
                if (!output) {
                    throw std::runtime_error("Could not prepare a dragged file");
                }
            }
            paths.push_back(extracted);
        }
        return paths;
    }

    void UpdateCommands()
    {
        const auto rows = list_ ? SelectedRows() : std::vector<std::size_t>{};
        const bool has_selection = !rows.empty();
        const bool has_single_selection = rows.size() == 1;
        const bool can_export = image_ && has_single_selection &&
            !entries_[rows.front()].directory;
        toolbar_->EnableTool(id_back, !history_.empty());
        toolbar_->EnableTool(id_up, image_ && !current_directory_.empty());
        toolbar_->EnableTool(id_refresh, image_ != nullptr);
        toolbar_->EnableTool(id_import, image_ != nullptr);
        toolbar_->EnableTool(id_export, can_export);
        toolbar_->EnableTool(id_new_folder, image_ &&
            image_->format() == toolshed::ImageFormat::os9);
        toolbar_->EnableTool(id_rename, has_single_selection);
        toolbar_->EnableTool(id_delete, has_single_selection);
        GetMenuBar()->Enable(id_back, !history_.empty());
        GetMenuBar()->Enable(id_up, image_ && !current_directory_.empty());
        GetMenuBar()->Enable(id_refresh, image_ != nullptr);
        GetMenuBar()->Enable(id_import, image_ != nullptr);
        GetMenuBar()->Enable(id_export, can_export);
        GetMenuBar()->Enable(id_new_folder, image_ &&
            image_->format() == toolshed::ImageFormat::os9);
        GetMenuBar()->Enable(id_rename, has_single_selection);
        GetMenuBar()->Enable(id_delete, has_single_selection);
    }

    void ShowMutationStatus(const wxString& action)
    {
        wxString status = action;
        if (image_ && image_->backup_path()) {
            status += " • Backup: " + wxString(image_->backup_path()->filename().wstring());
        }
        SetStatusText(status);
    }

    void ShowError(const wxString& title, const std::exception& error)
    {
        wxMessageBox(wxString::FromUTF8(error.what()), title, wxOK | wxICON_ERROR, this);
    }

    wxToolBar* toolbar_ = nullptr;
    wxStaticText* heading_ = nullptr;
    wxStaticText* path_label_ = nullptr;
    wxDataViewListCtrl* list_ = nullptr;
    std::unique_ptr<toolshed::DiskImage> image_;
    std::vector<toolshed::Entry> entries_;
    std::string current_directory_;
    std::vector<std::string> history_;
    std::filesystem::path drag_directory_;
    bool navigation_pending_ = false;
    bool reloading_list_ = false;
};

class DiskShedApp final : public wxApp {
public:
    bool OnInit() override
    {
        SetAppName("DiskShed");
        auto* frame = new MainFrame;
        frame->Show();
        for (int index = 1; index < argc; ++index) {
            OpenImageWindow(argv[index]);
        }
        return true;
    }

#ifdef __WXOSX__
    // Finder and the Dock send images dropped on the app icon (and Open With)
    // here, both at launch and while the app is running.
    void MacOpenFiles(const wxArrayString& files) override
    {
        for (const auto& file : files) {
            OpenImageWindow(file);
        }
    }
#endif

private:
    // Opens the image in the first window that has none, or else in a new
    // window, matching File > Open.
    static void OpenImageWindow(const wxString& filename)
    {
        for (auto* window : wxTopLevelWindows) {
            auto* frame = dynamic_cast<MainFrame*>(window);
            if (frame && !frame->HasImage() && !frame->IsBeingDeleted()) {
                frame->OpenImage(filename);
                frame->Raise();
                return;
            }
        }

        auto* frame = new MainFrame;
        frame->Show();
        if (!frame->OpenImage(filename)) {
            frame->Destroy();
        }
    }
};

}  // namespace

wxIMPLEMENT_APP(DiskShedApp);
