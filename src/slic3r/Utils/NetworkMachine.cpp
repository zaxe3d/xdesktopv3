#include "NetworkMachine.hpp"
#include <libslic3r/Utils.hpp>

namespace fs = boost::filesystem;

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
            attr->isNoneTLS = attr->deviceModel.find("z3") != string::npos || attr->isLite;
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
    struct response chunk;

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

size_t file_read_cb(char *buffer, size_t size, size_t nitems, void *userp)
{
    auto stream = reinterpret_cast<fs::ifstream*>(userp);

    try {
        stream->read(buffer, size * nitems);
    } catch (const std::exception &) {
        return CURL_READFUNC_ABORT;
    }
    return stream->gcount();
}

int xfercb(void *userp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    if (ultotal <= 0.0) return 0;

    auto self = static_cast<NetworkMachine*>(userp);

    int up = (int)(((double)ulnow / (double)ultotal) * 100);
    self->m_uploadProgressCallback(up);
    return 0;
}

void NetworkMachine::upload(const char *filename)
{
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (!curl) return;
    fs::path path = fs::path(filename);
    boost::system::error_code ec;
    boost::uintmax_t filesize = file_size(path, ec);
    std::unique_ptr<fs::ifstream> putFile;

    m_uploadProgressCallback(0); // reset
    states->uploading = true;
    if (!ec) {
        putFile = std::make_unique<fs::ifstream>(path);
        ::curl_easy_setopt(curl, CURLOPT_READDATA, (void *) (putFile.get()));
        ::curl_easy_setopt(curl, CURLOPT_INFILESIZE, filesize);
    }

    std::string url = "ftp://" + ip + ":" + std::to_string(m_ftpPort) + "/" + path.filename().string();
    ::curl_easy_setopt(curl, CURLOPT_USERNAME, "zaxe");
    ::curl_easy_setopt(curl, CURLOPT_PASSWORD, "zaxe");
    ::curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    ::curl_easy_setopt(curl, CURLOPT_READFUNCTION, file_read_cb);
    ::curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    ::curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    ::curl_easy_setopt(curl, CURLOPT_VERBOSE, get_logging_level() >= 5);
    ::curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfercb);
    ::curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, static_cast<void *>(this));

    if ( ! attr->isNoneTLS) {
        ::curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
        ::curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        ::curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    res = curl_easy_perform(curl);
    ::curl_easy_cleanup(curl);
    putFile.reset();
    states->uploading = false;
    if (CURLE_OK != res) {
        BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine - Couldn't connect to machine [%1% - %2%] for uploading print.") % name % ip;
        return;
    }

    curl_global_cleanup();
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
