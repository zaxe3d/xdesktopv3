#include "Device.hpp"

#include "I18N.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp" // for export_zaxe_code
#include "MsgDialog.hpp" // for RichMessageDialog

namespace Slic3r {
namespace GUI {

Device::Device(NetworkMachine* nm, wxWindow* parent) :
    wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(parent->GetSize().GetWidth(), DEVICE_HEIGHT)),
    nm(nm),
    m_mainSizer(new wxBoxSizer(wxVERTICAL)), // vertical sizer (device sizer - horizontal line (seperator).
    m_deviceSizer(new wxBoxSizer(wxHORIZONTAL)), // horizontal sizer (avatar | right pane).)
    m_rightSizer(new wxBoxSizer(wxVERTICAL)), // vertical right pane. (name - status - progress bar).
    m_progressBar(new CustomProgressBar(this, wxID_ANY, wxSize(-1, 5))),
    m_txtStatus(new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 18), wxTE_LEFT)),
    m_txtProgress(new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 18), wxTE_RIGHT)),
    m_btnPrintNow(new wxButton(this, wxID_ANY, _L("Print Now!"))),
    m_avatar(new RoundedPanel(this, wxID_ANY, "", wxSize(60, 60), wxColour(169, 169, 169), wxColour("WHITE"))),
    m_bitPreheatActive(new wxBitmap()),
    m_bitPreheatDeactive(new wxBitmap())
{
    SetSizer(m_mainSizer);
    m_mainSizer->Add(m_deviceSizer, 0, wxEXPAND | wxRIGHT, 25); // only expand horizontally in vertical sizer.

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
    wxBitmap bitCancel;
    bitCancel.LoadFile(Slic3r::resources_dir() + "/icons/device/stop.png", wxBITMAP_TYPE_PNG);
    m_btnCancel = new wxBitmapButton(this, wxID_ANY, bitCancel, wxDefaultPosition, wxSize(32, 20), wxTE_RIGHT);
    wxBitmap bitExpCol;
    bitExpCol.LoadFile(Slic3r::resources_dir() + "/icons/device/expand.png", wxBITMAP_TYPE_PNG);
    m_btnExpandCollapse = new wxBitmapButton(this, wxID_ANY, bitExpCol, wxDefaultPosition, wxSize(32, 20), wxTE_RIGHT);

    // Actions
    m_btnSayHi->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) { this->nm->sayHi(); });
    m_btnPreheat->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) { this->nm->togglePreheat(); });
    m_btnCancel->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) { this->confirm([this] { this->nm->cancel(); }); });
    m_btnPause->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) { this->confirm([this] { this->nm->pause(); }); });

    nm->setUploadProgressCallback([this](int progress) {
        if (progress <= 0 || progress >= 100) this->updateStates();
        this->updateProgress();
    });
    // End of actions
    // action buttons end.

    wxFont normalFont = wxGetApp().normal_font();
    wxFont boldFont = wxGetApp().bold_font();
    wxSizerFlags expandFlag(1);
    expandFlag.Expand();

    // Device model start... (left pane).
    // UI modifications to model text. Upper case then plus => +
    string dM = to_upper_copy(nm->attr->deviceModel);
    boost::replace_all(dM, "PLUS", "+");
    boldFont.SetPointSize(18);
    m_avatar->SetFont(boldFont);
    wxString dMWx(dM);
    m_avatar->SetText(dMWx);
    m_deviceSizer->Add(m_avatar, wxSizerFlags().Border(wxALL, 7));
    // End of Device model.

    // Right pane (2nd column start)
    m_deviceSizer->Add(m_rightSizer, 1, wxEXPAND);

    // Device name and action buttons start...
    wxBoxSizer* dnaabp = new wxBoxSizer(wxHORIZONTAL); // device name and action buttons
    boldFont.SetPointSize(11);
    auto* txtDn = new wxStaticText(this, wxID_ANY, nm->name,
                                   wxDefaultPosition, wxSize(-1, 20),
                                   wxTE_LEFT);
    txtDn->SetFont(boldFont);

    auto* actionBtnsSizer = new wxBoxSizer(wxHORIZONTAL); // action buttons ie: hi.
    actionBtnsSizer->Add(m_btnPreheat);
    actionBtnsSizer->Add(m_btnSayHi);
    actionBtnsSizer->Add(m_btnPause);
    actionBtnsSizer->Add(m_btnCancel);
    actionBtnsSizer->Add(m_btnExpandCollapse);
    dnaabp->Add(txtDn, expandFlag.Left());
    dnaabp->Add(actionBtnsSizer, wxSizerFlags().Right());
    m_rightSizer->AddSpacer(10);
    m_rightSizer->Add(dnaabp, 0, wxEXPAND); // only grow horizontally.
    // End of device name.

    // status start...
    wxBoxSizer* sapp = new wxBoxSizer(wxHORIZONTAL); // status and progress line.
    m_txtStatus->SetForegroundColour("GREY");
    m_txtStatus->SetFont(boldFont);
    m_txtProgress->SetForegroundColour("GREY");
    m_txtProgress->SetFont(boldFont);

    sapp->Add(m_txtStatus, expandFlag.Left());
    sapp->Add(m_txtProgress, wxSizerFlags().Right());
    m_rightSizer->AddSpacer(8);
    m_rightSizer->Add(sapp, 0, wxEXPAND); // expand horizontally in vertical box.
    // End of status

    // Progress start...
    m_progressBar->SetValue(0);
    m_rightSizer->Add(m_progressBar, 0, wxEXPAND); // expand horizontally in vertical box.
    // Progress end...

    // Print now button.
    m_rightSizer->Add(m_btnPrintNow, 0, wxTOP, -12);
    m_btnPrintNow->Bind(wxEVT_BUTTON, [this](const wxCommandEvent &evt) {
        BOOST_LOG_TRIVIAL(info) << "Print now pressed on " << this->nm->name;
        const ZaxeArchive& archive = wxGetApp().plater()->get_zaxe_archive();

        if (this->nm->attr->material.compare(archive.get_info("material")) != 0) {
            wxMessageBox(L("Materials don't match with this device. Please reslice with the correct material."), _L("Wrong material type"), wxICON_ERROR);
        } else if (this->nm->attr->nozzle.compare(archive.get_info("nozzle_diameter")) != 0) {
            wxMessageBox(L("Currently installed nozzle on device doesn't match with this slice. Please reslice with the correct nozzle."), _L("Wrong nozzle type"), wxICON_ERROR);
        } else this->nm->upload(wxGetApp().plater()->get_zaxe_code_path().c_str());
    });
    // Print now button end.

    // Bottom line. (separator)
    m_mainSizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 4), wxHORIZONTAL), expandFlag.Border(wxRIGHT, 20));

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
    if (nm->states->uploading) {
        statusTxt = _L("Uploading...");
        m_progressBar->SetColour(*wxGREEN);
    } else if (nm->states->paused) {
        statusTxt = _L("Paused...");
        m_progressBar->SetColour(wxColour(0, 155, 223));
    } else if (nm->states->printing) {
        statusTxt = _L("Printing...");
        m_progressBar->SetColour(wxColor(0, 155, 223));
    } else if (nm->states->heating) {
        statusTxt = _L("Heating...");
        m_progressBar->SetColour(wxColor(255, 153, 102));
    } else if (nm->states->calibrating) {
        statusTxt = _L("Calibrating...");
        m_progressBar->SetColour(wxColor(255, 165, 0));
    }
    m_txtStatus->SetLabel(statusTxt);

    if (nm->attr->hasSnapshot) { // if has snapshot and need to show the avatar start downloading...
        if(nm->states->heating || nm->states->printing || nm->states->calibrating) {
            nm->downloadAvatar(); // this fires EVT_MACHINE_AVATAR_READY when it's ready.
        } else m_avatar->Clear(); // clear the last image.
    }

    m_btnPreheat->SetBitmapLabel(*(nm->states->preheat
                                    ? m_bitPreheatActive
                                    : m_bitPreheatDeactive));
}

void Device::updateStates()
{
    if (nm->states->printing ||
        nm->states->heating ||
        nm->states->calibrating ||
        nm->states->paused ||
        nm->states->uploading) {
        m_progressBar->Show();
        m_txtProgress->Show();
        m_btnSayHi->Hide();
        m_btnPreheat->Hide();
        m_btnPrintNow->Hide();
        if (!nm->states->uploading)
            m_btnCancel->Show();
        updateProgress();
    } else {
        m_btnPause->Hide();
        m_btnCancel->Hide();
        m_progressBar->Hide();
        m_txtProgress->Hide();
        m_btnSayHi->Show();
        m_btnPreheat->Show();
        m_btnPrintNow->Show();
    }

    if (nm->states->printing) {
        if (nm->states->paused)
            m_btnPause->Hide();
        else m_btnPause->Show();
    }

    m_rightSizer->Layout();
    updateStatus();
}

void Device::updateProgress()
{
    m_progressBar->SetValue(nm->progress);
    m_txtProgress->SetLabel("%" + std::to_string(nm->progress));
    m_mainSizer->Layout();
}

Device::~Device()
{
    if (this->GetParent() == NULL) return;
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
        gc->DrawBitmap(m_avatar, 2, 2, rect.GetWidth(), rect.GetHeight());
    } else {
        gc->SetFont(GetFont(), m_fgColour);
        gc->DrawText(m_label, (rect.GetWidth() / 2) - (m_label.size() * 4), (rect.GetHeight() / 2) - 8);
    }

    delete gc;
}
} // namespace GUI
} // namespace Slic3r
