#include "Device.hpp"

#include "I18N.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp" // for export_zaxe_code
#include "MsgDialog.hpp" // for RichMessageDialog
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Utils.hpp"

namespace Slic3r {
namespace GUI {

Device::Device(NetworkMachine* nm, wxWindow* parent) :
    wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(parent->GetSize().GetWidth(), DEVICE_HEIGHT)),
    nm(nm),
    m_mainSizer(new wxBoxSizer(wxVERTICAL)), // vertical sizer (device sizer - horizontal line (seperator).
    m_deviceSizer(new wxBoxSizer(wxHORIZONTAL)), // horizontal sizer (avatar | right pane).)
    m_filamentSizer(new wxBoxSizer(wxHORIZONTAL)), // horizontal sizer (filament | unload filament button).
    m_expansionSizer(new wxBoxSizer(wxVERTICAL)), // vertical sizer (filament | printing time etc.)
    m_rightSizer(new wxBoxSizer(wxVERTICAL)), // vertical right pane. (name - status - progress bar).
    m_progressBar(new CustomProgressBar(this, wxID_ANY, wxSize(-1, 5))),
    m_txtStatus(new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 18), wxTE_LEFT)),
    m_txtProgress(new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 18), wxTE_RIGHT)),
    m_txtDeviceName(new wxStaticText(this, wxID_ANY, nm->name, wxDefaultPosition, wxSize(-1, 20), wxTE_LEFT)),
    m_txtDeviceMaterial(new wxStaticText(this, wxID_ANY, _L("Material: ") + NetworkMachineManager::MaterialName(nm->attr->material), wxDefaultPosition, wxSize(-1, 20), wxTE_LEFT)),
    m_txtDeviceNozzleDiameter(new wxStaticText(this, wxID_ANY, _L("Nozzle: ") + (nm->attr->isLite ? "-" : nm->attr->nozzle + "mm"), wxDefaultPosition, wxSize(-1, 20), wxTE_LEFT)),
    m_txtDeviceIP(new wxStaticText(this, wxID_ANY, _L("IP Address: ") + nm->ip, wxDefaultPosition, wxSize(-1, 20), wxTE_LEFT)),
    m_txtBedOccupiedMessage(new wxStaticText(this, wxID_ANY, _L("Please take your print!"), wxDefaultPosition, wxSize(-1, 20), wxTE_LEFT)),
    m_txtErrorMessage(new wxStaticText(this, wxID_ANY, _L("Device is in error state!"), wxDefaultPosition, wxSize(-1, 20), wxTE_LEFT)),
    m_txtFileTime(new wxStaticText(this, wxID_ANY, _L("Elapsed / Estimated time: ") + get_time_hms(std::to_string(nm->attr->startTime)) + " / " + nm->attr->estimatedTime, wxDefaultPosition, wxSize(-1, 20), wxTE_LEFT)),
    m_txtFileName(new wxStaticText(this, wxID_ANY, _L("File: ") + nm->attr->printingFile.substr(0, DEVICE_FILENAME_MAX_NUM_CHARS), wxDefaultPosition, wxSize(-1, 20), wxTE_LEFT)),
    m_txtFWVersion(new wxStaticText(this, wxID_ANY, nm->attr->firmwareVersion.GetVersionString(), wxDefaultPosition, wxSize(-1, 10), wxTE_RIGHT)),
    m_btnUnload(new wxButton(this, wxID_ANY, _L("Unload"), wxDefaultPosition, wxSize(-1, 15), wxTE_CENTER)),
    m_btnPrintNow(new wxButton(this, wxID_ANY, _L("Print Now!"))),
    m_avatar(new RoundedPanel(this, wxID_ANY, "", wxSize(60, 60), wxColour(169, 169, 169), wxColour("WHITE"))),
    m_bitPreheatActive(new wxBitmap()),
    m_bitPreheatDeactive(new wxBitmap()),
    m_bitCollapsed(new wxBitmap()),
    m_bitExpanded(new wxBitmap()),
    m_timer(new wxTimer()),
    m_isExpanded(false),
    m_pausedSeconds(0)
{
    SetSizer(m_mainSizer);
    m_mainSizer->Add(m_deviceSizer, 1, wxEXPAND | wxRIGHT, 25); // only expand horizontally in vertical sizer.

    // action buttons with bitmaps.
    wxBitmap bitSayHi;
    bitSayHi.LoadFile(Slic3r::resources_dir() + "/icons/device/hi.png", wxBITMAP_TYPE_PNG);
    m_btnSayHi = new wxBitmapButton(this, wxID_ANY, bitSayHi, wxDefaultPosition, wxSize(32, 20), wxTE_RIGHT);
    m_bitPreheatActive->LoadFile(Slic3r::resources_dir() + "/icons/device/preheat_active.png", wxBITMAP_TYPE_PNG);
    m_bitPreheatDeactive->LoadFile(Slic3r::resources_dir() + "/icons/device/preheat.png", wxBITMAP_TYPE_PNG);
    m_btnPreheat = new wxBitmapButton(this, wxID_ANY, *m_bitPreheatDeactive, wxDefaultPosition, wxSize(32, 20), wxTE_RIGHT);
    wxBitmap bitPause;
    bitPause.LoadFile(Slic3r::resources_dir() + "/icons/device/pause.png", wxBITMAP_TYPE_PNG);
    m_btnPause = new wxBitmapButton(this, wxID_ANY, bitPause, wxDefaultPosition, wxSize(32, 20), wxTE_RIGHT);
    wxBitmap bitResume;
    bitResume.LoadFile(Slic3r::resources_dir() + "/icons/device/resume.png", wxBITMAP_TYPE_PNG);
    m_btnResume = new wxBitmapButton(this, wxID_ANY, bitResume, wxDefaultPosition, wxSize(32, 20), wxTE_RIGHT);
    wxBitmap bitCancel;
    bitCancel.LoadFile(Slic3r::resources_dir() + "/icons/device/stop.png", wxBITMAP_TYPE_PNG);
    m_btnCancel = new wxBitmapButton(this, wxID_ANY, bitCancel, wxDefaultPosition, wxSize(32, 20), wxTE_RIGHT);
    m_bitExpanded->LoadFile(Slic3r::resources_dir() + "/icons/device/collapse.png", wxBITMAP_TYPE_PNG);
    m_bitCollapsed->LoadFile(Slic3r::resources_dir() + "/icons/device/expand.png", wxBITMAP_TYPE_PNG);
    m_btnExpandCollapse = new wxBitmapButton(this, wxID_ANY, *m_bitCollapsed, wxDefaultPosition, wxSize(32, 20), wxTE_RIGHT);

    // Actions
    m_btnSayHi->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) { this->nm->sayHi(); });
    m_btnPreheat->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) { this->nm->togglePreheat(); });
    m_btnCancel->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) { this->confirm([this] { this->nm->cancel(); }); });
    m_btnPause->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) { this->confirm([this] { this->nm->pause(); }); });
    m_btnResume->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) { this->confirm([this] { this->nm->resume(); }); });
    m_btnUnload->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) { this->confirm([this] { this->nm->unloadFilament(); }); });
    m_timer->Bind(wxEVT_TIMER, [this](wxTimerEvent &evt) { this->onTimer(evt); });

    nm->setUploadProgressCallback([this](int progress) {
        if (progress <= 0 || progress >= 100) this->updateStates();
        this->updateProgress();
    });
    // End of actions
    // action buttons end.

    wxFont normalFont = wxGetApp().normal_font();
    wxFont xsmallFont = wxGetApp().normal_font();
    wxFont boldFont = wxGetApp().bold_font();
    wxFont boldSmallFont = wxGetApp().bold_font();
    wxSizerFlags expandFlag(1);
    expandFlag.Expand();

    // Device model start... (left pane).
    // UI modifications to model text. Upper case then plus => +
    string dM = to_upper_copy(nm->attr->deviceModel);
    boost::replace_all(dM, "PLUS", "+");
#ifdef _WIN32
    boldFont.SetPointSize(14);
    boldSmallFont.SetPointSize(9);
    xsmallFont.SetPointSize(7);
#else
    boldFont.SetPointSize(18);
    boldSmallFont.SetPointSize(12);
    xsmallFont.SetPointSize(10);
#endif
    m_avatar->SetFont(boldFont);
    wxString dMWx(dM);
    m_avatar->SetText(dMWx);
    m_deviceSizer->Add(m_avatar, wxSizerFlags().Border(wxALL, 7));
    if (is_there(this->nm->attr->deviceModel, {"z2", "z3"})) {
        m_avatar->Bind(wxEVT_LEFT_DCLICK, [this](const wxMouseEvent &evt) {
            BOOST_LOG_TRIVIAL(info) << "Clicked on avatar trying to open stream on: " << this->nm->name;
            if (this->nm->attr->firmwareVersion.GetMinor() >= 3 && this->nm->attr->firmwareVersion.GetMicro() >= 80) {
                wxFileName ffplay(wxStandardPaths::Get().GetExecutablePath());
                wxString curExecPath(ffplay.GetPath());
                wxExecute(
#ifdef _WIN32
                    "cmd.exe /c " + curExecPath + "/ffplay tcp://" + this->nm->ip + ":5002 -window_title \"Zaxe " + to_upper_copy(this->nm->attr->deviceModel) + ": " + this->nm->name + "\" -x 720",
                    wxEXEC_ASYNC | wxEXEC_HIDE_CONSOLE
#else
                    curExecPath + "/ffplay tcp://" + this->nm->ip + ":5002 -window_title \"Zaxe " + to_upper_copy(this->nm->attr->deviceModel) + ": " + this->nm->name + "\" -x 720",
                    wxEXEC_ASYNC
#endif
                );
            } else {
                wxMessageBox("Need device firmware version at least v3.3.80 to comply.", "Need firmware update for this feautre.", wxICON_INFORMATION);
            }
        });
    }
    // End of Device model.

    // Right pane (2nd column start)
    m_deviceSizer->Add(m_rightSizer, 1, wxEXPAND);

    // Device name and action buttons start...
    wxBoxSizer* dnaabp = new wxBoxSizer(wxHORIZONTAL); // device name and action buttons
    m_txtDeviceName->SetFont(boldSmallFont);
    wxGetApp().UpdateDarkUI(m_txtDeviceName);
    auto* actionBtnsSizer = new wxBoxSizer(wxHORIZONTAL); // action buttons ie: hi.
    actionBtnsSizer->Add(m_btnPreheat);
    actionBtnsSizer->Add(m_btnSayHi);
    actionBtnsSizer->Add(m_btnPause);
    actionBtnsSizer->Add(m_btnResume);
    actionBtnsSizer->Add(m_btnCancel);
    actionBtnsSizer->Add(m_btnExpandCollapse);
    dnaabp->Add(m_txtDeviceName, expandFlag.Left());
    dnaabp->Add(actionBtnsSizer, wxSizerFlags().Right());
    m_rightSizer->AddSpacer(10);
    m_rightSizer->Add(dnaabp, 0, wxEXPAND); // only grow horizontally.
    // End of device name.

    // status start...
    wxBoxSizer* sapp = new wxBoxSizer(wxHORIZONTAL); // status and progress line.
    m_txtStatus->SetForegroundColour("GREY");
    m_txtStatus->SetFont(boldSmallFont);
    m_txtProgress->SetForegroundColour("GREY");
    m_txtProgress->SetFont(boldSmallFont);

    sapp->Add(m_txtStatus, expandFlag.Left());
    sapp->Add(m_txtProgress, wxSizerFlags().Right());
    m_rightSizer->AddSpacer(8);
    m_rightSizer->Add(sapp, 0, wxEXPAND); // expand horizontally in vertical box.
    // End of status

    // Progress start...
    m_progressBar->SetValue(0);
    m_rightSizer->Add(m_progressBar, 0, wxEXPAND); // expand horizontally in vertical box.
    // Progress end...

    // Bed occupied message
    m_txtBedOccupiedMessage->SetForegroundColour(DEVICE_COLOR_SUCCESS);
    m_txtBedOccupiedMessage->SetFont(xsmallFont);
    m_rightSizer->Add(m_txtBedOccupiedMessage, 0, wxTOP, -1);
    // Bed occupied message end...
    // Error message
    m_txtErrorMessage->SetForegroundColour(DEVICE_COLOR_DANGER);
    m_txtErrorMessage->SetFont(xsmallFont);
    m_rightSizer->Add(m_txtErrorMessage, 0, wxTOP, -1);
    m_txtErrorMessage->Hide();
    // Error message end...
    // Unload button
    m_btnUnload->SetFont(xsmallFont);
    // Unload button end...
    // FW version
    m_txtFWVersion->SetFont(xsmallFont);
    m_txtFWVersion->SetForegroundColour("GREY");
    // FW version end...
    // Print now button.
    m_rightSizer->Add(m_btnPrintNow, 0, wxTOP, -12);
    m_btnPrintNow->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) {
        this->m_btnPrintNow->Enable(false);
        BOOST_LOG_TRIVIAL(info) << "Print now pressed on " << this->nm->name;
        const ZaxeArchive& archive = wxGetApp().plater()->get_zaxe_archive();

        string dM = to_upper_copy(this->nm->attr->deviceModel);
        boost::replace_all(dM, "PLUS", "+");
        if (GUI::wxGetApp().preset_bundle->printers.get_selected_preset().name.find(dM) == std::string::npos) {
            wxMessageBox(L("Device model does NOT match. Please reslice with the correct model."), _L("Wrong device model"), wxICON_ERROR);
        } else if (!this->nm->attr->isLite && this->nm->attr->material.compare(archive.get_info("material")) != 0) {
            wxMessageBox(L("Materials don't match with this device. Please reslice with the correct material."), _L("Wrong material type"), wxICON_ERROR);
        } else if (!this->nm->attr->isLite && this->nm->attr->nozzle.compare(archive.get_info("nozzle_diameter")) != 0) {
            wxMessageBox(L("Currently installed nozzle on device doesn't match with this slice. Please reslice with the correct nozzle."), _L("Wrong nozzle type"), wxICON_ERROR);
        } else {
            std::thread t([&]() {
                if (this->nm->attr->isLite) {
                    this->nm->upload(wxGetApp().plater()->get_gcode_path().c_str(),
                                     translate_chars(wxGetApp().plater()->get_filename().ToStdString()).c_str());
                } else this->nm->upload(wxGetApp().plater()->get_zaxe_code_path().c_str());
            });
            t.detach(); // crusial. otherwise blocks main thread.
        }
        this->m_btnPrintNow->Enable(true);
    });
    // Print now button end.
    m_btnExpandCollapse->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) {
        if (m_isExpanded) {
            SetMinSize(wxSize(GetParent()->GetSize().GetWidth(), DEVICE_HEIGHT));
            m_expansionSizer->ShowItems(false);
        } else {
            SetMinSize(wxSize(GetParent()->GetSize().GetWidth(), DEVICE_HEIGHT +  (this->nm->states->printing ? 110 : 70)));
            m_expansionSizer->ShowItems(true);
            if (!this->nm->states->printing) {
                // hide m_txtFileName and duration and their bottom borders.
                for (int i = 0; i < 2; i++)
                    m_expansionSizer->Hide((size_t)i);
                !is_there(this->nm->attr->deviceModel, {"z"}) || this->nm->isBusy() || this->nm->states->bedOccupied || this->nm->states->hasError ? m_btnUnload->Hide() : m_btnUnload->Show();
            } else {
                m_btnUnload->Hide();
            }
        }
        m_expansionSizer->Layout();
        GetParent()->Layout();
        GetParent()->FitInside();
        m_isExpanded = !m_isExpanded;
        m_btnExpandCollapse->SetBitmapLabel(*(m_isExpanded
                                              ? m_bitExpanded
                                              : m_bitCollapsed));
    });

    // inside the expansion panel.
    m_expansionSizer->Add(m_txtFileName, 0, wxBOTTOM);
    m_expansionSizer->Add(m_txtFileTime, 0, wxBOTTOM);
    m_filamentSizer->Add(m_txtDeviceMaterial, 0, wxLEFT);
    m_filamentSizer->Add(m_btnUnload, 0, wxLEFT, 10);
    m_expansionSizer->Add(m_filamentSizer, 0, wxTOP);
    m_expansionSizer->Add(m_txtDeviceNozzleDiameter, 0, wxBOTTOM);
    m_expansionSizer->Add(m_txtDeviceIP, 0, wxBOTTOM);
    m_expansionSizer->Add(m_txtFWVersion, 0, wxEXPAND | wxRIGHT, 25);
    m_expansionSizer->ShowItems(false);
    m_mainSizer->Add(m_expansionSizer, 0, wxEXPAND | wxLEFT, m_avatar->GetSize().GetWidth() + 14); // only expand horizontally in vertical sizer.
    // end of expansion panel contents.

    // Bottom line. (separator)
    m_mainSizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxHORIZONTAL), 0, wxRIGHT | wxEXPAND, 20);

    updateStates();

    m_mainSizer->Layout();
}

void Device::avatarReady()
{
    if (this->nm == nullptr) return;
    m_avatar->SetAvatar(this->nm->getAvatar());
}

void Device::updateStatus()
{
    wxString statusTxt = "";
    if (nm->states->bedOccupied) {
        statusTxt = _L("Bed is occupied...");
    } else if (nm->states->heating) {
        statusTxt = _L("Heating...");
        m_progressBar->SetColour(DEVICE_COLOR_DANGER);
    } else if (nm->states->printing) {
        statusTxt = _L("Printing...");
        m_progressBar->SetColour(DEVICE_COLOR_ZAXE_BLUE);
        if (!m_timer->IsRunning())
            m_timer->Start(1000);
    } else if (nm->states->uploading) {
        statusTxt = _L("Uploading...");
        m_progressBar->SetColour(DEVICE_COLOR_UPLOADING);
    } else if (nm->states->paused) {
        statusTxt = _L("Paused...");
        m_progressBar->SetColour(DEVICE_COLOR_ZAXE_BLUE);
    } else if (nm->states->calibrating) {
        statusTxt = _L("Calibrating...");
        m_progressBar->SetColour(DEVICE_COLOR_ORANGE);
    }
    m_txtStatus->SetLabel(statusTxt);

    if (nm->attr->hasSnapshot) { // if has snapshot and need to show the avatar start downloading...
        if(nm->states->heating || nm->states->printing || nm->states->calibrating || nm->states->bedOccupied) {
            nm->downloadAvatar(); // this fires EVT_MACHINE_AVATAR_READY when it's ready.
        } else m_avatar->Clear(); // clear the last image.
    }

    if (!nm->states->printing && m_timer->IsRunning()) // stop when done.
        m_timer->Stop();

    m_btnPreheat->SetBitmapLabel(*(nm->states->preheat
                                    ? m_bitPreheatActive
                                    : m_bitPreheatDeactive));
}

void Device::updateStates()
{

    if (nm->isBusy()) {
        m_progressBar->Show();
        m_txtProgress->Show();
        m_btnSayHi->Hide();
        m_btnPreheat->Hide();
        m_btnPrintNow->Hide();
        m_btnResume->Hide();
        m_btnPause->Hide();
        m_txtBedOccupiedMessage->Hide();
        m_btnUnload->Hide();

        if (nm->states->printing) {
            if (nm->states->paused) {
                m_btnResume->Show();
                m_btnPause->Hide();
            } else if (!nm->states->heating) {
                m_btnResume->Hide();
                m_btnPause->Show();
            }
            m_btnCancel->Show();
        }
        if (!nm->states->uploading) {
            m_btnCancel->Show();
        }
        updateProgress();
        if (nm->states->hasError) {
            m_progressBar->Hide();
            m_txtProgress->Hide();
            m_txtErrorMessage->Show();
        }
    } else {
        m_btnPause->Hide();
        m_btnResume->Hide();
        m_btnCancel->Hide();
        m_progressBar->Hide();
        m_txtProgress->Hide();
        if (nm->states->bedOccupied || nm->states->hasError) {
            if (nm->states->hasError) {
                m_txtErrorMessage->Show();
                m_txtBedOccupiedMessage->Hide();
            } else if (nm->states->bedOccupied)
                m_txtBedOccupiedMessage->Show();
            m_btnPrintNow->Hide();
            m_btnSayHi->Hide();
            m_btnPreheat->Hide();
            m_btnUnload->Hide();
        } else {
            m_btnSayHi->Show();
            m_btnPreheat->Show();
            m_btnPrintNow->Show();
            m_txtBedOccupiedMessage->Hide();
            if (is_there(this->nm->attr->deviceModel, {"z"}) && m_isExpanded)
                m_btnUnload->Show();
        }
    }

    if (nm->states->printing) {
        if (m_isExpanded) {
            // show m_txtFileName and duration
            for (int i = 0; i < 2; i++)
                m_expansionSizer->Show((size_t)i);
        }
    } else if (m_isExpanded) {
        // hide m_txtFileName and duration
        for (int i = 0; i < 2; i++)
            m_expansionSizer->Hide((size_t)i);
    }

    if (m_isExpanded) {
        SetMinSize(wxSize(GetParent()->GetSize().GetWidth(), DEVICE_HEIGHT +  (this->nm->states->printing ? 110 : 70)));
        m_expansionSizer->Layout();
        GetParent()->Layout();
        GetParent()->FitInside();
    }

    m_rightSizer->Layout();
    updateStatus();
}

void Device::updateProgress()
{
    m_progressBar->SetValue(nm->progress);
    m_txtProgress->SetLabel("%" + std::to_string(nm->progress));
    m_mainSizer->Layout();
    GetParent()->Refresh(); // crusial otherwise doesn't show upload progress...
}

void Device::setName(const string &name)
{
    m_txtDeviceName->SetLabel(name);
}

void Device::setMaterial(const string &material)
{
    m_txtDeviceMaterial->SetLabel(_L("Material: ") + NetworkMachineManager::MaterialName(nm->attr->material));
}

void Device::setPin(const bool hasPin)
{
    nm->attr->hasPin = hasPin; // needed?
}

void Device::setNozzle(const string &nozzle)
{
    m_txtDeviceNozzleDiameter->SetLabel(_L("Nozzle: ") + nm->attr->nozzle + "mm");
}

void Device::setFileStart()
{
    m_txtFileName->SetLabel(nm->attr->printingFile.substr(0, DEVICE_FILENAME_MAX_NUM_CHARS));
    m_pausedSeconds = 0; // reset
}

Device::~Device()
{
    if (this->GetParent() == NULL) return;
    if (m_timer->IsRunning()) m_timer->Stop();
    m_rightSizer->Clear(true); // Also destroys children.
    m_deviceSizer->Clear(true);
    m_mainSizer->Clear(true);
    GetParent()->Layout();
    GetParent()->FitInside();
}

void Device::enablePrintNowButton(bool enable)
{
    m_btnPrintNow->Enable(enable);
}

void Device::onTimer(wxTimerEvent& event)
{
    m_txtFileTime->SetLabel(_L("Elapsed / Estimated time: ") +
        get_time_hms(
            wxDateTime::Now().GetTicks() -
            this->nm->attr->startTime -
            // make it look paused by incrementing and subtracting from the counter.
            (nm->states->paused ? m_pausedSeconds++ : m_pausedSeconds))
        + " / " + nm->attr->estimatedTime);
}

void Device::confirm(function<void()> cb)
{
    RichMessageDialog dialog(GetParent(), wxString(_L("Are you sure?")), _L("XDesktop: Confirmation"), wxYES_NO);
    dialog.SetYesNoLabels(_L("Yes"), _L("No"));
    int res = dialog.ShowModal();
    if (res == wxID_YES) cb();
}

// Custom controls start.
CustomProgressBar::CustomProgressBar(wxPanel *parent, int id, wxSize size) :
    wxPanel(parent, id, wxDefaultPosition, size),
    m_fgColour(*wxRED),
    m_progress(0)
{
    Connect(wxEVT_PAINT, wxPaintEventHandler(CustomProgressBar::OnPaint));
}

void CustomProgressBar::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxGraphicsContext *gc = wxGraphicsContext::Create(dc);
    if (!gc) return;

    wxRect rect = GetClientRect();
    wxBrush bBrush(wxColour(210, 208, 207));
    gc->SetBrush(bBrush);
    gc->DrawRoundedRectangle(rect.GetLeft() + 2 /*margin*/, rect.GetTop(), rect.GetRight() - 2, rect.GetHeight(), 2);
    if (m_progress > 0) {
        wxBrush fBrush(m_fgColour);
        gc->SetBrush(fBrush);
        gc->DrawRoundedRectangle(rect.GetLeft() + 2 /*margin*/, rect.GetTop(), (rect.GetRight() * m_progress / 100) - 2, rect.GetHeight(), 2);
    }
    delete gc;
}

RoundedPanel::RoundedPanel(wxPanel *parent, int id, wxString label, wxSize size, wxColour bgColour, wxColour fgColour) :
    wxPanel(parent, id, wxDefaultPosition, size),
    m_bgColour(bgColour),
    m_fgColour(fgColour),
    m_label(label)
{
    Connect(wxEVT_PAINT, wxPaintEventHandler(RoundedPanel::OnPaint));
}

void RoundedPanel::SetText(wxString& txt)
{
    m_label = txt;
}

void RoundedPanel::Clear()
{
    m_avatar = wxNullBitmap;
    Refresh();
}

void RoundedPanel::SetAvatar(wxBitmap& bmp)
{
    m_avatar = bmp;
    Refresh();
}

wxRect RoundedPanel::ShrinkToSize(const wxRect &container, const int margin, const int thickness)
{
    wxRect retRect;
    int TLOffset = margin + thickness / 2; // top left corner
    int BROffset = thickness % 2 + TLOffset * 2; // bottom right corner
    retRect.x = container.x + TLOffset;
    retRect.y = container.y + TLOffset;
    retRect.width = container.width - BROffset;
    retRect.height = container.height - BROffset;
    return retRect;
}

void RoundedPanel::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxGraphicsContext *gc = wxGraphicsContext::Create(dc);
    if (!gc) return;
    wxRect rect = ShrinkToSize(GetClientRect(), 2, 2);
    wxPen bPen(m_bgColour);
    wxBrush bBrush(m_bgColour);
    bPen.SetWidth(5);
    gc->SetPen(bPen);
    gc->SetBrush(bBrush);
    gc->DrawRoundedRectangle(rect.GetLeft(), rect.GetTop(), rect.GetWidth(), rect.GetHeight(), 10);
    if (m_avatar.IsOk()) {
        gc->DrawBitmap(m_avatar, 5, 5, rect.GetWidth() - 5, rect.GetHeight() - 5);
    } else {
        gc->SetFont(GetFont(), m_fgColour);
        gc->DrawText(m_label, (rect.GetWidth() / 2) - (m_label.size() * 4), (rect.GetHeight() / 2) - 8);
    }

    delete gc;
}
} // namespace GUI
} // namespace Slic3r
