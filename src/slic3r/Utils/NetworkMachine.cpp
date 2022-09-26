#include "NetworkMachine.hpp"

namespace Slic3r {
wxDEFINE_EVENT(EVT_MACHINE_OPEN, MachineEvent);
wxDEFINE_EVENT(EVT_MACHINE_CLOSE, MachineEvent);
wxDEFINE_EVENT(EVT_MACHINE_NEW_MESSAGE, MachineNewMessageEvent);
wxDEFINE_EVENT(EVT_MACHINE_AVATAR_READY, wxCommandEvent);

NetworkMachine::NetworkMachine(string ip, int port, string name, wxEvtHandler* hndlr) :
    ip(ip),
    port(port),
    name(name),
    m_evtHandler(hndlr),
    attr(new MachineAttributes()),
    states(new MachineStates())
{
}

void NetworkMachine::run()
{
    net::io_context ioc; // The io_context is required for websocket I/O
    auto ws = make_shared<Websocket>(ip, port, ioc); // Run the websocket for this machine.
    ws->addReadEventHandler(boost::bind(&NetworkMachine::onWSRead, this, _1));
    ws->addConnectEventHandler(boost::bind(&NetworkMachine::onWSConnect, this));
    ws->addErrorEventHandler(boost::bind(&NetworkMachine::onWSError, this, _1));
    m_ws = ws.get();
    ws->run();
    ioc.run(); // Run the I/O service.
}

void NetworkMachine::onWSConnect()
{
}

void NetworkMachine::onWSError(string message)
{
    BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine - WS error: %1% on machine [%2% - %3%].") % message % name % ip;
    MachineEvent evt(EVT_MACHINE_CLOSE, this, wxID_ANY);
    evt.SetEventObject(this->m_evtHandler);
    wxPostEvent(this->m_evtHandler, evt);
}

void NetworkMachine::onWSRead(string message)
{
    //BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine onReadWS: %1%") % message;
    if (!m_running) return;
    stringstream jsonStream;
    jsonStream.str(message);

    ptree pt; // construct root obj.
    read_json(jsonStream, pt);

    try {
        auto event = pt.get<string>("event");
        if (event == "ping" || event == "temperature_change") return; // ignore...
        //BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine event. [%1%:%2% - %3%]") % name % ip % event;
        if (event == "hello") {
            attr->deviceModel = to_lower_copy(pt.get<string>("device_model", "x1"));
            attr->material = to_lower_copy(pt.get<string>("material", "zaxe_abs"));
            attr->nozzle = pt.get<string>("nozzle", "0.4");
            attr->hasSnapshot = contains(attr->deviceModel, 'z');
            attr->isLite = attr->deviceModel.find("lite") != string::npos || attr->deviceModel == "x3";
            // printing
            attr->printingFile = pt.get<string>("filename", "");
            attr->elapsedTime = pt.get<float>("elapsed_time", 0);
            attr->estimatedTime = time(nullptr) - attr->elapsedTime;
            if (!attr->isLite) {
                attr->hasPin = to_lower_copy(pt.get<string>("has_pin", "false")) == "true";
                attr->hasNFCSpool = to_lower_copy(pt.get<string>("has_nfc_spool", "false")) == "true";
                attr->filamentColor = to_lower_copy(pt.get<string>("filament_color", "unknown"));
            }
        }
        if (event == "hello" || event == "states_update") {
            // states
            states->updating       = states->ptreeStringtoBool(pt, "is_updating");
            states->calibrating    = states->ptreeStringtoBool(pt, "is_calibrating");
            states->bedOccupied    = states->ptreeStringtoBool(pt, "is_bed_occupied");
            states->usbPresent     = states->ptreeStringtoBool(pt, "is_usb_present");
            states->preheat        = states->ptreeStringtoBool(pt, "is_preheat");
            states->printing       = states->ptreeStringtoBool(pt, "is_printing");
            states->heating        = states->ptreeStringtoBool(pt, "is_heating");
            states->paused         = states->ptreeStringtoBool(pt, "is_paused");
        }
        if (event == "new_name")
            name = pt.get<string>("name", "Zaxe");
        if (event == "material_change")
            attr->material = to_lower_copy(pt.get<string>("material", "zaxe_abs"));
        if (event == "nozzle_change")
            attr->nozzle = pt.get<string>("nozzle", "0.4");
        if (event == "pin_change")
            attr->hasPin = to_lower_copy(pt.get<string>("has_pin", "false")) == "true";
        if (event == "start_print") {
            attr->printingFile = pt.get<string>("filename", "");
            attr->elapsedTime = pt.get<float>("elapsed_time", 0);
            attr->estimatedTime = time(nullptr) - attr->elapsedTime;
        }
        if (event == "spool_data_change") {
            attr->hasNFCSpool = to_lower_copy(pt.get<string>("has_nfc_spool", "false")) == "true";
            attr->filamentColor = to_lower_copy(pt.get<string>("filament_color", "unknown"));
        }
        if (event == "hello") { // gather up all the events up untill here.
            MachineEvent evt(EVT_MACHINE_OPEN, this, wxID_ANY); // ? get window id here ?; // ? get window id here ?
            evt.SetEventObject(this->m_evtHandler);
            wxPostEvent(this->m_evtHandler, evt);
        } else {
            MachineNewMessageEvent evt(EVT_MACHINE_NEW_MESSAGE, event, pt, this, wxID_ANY); // ? get window id here ?
            evt.SetEventObject(this->m_evtHandler);
            wxPostEvent(this->m_evtHandler, evt);
        }
    } catch(exception const& ex) {
        BOOST_LOG_TRIVIAL(warning) << boost::format("Cannot parse machine message json: [%1%].") % ex.what();
    }
}

void NetworkMachine::sayHi()
{
    request("say_hi");
}

void NetworkMachine::cancel()
{
    request("cancel");
}

void NetworkMachine::pause()
{
    request("pause");
}

void NetworkMachine::resume()
{
    request("pause");
}

void NetworkMachine::togglePreheat()
{
    request("toggle_preheat");
}

void NetworkMachine::request(const char* command)
{
    ptree pt; // construct root obj.
    pt.put("request", command);
    send(pt);
}

void NetworkMachine::send(ptree pt)
{
    stringstream ss;
    json_parser::write_json(ss, pt);
    m_ws->send(ss.str());
}

NetworkMachine::~NetworkMachine()
{
    delete attr;
    delete states;
    delete m_ws;
    runnerThread.join();
}

void NetworkMachine::downloadAvatar()
{
    boost::thread t([this] () {
        wxFTP ftp;
        if (!ftp.Connect(ip, m_ftpPort)) {
            BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine - Couldn't connect to machine [%1% - %2%] for downloading avatar.") % name % ip;
            return;
        }
        if (ftp.GetError() != wxPROTO_NOERR) return;
        wxInputStream *in = ftp.GetInputStream("snapshot.png");
        if (!in)
            BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine - Couldn't get avatar on machine [%1% - %2%].") % name % ip;
        else {
            boost::lock_guard<boost::mutex> avatarlock(m_avatarMtx);
            m_avatar = wxBitmap(wxImage(*in, wxBITMAP_TYPE_PNG));
            if (m_avatar.IsOk()) {
                wxCommandEvent evt(EVT_MACHINE_AVATAR_READY, wxID_ANY);
                evt.SetString(this->ip);
                evt.SetEventObject(this->m_evtHandler);
                wxPostEvent(this->m_evtHandler, evt);
            }
            delete in;
        }
        //ftp.Close();
    });
    t.detach();
}

void NetworkMachine::upload(const char *filename)
{
    wxFTP ftp;
    ftp.SetUser("zaxe");
    ftp.SetPassword("zaxe");

    if (!ftp.Connect(ip, m_ftpPort)) {
        BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine - Couldn't connect to machine [%1% - %2%] for uploading file.") % name % ip;
        return;
    }
    wxOutputStream *out = ftp.GetOutputStream(boost::filesystem::path(filename).filename().string());
    wxFile file = wxFile(filename, wxFile::read);
    char* data = new char[file.Length()];
    file.Read(data, file.Length());
    wxLongLong_t bufferSize = 8192;
    wxLongLong_t sent = 0;
    states->uploading = true;
    if (m_uploadProgressCallback) {
        progress = 0;
        m_uploadProgressCallback(progress); // reset.
    }
    while (sent < file.Length()) {
        out->Write(&data[sent], std::min(bufferSize, file.Length() - sent));
        sent += out->LastWrite();
        if (m_uploadProgressCallback) {
            progress = 100 * sent / file.Length();
            m_uploadProgressCallback(progress);
        }
    }
    states->uploading = false;
    delete out;
    delete [] data;
    file.Close();
    ftp.Close();
}

NetworkMachineContainer::NetworkMachineContainer() {}

NetworkMachineContainer::~NetworkMachineContainer() {
    boost::lock_guard<boost::mutex> maplock(m_mtx);
    m_machineMap.clear(); // since the map holds shared_ptrs clear is enough?
}

shared_ptr<NetworkMachine> NetworkMachineContainer::addMachine(string ip, int port, string name)
{
    boost::lock_guard<boost::mutex> maplock(m_mtx);
    // If we have it already with the same ip, - do nothing.
    if (m_machineMap.find(string(ip)) != m_machineMap.end()) return nullptr;
    BOOST_LOG_TRIVIAL(info) << boost::format("NetworkMachineContainer - Trying to connect machine: [%1% - %2%].") % name % ip;
    auto nm = make_shared<NetworkMachine>(ip, port, name, this);
    nm->runnerThread = boost::thread(&NetworkMachine::run, nm);
    nm->runnerThread.detach();
    m_machineMap[ip] = nm; // Hold this for the carousel.

    return nm;
}

void NetworkMachineContainer::removeMachine(string ip)
{
    if (m_machineMap.find(string(ip)) == m_machineMap.end()) return;
    boost::lock_guard<boost::mutex> maplock(m_mtx);
    m_machineMap[ip]->shutdown();
    m_machineMap.erase(ip);
}
} // namespace Slic3r
