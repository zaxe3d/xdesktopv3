#include "NetworkMachineManager.hpp"

namespace Slic3r {
namespace GUI {

NetworkMachineManager::NetworkMachineManager(wxWindow* parent) :
    wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(parent->GetSize().GetWidth(), -1)),
    m_scrolledSizer(new wxBoxSizer(wxVERTICAL)),
    m_broadcastReceiver(new BroadcastReceiver()),
    m_networkMContainer(new NetworkMachineContainer()),
    m_printNowButtonEnabled(false)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif
    SetSizer(m_scrolledSizer);
    // As result of below line we can see the empty block at the bottom of the carousel
    //SetScrollbars(0, DEVICE_HEIGHT, 0, DEVICE_HEIGHT);
    // This fixes the above problem.
    SetScrollRate(0, 5);
    FitInside();

    // start listenting for devices here on the network.
    m_broadcastReceiver->Bind(EVT_BROADCAST_RECEIVED, &NetworkMachineManager::onBroadcastReceived, this);
}

void NetworkMachineManager::enablePrintNowButton(bool enable)
{
    if (enable == m_printNowButtonEnabled) return;
    for (auto& it : m_deviceMap)
        m_deviceMap[it.first]->enablePrintNowButton(enable);
    m_printNowButtonEnabled = enable;
}

void NetworkMachineManager::onBroadcastReceived(wxCommandEvent &event)
{
    std::stringstream jsonStream;
    jsonStream.str(std::string(event.GetString().utf8_str().data())); // wxString to std::string

    boost::property_tree::ptree pt; // construct root obj.
    boost::property_tree::read_json(jsonStream, pt);

    try {
        auto machine  = this->m_networkMContainer->addMachine(
            pt.get<std::string>("ip"),
            pt.get<int>("port"),
            pt.get<std::string>("id"));
        if (machine != nullptr) {
            this->m_networkMContainer->Bind(EVT_MACHINE_OPEN, &NetworkMachineManager::onMachineOpen, this);
            this->m_networkMContainer->Bind(EVT_MACHINE_CLOSE, &NetworkMachineManager::onMachineClose, this);
            this->m_networkMContainer->Bind(EVT_MACHINE_NEW_MESSAGE, &NetworkMachineManager::onMachineMessage, this);
            this->m_networkMContainer->Bind(EVT_MACHINE_AVATAR_READY, &NetworkMachineManager::onMachineAvatarReady, this);
        }
    } catch(std::exception ex) {
        BOOST_LOG_TRIVIAL(warning) << boost::format("Cannot parse broadcast message json: [%1%].") % ex.what();
    }
}

void NetworkMachineManager::onMachineOpen(MachineEvent &event)
{
    // Now we can add this to UI.
    if (!event.nm || m_deviceMap.find(event.nm->ip) != m_deviceMap.end()) return;
    BOOST_LOG_TRIVIAL(info) << boost::format("NetworkMachineManager - Connected to machine: [%1% - %2%].") % event.nm->name % event.nm->ip;
    shared_ptr<Device> d = make_shared<Device>(event.nm, this);
    d->enablePrintNowButton(m_printNowButtonEnabled);
    m_deviceMap[event.nm->ip] = d;
    m_scrolledSizer->Add(d.get());
    m_scrolledSizer->Layout();
    FitInside();
    Refresh();
}

void NetworkMachineManager::onMachineClose(MachineEvent &event)
{
    if (!event.nm) return;
    if (m_deviceMap.erase(event.nm->ip) == 0) return; // couldn't delete so don't continue...
    BOOST_LOG_TRIVIAL(info) << boost::format("NetworkMachineManager - Closing machine: [%1% - %2%].") % event.nm->name % event.nm->ip;
    this->m_networkMContainer->removeMachine(event.nm->ip);
    m_deviceMap.erase(event.nm->ip);
    m_scrolledSizer->Layout();
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
        m_deviceMap[event.nm->ip]->nm->progress = event.pt.get<float>("progress", 0);
        m_deviceMap[event.nm->ip]->updateProgress();
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
} // namespace GUI
} // namespace Slic3r
