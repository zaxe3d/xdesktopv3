#pragma once

#include "libslic3r/Calib.hpp"
#include <wx/activityindicator.h>

namespace Slic3r::GUI {

class PressureAdvanceCalibrationDialog : public wxDialog
{
public:
    PressureAdvanceCalibrationDialog(wxWindow       *parent,
                                     wxWindowID      id,
                                     const wxString &title);

    void checkState();

private:
    wxBoxSizer          *sizer;
    wxTextCtrl          *fromKValueTextCtrl;
    wxTextCtrl          *toKValueTextCtrl;
    wxTextCtrl          *stepValueTextCtrl;
    wxButton            *startCalibrationButton;
    wxButton            *redirectToFilamentSettingsButton;
    wxStaticBitmap      *fromValueRuleIcon;
    wxStaticBitmap      *toValueRuleIcon;
    wxStaticBitmap      *stepValueRuleIcon;
    wxStaticBitmap      *stepNullValueRuleIcon;
    wxStaticBitmap      *deviceAvailableRuleIcon;
    wxStaticBitmap      *calibSummaryIcon;
    wxStaticText        *fromValueRuleText;
    wxStaticText        *toValueRuleText;
    wxStaticText        *stepValueRuleText;
    wxStaticText        *stepNullValueRuleText;
    wxStaticText        *deviceAvailableRuleText;
    wxStaticText        *calibSummaryText;
    wxActivityIndicator *spinner;
    wxStaticText        *spinnerText;

    bool isCalibStarted{false};

    void createInputSection();
    void createInfoSection();
    void createActionSection();
    void refreshCalibSummary(double from, double to, double step);

    void checkInputs();
    bool checkRule(wxStaticBitmap           *icon,
                   wxStaticText             *text,
                   std::function<bool(void)> ruleFunc);
};
} // namespace Slic3r::GUI
