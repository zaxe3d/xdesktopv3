#include "PressureAdvanceCalibrationDialog.hpp"

#include "GUI_App.hpp"
#include "Plater.hpp"
#include "MainFrame.hpp"

#include <wx/valnum.h>
#include <wx/artprov.h>
#include <wx/hyperlink.h>

namespace Slic3r::GUI {

static const double MIN_PA_K_VALUE      = 0.f;
static const double MAX_PA_K_VALUE      = 0.5f;
static const double MIN_PA_K_VALUE_STEP = 0.001f;

PressureAdvanceCalibrationDialog::PressureAdvanceCalibrationDialog(
    wxWindow *parent, wxWindowID id, const wxString &title)
    : wxDialog(parent,
               id,
               wxString::Format(_L("Pressure Advance Calibration - %s"),
                                title),
               wxDefaultPosition,
               wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    sizer = new wxBoxSizer(wxVERTICAL);

    createInputSection();
    createInfoSection();
    createActionSection();

    SetSizerAndFit(sizer);

    checkInputs();
    checkState();
}

void PressureAdvanceCalibrationDialog::createInputSection()
{
    auto fromKValueText = new wxStaticText(this, wxID_ANY, _L("From K Value"),
                                           wxDefaultPosition, wxSize(-1, 18),
                                           wxTE_LEFT);
    auto toKValueText   = new wxStaticText(this, wxID_ANY, _L("To K Value"),
                                           wxDefaultPosition, wxSize(-1, 18),
                                           wxTE_LEFT);
    auto stepValueText  = new wxStaticText(this, wxID_ANY, _L("Step Value"),
                                           wxDefaultPosition, wxSize(-1, 18),
                                           wxTE_LEFT);

    int validatorPrecision{3};
    int validatorStyle{wxNUM_VAL_NO_TRAILING_ZEROES};
    wxFloatingPointValidator<float> validator(validatorPrecision, nullptr,
                                              validatorStyle);

    fromKValueTextCtrl = new wxTextCtrl(this, wxID_ANY,
                                        wxString::Format(wxT("%.0f"), 0),
                                        wxDefaultPosition, wxSize(-1, 20),
                                        wxTE_LEFT | wxBORDER, validator);
    toKValueTextCtrl   = new wxTextCtrl(this, wxID_ANY,
                                        wxString::Format(wxT("%.2f"), 0.05),
                                        wxDefaultPosition, wxSize(-1, 20),
                                        wxTE_LEFT | wxBORDER, validator);
    stepValueTextCtrl  = new wxTextCtrl(this, wxID_ANY,
                                        wxString::Format(wxT("%.3f"), 0.005),
                                        wxDefaultPosition, wxSize(-1, 20),
                                        wxTE_LEFT | wxBORDER, validator);

    auto createRuleIndicator =
        [&](wxStaticBitmap **icon, wxStaticText **staticText,
            const wxString &text, const wxArtID &artId = wxART_CROSS_MARK) {
            *icon               = new wxStaticBitmap(this, wxID_ANY,
                                                     wxArtProvider::GetBitmap(artId));
            *staticText         = new wxStaticText(this, wxID_ANY, text,
                                                   wxDefaultPosition, wxDefaultSize);
            auto indicatorSizer = new wxBoxSizer(wxHORIZONTAL);
            indicatorSizer->Add(*icon, 0, wxALIGN_CENTER | wxALL, 1);
            indicatorSizer->Add(*staticText, 0, wxALIGN_CENTER | wxALL, 1);
            return indicatorSizer;
        };

    auto fromValueRuleSizer =
        createRuleIndicator(&fromValueRuleIcon, &fromValueRuleText,
                            wxString::Format(_L("Start value >= %.3f"),
                                             MIN_PA_K_VALUE));

    auto toValueRuleSizer =
        createRuleIndicator(&toValueRuleIcon, &toValueRuleText,
                            wxString::Format(_L("End value <= %.3f"),
                                             MAX_PA_K_VALUE));

    auto stepValueRuleSizer = createRuleIndicator(&stepValueRuleIcon,
                                                  &stepValueRuleText,
                                                  _L("Start + Step <= End"));

    auto deviceAvailableRuleSizer =
        createRuleIndicator(&deviceAvailableRuleIcon,
                            &deviceAvailableRuleText,
                            _L("Printer must be available"));

    auto stepNullValueRuleSizer =
        createRuleIndicator(&stepNullValueRuleIcon, &stepNullValueRuleText,
                            wxString::Format(_L("Step >= %.3f"),
                                             MIN_PA_K_VALUE_STEP));

    auto numberOfLinesSizer = createRuleIndicator(&calibSummaryIcon,
                                                  &calibSummaryText, "",
                                                  wxART_INFORMATION);

    auto kValueSizer = new wxGridSizer(4, 3, 5, 5);
    int  border{2};
    kValueSizer->Add(fromKValueText, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(toKValueText, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(stepValueText, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(fromKValueTextCtrl, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(toKValueTextCtrl, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(stepValueTextCtrl, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(fromValueRuleSizer, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(toValueRuleSizer, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(stepValueRuleSizer, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(deviceAvailableRuleSizer, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(stepNullValueRuleSizer, 0, wxEXPAND | wxALL, border);
    kValueSizer->Add(numberOfLinesSizer, 0, wxEXPAND | wxALL, border);

    sizer->Add(kValueSizer, 0, wxEXPAND | wxALL, 15);

    fromKValueTextCtrl->Bind(wxEVT_TEXT, [&](auto &evt) {
        checkInputs();
        evt.Skip();
    });

    toKValueTextCtrl->Bind(wxEVT_TEXT, [&](auto &evt) {
        checkInputs();
        evt.Skip();
    });

    stepValueTextCtrl->Bind(wxEVT_TEXT, [&](auto &evt) {
        checkInputs();
        evt.Skip();
    });
}

void PressureAdvanceCalibrationDialog::createInfoSection()
{
    auto createWrappedText = [&](const wxString &text) {
        auto _bg    = wxColour(255, 246, 233);
        auto _panel = new wxPanel(this);
        _panel->SetBackgroundColour(_bg);

        auto _widget = new wxStaticText(_panel, wxID_ANY, text,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxALIGN_CENTER | wxTE_MULTILINE);

        auto _sizer = new wxBoxSizer(wxVERTICAL);
        _sizer->Add(_widget, 0, wxEXPAND | wxALIGN_CENTER | wxALL, 10);

        _panel->SetSizerAndFit(_sizer);
        return _panel;
    };

    auto wrappedInputInfoText = createWrappedText(
        _L("After calibration print is done, determine the best matching "
           "K-Factor.\n"
           "Click 'Go to Settings' button and modify 'Filament Settings -> "
           "Pressure Advance' field with\n"
           "K-Factor and do not forget to enable pressure advance."));

    sizer->Add(wrappedInputInfoText, 0, wxEXPAND | wxALL, 10);
}

void PressureAdvanceCalibrationDialog::createActionSection()
{
    spinner     = new wxActivityIndicator(this, wxID_ANY, wxDefaultPosition,
                                          wxSize(-1, 18), wxALIGN_CENTER);
    spinnerText = new wxStaticText(this, wxID_ANY, _L("Calibration started"),
                                   wxDefaultPosition, wxDefaultSize,
                                   wxALIGN_CENTER);

    spinner->Start();
    spinner->Hide();
    spinnerText->Hide();

    redirectToFilamentSettingsButton = new wxButton(this, wxID_ANY,
                                                    _L("Go to Settings"),
                                                    wxDefaultPosition,
                                                    wxDefaultSize,
                                                    wxCENTER | wxCentreY);

    startCalibrationButton = new wxButton(this, wxID_ANY,
                                          _L("Start Calibration"),
                                          wxDefaultPosition, wxDefaultSize,
                                          wxCENTER | wxCentreY);

    auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(spinner, 0, wxALIGN_LEFT | wxALL, 5);
    buttonSizer->Add(spinnerText, 0, wxALIGN_LEFT | wxALL, 5);
    buttonSizer->AddStretchSpacer();
    buttonSizer->Add(redirectToFilamentSettingsButton, 0,
                     wxALIGN_RIGHT | wxALL, 5);
    buttonSizer->Add(startCalibrationButton, 0, wxALIGN_RIGHT | wxALL, 5);

    sizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 15);

    redirectToFilamentSettingsButton->Bind(wxEVT_BUTTON, [&](auto &evt) {
        evt.Skip();
        wxGetApp().mainframe->select_tab(
            wxGetApp().get_tab(Preset::TYPE_FILAMENT));
        EndModal(wxID_CLOSE);
    });

    startCalibrationButton->Bind(wxEVT_BUTTON, [&](auto &evt) {
        evt.Skip();
        startCalibrationButton->Enable(false);
        Layout();
        Refresh();

        Calib_Params param;
        param.print_numbers = true;
        fromKValueTextCtrl->GetValue().ToDouble(&param.start);
        toKValueTextCtrl->GetValue().ToDouble(&param.end);
        stepValueTextCtrl->GetValue().ToDouble(&param.step);
        param.mode = CalibMode::Calib_PA_Line;

        auto plater = wxGetApp().plater();
        plater->calib_pa(param);
        plater->reslice();
        // plater->select_view_3D("Preview");

        std::thread t([&]() {
            auto step_ms{200};
            auto duration_ms{10'000};
            auto step    = duration_ms / step_ms;
            auto _plater = wxGetApp().plater();
            bool ready{false};
            for (size_t i = 0; i < step; ++i) {
                if (_plater->is_ready_for_printing()) {
                    ready = true;
                    break;
                };
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            _plater->fff_print().set_calib_params(Calib_Params{});

            if (!ready) {
                startCalibrationButton->Enable(true);
                wxMessageBox(_L("Calibration is cancelled due to timeout!"),
                             _L("Slicing timeout"), wxICON_ERROR);
                return;
            }

            auto device = dynamic_cast<Device *>(GetParent());
            if (device) {
                if (device->print()) {
                    isCalibStarted = true;
                } else {
                    startCalibrationButton->Enable(true);
                }
            } else {
                startCalibrationButton->Enable(true);
            }
        });
        t.detach();
    });
}

bool PressureAdvanceCalibrationDialog::checkRule(
    wxStaticBitmap           *icon,
    wxStaticText             *text,
    std::function<bool(void)> ruleFunc)
{
    auto icon_name = wxART_CROSS_MARK;
    auto color     = wxColor(187, 33, 36);
    bool result    = false;
    if (ruleFunc()) {
        icon_name = wxART_TICK_MARK;
        color     = wxColor(34, 187, 51);
        result    = true;
    }

    icon->SetBitmap(wxArtProvider::GetBitmap(icon_name));
    text->SetForegroundColour(color);
    return result;
}

void PressureAdvanceCalibrationDialog::checkInputs()
{
    auto convertEmptyToZero = [](const auto &text) {
        return text.empty() ? "0.0" : text;
    };

    auto from_str = convertEmptyToZero(fromKValueTextCtrl->GetValue());
    auto to_str   = convertEmptyToZero(toKValueTextCtrl->GetValue());
    auto step_str = convertEmptyToZero(stepValueTextCtrl->GetValue());

    double from, to, step;
    from_str.ToDouble(&from);
    to_str.ToDouble(&to);
    step_str.ToDouble(&step);

    bool ready = checkRule(fromValueRuleIcon, fromValueRuleText,
                           [&]() { return from >= MIN_PA_K_VALUE; });
    ready      = checkRule(toValueRuleIcon, toValueRuleText,
                           [&]() { return to <= MAX_PA_K_VALUE; }) &&
            ready;
    ready = checkRule(stepValueRuleIcon, stepValueRuleText,
                      [&]() { return to >= from + step; }) &&
            ready;

    ready = checkRule(stepNullValueRuleIcon, stepNullValueRuleText,
                      [&]() { return step >= MIN_PA_K_VALUE_STEP * 0.9; }) &&
            ready;

    startCalibrationButton->Enable(ready);

    refreshCalibSummary(from, to, step);

    Layout();
    Refresh();
    FitInside();
}

void PressureAdvanceCalibrationDialog::checkState()
{
    auto device = dynamic_cast<Device *>(GetParent());
    if (!device) { return; }

    bool isBusy = device->nm->isBusy() || device->nm->states->bedOccupied ||
                  device->nm->states->hasError ||
                  device->nm->states->updating;

    fromKValueTextCtrl->Enable(!isBusy);
    toKValueTextCtrl->Enable(!isBusy);
    stepValueTextCtrl->Enable(!isBusy);
    startCalibrationButton->Enable(!isBusy);
    spinner->Show(isBusy && isCalibStarted);
    spinnerText->Show(isBusy && isCalibStarted);

    if (isCalibStarted && !isBusy) { isCalibStarted = false; }

    checkRule(deviceAvailableRuleIcon, deviceAvailableRuleText,
              [&]() { return !isBusy; });

    Layout();
    Refresh();
    FitInside();
}

void PressureAdvanceCalibrationDialog::refreshCalibSummary(double from,
                                                           double to,
                                                           double step)
{
    auto line_count = static_cast<int>(
        std::llround(std::ceil((to - from) / step)) + 1);

    calibSummaryText->SetLabel(
        wxString::Format(_L("%d lines will be generated"), line_count));
}

} // namespace Slic3r::GUI
