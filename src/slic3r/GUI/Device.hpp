#ifndef slic3r_Device_hpp_
#define slic3r_Device_hpp_

#include <wx/panel.h>
#include <wx/statline.h>
#include <wx/gauge.h>
#include <boost/log/trivial.hpp>

#include "../Utils/NetworkMachine.hpp"

namespace Slic3r {
namespace GUI {
#define DEVICE_HEIGHT 80

class RoundedPanel: public wxPanel
{
public:
    RoundedPanel(wxPanel *parent, int id, wxString label, wxSize size, wxColour bgColour, wxColour fgColour);

    void OnPaint(wxPaintEvent& event);
    void SetAvatar(wxBitmap& bmp);
    void Clear();
    void SetText(wxString& text);
private:
    wxRect ShrinkToSize(const wxRect &container, const int margin, const int thickness);

    wxColour m_bgColour;
    wxColour m_fgColour;
    wxString m_label;
    wxBitmap m_avatar;
};

class CustomProgressBar: public wxPanel
{
public:
    CustomProgressBar(wxPanel *parent, int id, wxSize size);

    void OnPaint(wxPaintEvent& event);
    void SetValue(uint8_t value) { m_progress = value; Refresh(); }
    void SetColour(wxColour c) { m_fgColour = c; Refresh(); }
private:
    wxColour m_fgColour;
    uint8_t m_progress;
};

class Device : public wxPanel {
public:
    Device(NetworkMachine* nm, wxWindow* parent);
    ~Device();
    wxString name;
    NetworkMachine* nm; // network machine.

    void updateStates();
    void updateProgress();
    void updateStatus();
    void avatarReady();

    void enablePrintNowButton(bool enable);
private:
    void confirm(std::function<void()> cb);
    wxSizer* m_mainSizer; // vertical sizer (device sizer - horizontal line (seperator).
    wxSizer* m_deviceSizer;  // horizontal sizer (avatar | right pane)).
    wxSizer* m_rightSizer; // vertical right pane. (name - status - progress bar).

    CustomProgressBar* m_progressBar; // progress bar.
    wxBitmapButton* m_btnSayHi; // hi button.
    wxBitmapButton* m_btnPreheat; // preheat button.
    wxBitmapButton* m_btnPause; // pause button.
    wxBitmapButton* m_btnResume; // resume button.
    wxBitmapButton* m_btnCancel; // cancel button.
    wxBitmapButton* m_btnExpandCollapse; // expand/collapse button.
    wxButton* m_btnPrintNow; // print now button.
    wxStaticText* m_txtStatus; // status text.
    wxStaticText* m_txtProgress; // progress text.

    RoundedPanel* m_avatar;

    wxBitmap* m_bitPreheatActive;
    wxBitmap* m_bitPreheatDeactive;
};
} // namespace GUI
} // namespace Slic3r
#endif
