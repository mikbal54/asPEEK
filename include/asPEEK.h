/*
Copyright (c) 2012 Muhammed Ikbal Akpaca

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/


#ifndef asPEEK_H
#define asPEEK_H

#include <angelscript.h>
#include <websocketpp.hpp>
#include <vector>
#include <boost/unordered_map.hpp>

// comment this if you don't want CScriptArray support
#define asPEEK_HASARRAY
// display console output
#define asPEEK_DEBUG

class asPEEK;

namespace asPEEK_UTILS
{
  inline bool ToInt( const std::string &str, int &val)
  {
    std::istringstream ss(str);
    ss >> val;
    return ss.eof();
  }
};


class ClientData
{
public:

  ClientData() { id = "";}

  ClientData(const std::string &id)
  {
    this->id = id;
  }

  std::string id;

};

/**
Contains address of an angelscript variable and its type id.
Used To assign a value to it or get its value 
*/
class asPEEK_Variable
{
public:

  void *address;
  int tid;

  asPEEK_Variable(void *addr, int type_id)
  {
    address = addr;
    tid = type_id;
  }

  asPEEK_Variable()
  {
    address = 0;
    tid = 0;
  }

  inline bool isValid() const
  {
    if(address)
      return true;
    else
      return false;
  }

  inline bool isCPPObject() const
  {
    if(tid & asTYPEID_APPOBJECT)
      return true;
    else
      return false;
  }

};

class asPEEK_Listener: public websocketpp::server::handler
{
protected:

  friend asPEEK;

  std::map<connection_ptr, ClientData> open_connections;

  asPEEK *peek;

  websocketpp::server *endpoint;

public:

  asPEEK_Listener(asPEEK *peek)
  {
    this->peek = peek;
  }

  virtual ~asPEEK_Listener()
  { 
    for(std::map<connection_ptr, ClientData>::iterator it = open_connections.begin(); it != open_connections.end(); ++it)
    {
      it->first->close(websocketpp::close::status::NORMAL, "asPEEK server ended");
    }
    open_connections.clear();
  }

  void on_message(connection_ptr con, message_ptr msg);

  /** Store connection and info on a map */
  void on_open(connection_ptr connection);

  void on_close(connection_ptr con);

  void Send(connection_ptr con, const std::string &message);

  void SendToAll(const std::string &msg);

};

class asPEEK_Message
{
public:

  websocketpp::server::connection_ptr client;

  std::string messageContents;

};

class TrackedVariable
{
protected:

  int typeId;
  asIObjectType *obj_type;

public:

  TrackedVariable()
  {
    name = "";
    typeId = 0;
    obj_type = 0;
    parent = 0;
  }

  std::string name;

  TrackedVariable *parent;

  inline int GetTypeId() const
  {
    return typeId;
  }

  inline asIObjectType* GetObjType() const
  {
    return obj_type;
  }

  inline void SetTypeId(int tid)
  {
    typeId = tid;
  }

  inline void SetObjectType(asIObjectType *t)
  {
    obj_type = t;
  }

};

class asPEEK
{

private:

  int specialTypeCount;

  int contextCount;

  int sectionCount;

  boost::unordered_map<int, int> specialTypes; // Find how to convert this type to JSON
  boost::unordered_map<int, boost::function<std::string(void*)>> conversionMethods;

  class SectionData
  {
  public:

    int id;
    std::string name;
    std::string mod;
    std::set<int> breakpoints; /** list of breakpoints*/

  };

  boost::unordered_map<int, SectionData> sections;
  boost::unordered_map<std::string, int> sectionIds;

  /** These contexts are tracked. PEEK suspends/resumes when variable updates are needed */
  boost::unordered_map<int, asIScriptContext*> tracked_context;

  // TODO, this context might be overused, but better be safe then fast crashy code
  boost::mutex debugging_mutex;
  asIScriptContext *debuggingContext;
  /** Section current we are debugging*/
  int debuggingSection;
  /** Line we are at */
  int debuggingLine;
  /** Next stack level, used to determine step in|step out|step over*/
  unsigned int debuggingStackLevel;



  boost::mutex debugCommands_mutex;
  /** Queue for debug commands sent by client */
  std::vector<std::string> debugCommands;

  enum DebugCommand
  {
    CONTINUE,
    STEPOUT,
    STEPIN,
    STEPOVER
  };

  DebugCommand nextDebugCommand;

  bool endAll;
  boost::mutex endAllMutex;
  boost::condition_variable endAll_condition;

protected:

  friend asPEEK_Listener;

  asIScriptEngine *engine;

  boost::shared_ptr<asPEEK_Listener> listener;

  unsigned short port;

  void StartListening();

  boost::condition_variable receivedMessages_condition;
  boost::mutex receivedMessages_mutex;
  std::list<asPEEK_Message> receivedMessages;

  bool isPaused;
  bool debugInterrupt;


  void HandleMessage(asPEEK_Message &msg);

  SectionData *asPEEK::GetSectionData(const char *section);

  void LineCallback(asIScriptContext *ctx);

  /** Two threads asPEEK uses*/
  boost::thread updateThread;
  boost::thread listenThread;

  /** These modules we where look for our variables */
  std::set<asIScriptModule*> tracked_modules;

  /** Update Thread*/
  void Update();

  /** Send module names to client */
  void SendModuleNames(websocketpp::server::connection_ptr con);

  /** Send script section names */
  void SendSectionNames(websocketpp::server::connection_ptr con);

  /** Send contexts to client. This current not used by client in anyway. Will be useful when(if) profiler and flow charts are added.*/
  void SendContexts(websocketpp::server::connection_ptr con);

  /** Send current section id and line to client, only if execution is halted */
  void SendCurrentLine(websocketpp::server::connection_ptr con);

  /** Send file to client */
  void SendFile(const std::vector<std::string> &words, websocketpp::server::connection_ptr con);

  /**
  Turn a variable to string, will send this string to client 
  Third variable if set of previous variables. This is a must to stop circular lookup.
  Example:
  class Base { Child @c; Base(){ @c = Child(@this); } }; class Child { Base @base; Child(Base @b){ @base = b; }; }
  One way to stop this is lazy loading. It would be faster too.
  It is a client side implementation, not as easy to implement. Did not have time for it.
  Also still circular lookup is necessary to stop client fuckups.
  */
  std::string ToString(const asPEEK_Variable &var, std::set<void*> *previous = 0);

  /** Processes a message sent by client */
  void ProcessMessage(const std::string &msg);
  /** Client requested a value of a variable */
  void ProcessVariableRequest(std::vector<std::string> *msg);
  /** Client wants to assign a new value to a variable */
  void ProcessVaribleAssignmentRequest(std::vector<std::string> *msg);

  /** Gets variable. Only called by ProcessVariableRequest */
  asPEEK_Variable GetGlobalVariable(std::vector<std::string> &msg);

  /** Gets name of global variable and returns as asPEEK_Variable. Called by GetVariable */
  asPEEK_Variable GetGlobalVariable(const std::string &msg, std::string &modName, const std::string &ns);

  /** Find member of a variable. Called from GetVariable */
  asPEEK_Variable GetMemberVariable(const asPEEK_Variable &parent, const std::string &name);

  /** Convert an angelscript primitive to string. Also includes enumerations*/
  std::string PrimitiveToString(const asPEEK_Variable &var);

  /** Convert a script object to a JSON string. This is recursive.*/
  void ScriptObjectToString(asIScriptObject *obj, std::stringstream &ss, std::set<void*> *previous = 0);

  /** Set Breakpoint on a line*/
  void SetBreakpoint(const std::vector<std::string> &words);

  /** Remove a break point. Sends removed message to all clients*/
  void RemoveBreakpoint(const std::vector<std::string> &words);

  /** Put context in an endless loop, wait for debug commands*/
  void Debug(asIScriptContext *ctx, int line, SectionData *section);

  /** Send local stack variables a client*/
  void SendLocalVariables(websocketpp::server::connection_ptr con, asIScriptContext *ctx);

  /** Send @this members to a client*/
  void SendThisObject(websocketpp::server::connection_ptr con, asIScriptContext *ctx);

  /** Send current stack to a client. depth, function names, lines and sections*/
  void SendStack(websocketpp::server::connection_ptr con, asIScriptContext *ctx);

  /** Called when a new client is connected*/
  void NewClient(websocketpp::server::connection_ptr connection);

  /** Send value of a variable to client*/
  void SendVariable(const std::string &varname, asIScriptModule *mod, websocketpp::server::connection_ptr connection);

  /** Find variable in a stack. Searches whole context stack. varname maybe formatted like this father.child.toy.name */
  asPEEK_Variable GetVariableAtLocalStack(const std::string &varname);

  asPEEK_Variable GetMemberVariable(const std::string &varname, asIScriptObject *obj);

  /** Get a global variable by name*/
  asPEEK_Variable asPEEK::GetVariableByName(const std::vector<std::string> &name, asIScriptModule *mod);

  /** Find child of an object, recursive*/
  asPEEK_Variable GetChildOfObject(std::vector<std::string> &member, asIScriptObject *obj);

  /** Execute script*/
  void ExecuteScript(websocketpp::server::connection_ptr connection, asIScriptModule *mod, const std::string &script);

  void EndDebugging();
public:

  asPEEK(asIScriptEngine *engine, unsigned short port);

  /**
  Add and remove contexts, only added contexts will be debugged. 
  Any remaining context will continue to execute even debugging starts.
  */
  void AddContext(asIScriptContext *ctx);
  /**
  This function is not needed right now. Look at Update function.
  */
  void ContextIsInactive(asIScriptContext *ctx);
  void RemoveContext(asIScriptContext *ctx); // UNTESTED

  /**
  Look for global variable in these Modules.
  Please don't use '?' chracter in module names. 
  Client will lose a lot of its functionality if do so.
  Any other character is fine.
  */
  void AddModule(asIScriptModule *module);
  void RemoveModule(asIScriptModule *module); // UNTESTED

  /**
  Sections to be debugged.
  If a breakpoint is hit or current line is moved to a function which section is not known; client will not be able to view section contents,
  but will see where current line is.
  Failure to supply correct module name will result in you unable to see global variables on mouse hover.
  */
  void AddScriptSection(const std::string &name, const std::string &mod);
  void RemoveScriptSection(const std::string &name); // UNTESTED

  /** Add viewing capability for you application types. Function returns a string and takes a void* */
  void AddSpecialTypeConversion(int type_id, boost::function<std::string(void*)>);

#ifdef asPEEK_HASARRAY
  /** Convert an array to JSON string */
  std::string ArrayToString(const asPEEK_Variable &var);
#endif

  /** Start asPEEK, it will start its own threads */
  void Listen();

  /** Load section*/
  boost::function<void(const std::string &, std::string &)> LoadSectionFunction;

  /** Section name first, data second*/
  boost::function<void(const std::string &, const std::string &)> SaveSectionFunction;

  /**
  Optional. Called when client sends RSTR.
  I use this to recompile scripts, save game state and restart game.
  WARNING!!! since this function will probably call "delete peek;" it creates a deadlock. Like this:
  'delete peek;' will wait for thread to end and thread will wait RestartFunction to end, but RestartFunction is the one called 'delete peek;'. bummer.
  For that reason i am calling this RestartFunction in a worker thread. 
  Beware of the stuff you put in this function, they should be thread safe.
  A potential harm is like this:
  During execution of RestartFunction listening still continues. 
  If asPEEK receieves a SaveFunction call on a section and RestartFunction is compiling that section, section might get corrupted.
  Don't call this if currently debugging context, asPEEK will hang in that case. And crash when continue issue is ordered.
  */
  boost::function<void(void)> RestartFunction;

  /** Send Log Message to all connected clients. */
  void SendLogMessageToAll(const std::string &msg);

  /** Send Message to all. This may be used to expand client functionality*/
  void SendMessageToAll(const std::string &msg);

  /** Optional. Callback when a debugging has started.*/
  boost::function<void(void)> DebuggingStartedFunction;

  /** Optional. Callback when a debugging has ended.*/
  boost::function<void(void)> DebuggingEndedFunction;

  /**
  Pause debugging functionality. All client messages will be ignore, client connections are not closed.
  If there was a current debug session, context will resume its execution.
  */
  void Pause(); // UNTESTED
  /** Resume debugging functionality */
  void Resume(); // UNTESTED

  // TODO, AddBreakpoint RemoveBreakpoint may need mutex protection.
  /**
  Add breakpoint to section.
  Sometimes you may want to scripts to stop before you can open client to set breakpoints.
  Returns false section name is incorrect or there already is a breakpoint in that spot.
  */
  bool AddBreakpoint(const std::string section, int line);

  /**
  Remove a breakpoint.
  Return true if successfully removed.
  */
  bool RemoveBreakpoint(const std::string section, int line);

  /** Call to properly end asPEEK*/
  ~asPEEK();

};

#endif 
