#ifndef _WebServer_WebServer_h_
#define _WebServer_WebServer_h_

#include <Core/Core.h>


using namespace Upp;

namespace WebServer {

// Function type for handling commands
using CommandCallback = std::function<Tuple<bool, String>(const String&)>;

// Public API functions
bool start(int port);
void stop();
bool is_running();
void send_output(const String& output);
void set_command_callback(CommandCallback callback);

} // namespace WebServer

#endif
