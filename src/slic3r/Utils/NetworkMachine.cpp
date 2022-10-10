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
    m_running = true;
    ws->run();
    ioc.run(); // Run the I/O service.
}

void NetworkMachine::onWSConnect()
{
}

void NetworkMachine::onWSError(string message)
{
    BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine - WS error: %1% on machine [%2% - %3%].") % message % name % ip;
    if (!m_running) return;
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
            //name = pt.get<string>("name", name); // already got this from broadcast receiver. might be good for static ip.
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
    } catch(...) {
        BOOST_LOG_TRIVIAL(warning) << "Cannot parse machine message json.";
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
    //delete m_ws;
    if (runnerThread.joinable())
        runnerThread.join();
    if (ftpThread.joinable())
        ftpThread.join();
}

void NetworkMachine::downloadAvatar()
{
    if (!m_running) return;

    ftpThread = boost::thread(&NetworkMachine::ftpRun, this);
    ftpThread.detach();
}

struct response {
    char *memory;
    size_t size;
};

static size_t mem_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct response *mem = static_cast<response*>(userp);

    void *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) { /* out of memory! */
        BOOST_LOG_TRIVIAL(warning) << "Networkmachine - not enough memory (realloc returned NULL)";
        return 0;
    }

    mem->memory = static_cast<char*>(ptr);
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

void NetworkMachine::ftpRun()
{
    CURL *curl;
    CURLcode res;
    struct response chunk = { .memory = nullptr, .size = 0 };

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (!curl) return;

    std::string url = "ftp://" + ip + ":" + std::to_string(m_ftpPort) + "/snapshot.png";
    ::curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_cb);
    ::curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&chunk));
    //::curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    res = curl_easy_perform(curl);
    ::curl_easy_cleanup(curl);

    if (CURLE_OK != res) {
        BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine - Couldn't connect to machine [%1% - %2%] for downloading avatar.") % name % ip;
        return;
    }

    wxMemoryInputStream s (chunk.memory, chunk.size);
    m_avatar = wxBitmap(wxImage(s, wxBITMAP_TYPE_PNG));

    if (this->m_running && m_avatar.IsOk()) {
        wxCommandEvent evt(EVT_MACHINE_AVATAR_READY, wxID_ANY);
        evt.SetString(this->ip);
        evt.SetEventObject(this->m_evtHandler);
        wxPostEvent(this->m_evtHandler, evt);
    }

    curl_global_cleanup();
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
    for (auto& it : m_machineMap)
        m_machineMap[it.first]->shutdown(); // shutdown each before clearing.
    m_machineMap.clear();
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
    boost::lock_guard<boost::mutex> maplock(m_mtx);
    if (m_machineMap.find(string(ip)) == m_machineMap.end()) return;
    m_machineMap[ip]->shutdown();
    m_machineMap.erase(ip);
}
} // namespace Slic3r
