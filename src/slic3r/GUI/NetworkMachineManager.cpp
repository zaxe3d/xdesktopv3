#include "NetworkMachineManager.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp" // for wxGetApp().app_config
#include "I18N.hpp"
#include "libslic3r/Utils.hpp"

#include <wx/artprov.h>

namespace Slic3r {
namespace GUI {

NetworkMachineManager::NetworkMachineManager(wxWindow* parent, wxSize size) :
    wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(size)),
    m_mainSizer(new wxBoxSizer(wxVERTICAL)),
    m_deviceListSizer(new wxBoxSizer(wxVERTICAL)),
    m_broadcastReceiver(new BroadcastReceiver()),
    m_networkMContainer(new NetworkMachineContainer()),
    m_printNowButtonEnabled(false)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif

    wxStaticText *noDeviceFoundText =
        new wxStaticText(this, wxID_ANY,
                         _L("Can not find a Zaxe on the network"),
                         wxDefaultPosition, wxDefaultSize);
    wxGetApp().UpdateDarkUI(noDeviceFoundText);
    wxFont label_font = wxGetApp().normal_font();
    label_font.SetPointSize(14);
    noDeviceFoundText->SetFont(label_font);

    auto warningIcon = new wxStaticBitmap(this, wxID_ANY,
                                          wxArtProvider::GetBitmap(
                                              wxART_WARNING),
                                          wxDefaultPosition, wxSize(35, 35));

    m_warningSizer = new wxBoxSizer(wxHORIZONTAL);
    m_warningSizer->Add(warningIcon, 0, wxALIGN_CENTER | wxALL, 5);
    m_warningSizer->Add(noDeviceFoundText, 0, wxALIGN_CENTER | wxALL, 1);
    m_warningSizer->Show(m_deviceMap.empty());

    m_mainSizer->Add(m_deviceListSizer, 1, wxEXPAND | wxALL, 5);
    m_mainSizer->Add(m_warningSizer, 0, wxALIGN_CENTER);

    SetSizer(m_mainSizer);

    // As result of below line we can see the empty block at the bottom of the carousel
    //SetScrollbars(0, DEVICE_HEIGHT, 0, DEVICE_HEIGHT);
    // This fixes the above problem.
    SetScrollRate(0, 5);
    FitInside();

    // start listenting for devices here on the network.
    m_broadcastReceiver->Bind(EVT_BROADCAST_RECEIVED, &NetworkMachineManager::onBroadcastReceived, this);

    // add custom ips here.
    auto ips = wxGetApp().app_config->get_custom_ips();
    for (int i = 0; i < ips.size(); i++)
        addMachine(ips[i], 9294, "Zaxe (m.)");
}

void NetworkMachineManager::enablePrintNowButton(bool enable)
{
    if (enable == m_printNowButtonEnabled) return;
    for (auto& it : m_deviceMap) {
        if (m_deviceMap[it.first] == nullptr) continue;
        m_deviceMap[it.first]->enablePrintNowButton(enable);
    }
    m_printNowButtonEnabled = enable;
}

void NetworkMachineManager::onBroadcastReceived(wxCommandEvent &event)
{
    std::stringstream jsonStream;
    jsonStream.str(std::string(event.GetString().utf8_str().data())); // wxString to std::string

    boost::property_tree::ptree pt; // construct root obj.
    boost::property_tree::read_json(jsonStream, pt);
    try {
        this->addMachine(pt.get<std::string>("ip"), pt.get<int>("port"), pt.get<std::string>("id"));
    } catch(std::exception ex) {
        BOOST_LOG_TRIVIAL(warning) << boost::format("Cannot parse broadcast message json: [%1%].") % ex.what();
    }
}

void NetworkMachineManager::addMachine(std::string ip, int port, std::string id)
{
    auto machine  = this->m_networkMContainer->addMachine(ip, port, id);
    if (machine != nullptr) {
        this->m_networkMContainer->Bind(EVT_MACHINE_OPEN, &NetworkMachineManager::onMachineOpen, this);
        this->m_networkMContainer->Bind(EVT_MACHINE_CLOSE, &NetworkMachineManager::onMachineClose, this);
        this->m_networkMContainer->Bind(EVT_MACHINE_NEW_MESSAGE, &NetworkMachineManager::onMachineMessage, this);
        this->m_networkMContainer->Bind(EVT_MACHINE_AVATAR_READY, &NetworkMachineManager::onMachineAvatarReady, this);
    }
}
void NetworkMachineManager::onMachineOpen(MachineEvent &event)
{
    // Now we can add this to UI.
    if (!event.nm || m_deviceMap.find(event.nm->ip) != m_deviceMap.end()) return;
    BOOST_LOG_TRIVIAL(info) << boost::format("NetworkMachineManager - Connected to machine: [%1% - %2%].") % event.nm->name % event.nm->ip;

    Freeze();
    shared_ptr<Device> d = make_shared<Device>(event.nm, this);
    d->enablePrintNowButton(m_printNowButtonEnabled);

    if (!filter_text.empty() && d->getName().Lower().Find(filter_text.Lower()) == wxNOT_FOUND) {
        d->Hide();
    }
    m_deviceMap[event.nm->ip] = d;
    m_warningSizer->Show(m_deviceMap.empty());
    m_deviceListSizer->Add(d.get());
    Thaw();

    m_mainSizer->Layout();
    FitInside();
    Refresh();
}

void NetworkMachineManager::onMachineClose(MachineEvent &event)
{
    if (!event.nm) return;
    if (m_deviceMap.erase(event.nm->ip) == 0) return; // couldn't delete so don't continue...
    BOOST_LOG_TRIVIAL(info) << boost::format("NetworkMachineManager - Closing machine: [%1% - %2%].") % event.nm->name % event.nm->ip;
    this->m_networkMContainer->removeMachine(event.nm->ip);
    m_warningSizer->Show(m_deviceMap.empty());
    m_mainSizer->Layout();
    FitInside();
    Refresh();
}

void NetworkMachineManager::onMachineMessage(MachineNewMessageEvent &event)
{
    if (!event.nm || m_deviceMap[event.nm->ip] == nullptr) return;
    if (event.event == "states_update") {
        m_deviceMap[event.nm->ip]->updateStates();
    } else if (event.event == "print_progress" ||
               event.event == "temperature_progress" ||
               event.event == "calibration_progress") {
        event.nm->progress = event.pt.get<float>("progress", 0);
        m_deviceMap[event.nm->ip]->updateProgress();
    } else if (event.event == "new_name") {
        m_deviceMap[event.nm->ip]->setName(event.nm->name);
    } else if (event.event == "material_change") {
        m_deviceMap[event.nm->ip]->setMaterialLabel(
            event.nm->attr->materialLabel);
    } else if (event.event == "nozzle_change") {
        m_deviceMap[event.nm->ip]->setNozzle(event.nm->attr->nozzle);
    } else if (event.event == "pin_change") {
        m_deviceMap[event.nm->ip]->setPin(event.nm->attr->hasPin);
    } else if (event.event == "start_print") {
        m_deviceMap[event.nm->ip]->setFileStart();
    } else if (event.event == "upload_done") {
        m_deviceMap[event.nm->ip]->onUploadDone();
    }
}

void NetworkMachineManager::onMachineAvatarReady(wxCommandEvent &event)
{
    string ip = event.GetString().ToStdString();
    if (m_deviceMap.find(ip) == m_deviceMap.end()) return;
    //BOOST_LOG_TRIVIAL(debug) << boost::format("NetworkMachineManager - Avatar ready on machine: [%1%].") % ip;
    m_deviceMap[ip]->avatarReady();
}

NetworkMachineManager::~NetworkMachineManager()
{
    m_deviceMap.clear(); // since the map holds shared_ptrs clear is enough
    delete m_broadcastReceiver;
    delete m_networkMContainer;
}

void NetworkMachineManager::onModeChanged()
{
    std::for_each(m_deviceMap.begin(), m_deviceMap.end(), [](auto &d) {
        if (d.second) d.second->onModeChanged();
    });
}

void NetworkMachineManager::filter(const wxString &text)
{
    filter_text = text;
    for (auto &[ip, dev] : m_deviceMap) {
        if (!dev) {
            continue;
        }
        if (dev->getName().Lower().Find(filter_text.Lower()) == wxNOT_FOUND) {
            dev->Hide();
        } else {
            dev->Show();
        }
    }

    m_mainSizer->Layout();
    Refresh();
    FitInside();
}
} // namespace GUI
} // namespace Slic3r
