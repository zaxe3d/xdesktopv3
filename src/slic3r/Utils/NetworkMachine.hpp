#ifndef slic3r_NetworkMachine_hpp_
#define slic3r_NetworkMachine_hpp_

#include "WebSocket.hpp"

#include <algorithm> // std::min

#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <curl/curl.h>
#include <wx/protocol/ftp.h>

using namespace std;
using namespace boost::algorithm;
using namespace boost::property_tree;
using boost::mutex;
using boost::lock_guard;
using boost::unordered_map;

namespace Slic3r {
class NetworkMachine; // forward declaration.

// custome connection established message.
struct MachineEvent : public wxEvent
{
    NetworkMachine* nm;
    MachineEvent(wxEventType eventType, NetworkMachine* nm, int winid) :
        wxEvent(winid, eventType),
        nm(nm)
        {}
    virtual wxEvent *Clone() const { return new MachineEvent(*this); }
};

// custome new mesage event.
struct MachineNewMessageEvent : public wxEvent
{
    ptree pt;
    NetworkMachine* nm;
    string event;
    MachineNewMessageEvent(wxEventType eventType, string event, boost::property_tree::ptree pt, NetworkMachine* nm, int winid) :
        wxEvent(winid, eventType),
        event(event),
        pt(pt),
        nm(nm)
        {}
    virtual wxEvent *Clone() const { return new MachineNewMessageEvent(*this); }
};

// Machine events. Open, close and new message for the NetworkMachineContainer
wxDECLARE_EVENT(EVT_MACHINE_OPEN, MachineEvent);
wxDECLARE_EVENT(EVT_MACHINE_CLOSE, MachineEvent);
wxDECLARE_EVENT(EVT_MACHINE_NEW_MESSAGE, MachineNewMessageEvent);
wxDECLARE_EVENT(EVT_MACHINE_AVATAR_READY, wxCommandEvent);

struct MachineStates { // states.
    bool uploading;
    bool updating;
    bool calibrating;
    bool bedOccupied;
    bool usbPresent;
    bool preheat;
    bool printing;
    bool heating;
    bool paused;
    inline bool ptreeStringtoBool(ptree pt, string prop) {
        return pt.get<string>(prop, "False") == "True";
    }
};

struct MachineAttributes // attributes.
{
    string fileName;
    string nozzle;
    string filamentColor;
    string printingFile;
    string material;
    string deviceModel;
    bool hasPin;
    bool hasNFCSpool;
    bool hasSnapshot;
    bool isLite;
    bool isNoneTLS;
    bool snapshotURL;
    float elapsedTime;
    float estimatedTime;
    int startTime;
    int filamentRemaining;
    int firmwareVersion;
};

class NetworkMachine
{
public:
    NetworkMachine(string ip, int port, string name, wxEvtHandler* hndlr); // construct network machine.
    ~NetworkMachine();

    void run(); // start network machine by connecting to ws.
    void ftpRun(); // start downloading avatar in another thread.

    // Actions
    void sayHi();
    void togglePreheat();
    void cancel();
    void pause();
    void resume();
    void upload(const char *filename);
    void downloadAvatar();

    void shutdown() { m_running = false; }

    string id; // unique id of the machine.
    string name; // name of the machine.
    string ip; // ip of the machine.
    int port; // port number of the web socket server on machine.

    int progress = 0;

    MachineAttributes* attr; // attributes,
    MachineStates* states; // states,
    boost::thread runnerThread;
    boost::thread ftpThread;
    typedef std::function<void(int percent)>  progress_callback_t;
    void setUploadProgressCallback(progress_callback_t cb) { m_uploadProgressCallback = cb; }
    wxBitmap& getAvatar() {
        boost::lock_guard<boost::mutex> avatarlock(m_avatarMtx);
        return m_avatar;
    }
private:
    progress_callback_t m_uploadProgressCallback;
#ifdef _WIN32
    USHORT
#else
    ushort
#endif
    m_ftpPort = 9494;

    // Websocket callbacks.
    void onWSConnect(); // Websocket connect callback.
    void onWSRead(string message); // Websocket read message callback.
    void onWSError(string message); // Websocket error callback.

    void request(const char* command); // does a request with intended command on device.
    void send(ptree pt); // sends ptree as json string to websocket (m_ws).

    wxEvtHandler* m_evtHandler; // parent event handler.
    Websocket* m_ws; // websocket
    wxBitmap m_avatar; // avatar image via FTP.
    boost::mutex m_avatarMtx; // allows read operations on m_avatar without locking.
    bool m_running;
};

class NetworkMachineContainer : public std::enable_shared_from_this<NetworkMachineContainer>, public wxEvtHandler
{
public:
    NetworkMachineContainer();
    ~NetworkMachineContainer();
    shared_ptr<NetworkMachine> addMachine(string ip, int port, string name);
    void removeMachine(string id);
private:
    boost::mutex m_mtx; // allows read operations on m_machineMap without locking.
    boost::unordered_map<std::string, shared_ptr<NetworkMachine>> m_machineMap;
};

} // namespace Slic3r
#endif // slic3r_NetworkMachine_hpp_
