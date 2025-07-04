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


#include <asPEEK.h>
#include <iostream>
#include <sstream>

#ifdef asPEEK_HASARRAY
#include <scriptarray\scriptarray.h>
#endif


asPEEK::asPEEK(asIScriptEngine* engine, unsigned short port)
{
  this->engine = engine;
  this->port = port;

  specialTypeCount = 2;

  contextCount = 0;

  sectionCount = 1;

  LoadSectionFunction = 0;

  SaveSectionFunction = 0;

  RestartFunction = 0;

  debuggingContext = 0;

  debuggingSection = 0;
  debuggingLine = 0;

  isPaused = false;
  debugInterrupt = false;
  endAll = false;

  nextDebugCommand = CONTINUE;
}

void asPEEK::AddContext(asIScriptContext* ctx)
{
  ctx->SetLineCallback(asMETHOD(asPEEK, LineCallback), this, asCALL_THISCALL);

  tracked_context[contextCount] = ctx;
  contextCount++;
}

void asPEEK::StartListening()
{
  try
  {
    listener = boost::shared_ptr<asPEEK_Listener>(new asPEEK_Listener(this));
    websocketpp::server::handler::ptr h(listener);
    websocketpp::server endpoint(h);

    listener->endpoint = &endpoint;

#ifdef asPEEK_DEBUG
    endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
    endpoint.elog().unset_level(websocketpp::log::elevel::ALL);

    endpoint.alog().set_level(websocketpp::log::alevel::CONNECT);
    endpoint.alog().set_level(websocketpp::log::alevel::DISCONNECT);

    endpoint.elog().set_level(websocketpp::log::elevel::RERROR);
    endpoint.elog().set_level(websocketpp::log::elevel::FATAL);

    std::cout << "Starting WebSocket server on port " << port << std::endl;
#endif
    endpoint.listen(port);
  }
  catch(...)
  {
    // whatever.
  }

}

void asPEEK::Listen()
{
  updateThread = boost::thread(&asPEEK::Update, this);
  listenThread = boost::thread(&asPEEK::StartListening, this);
}

asPEEK::SectionData *asPEEK::GetSectionData(const char *section)
{
  asPEEK::SectionData *d = 0;

  // check if that section is added to asPEEK
  {
    boost::unordered_map<std::string, int>::iterator s = sectionIds.find(section);
    if(s == sectionIds.end())
      return 0;
    d = &sections[sectionIds[section]];
  }

  return d;
}

void asPEEK::LineCallback(asIScriptContext* ctx)
{
  while(true)
  {
    boost::mutex::scoped_lock lock(debugging_mutex);
    if(debuggingContext == 0)
      break;

    if(debuggingContext->GetState() != asEXECUTION_ACTIVE)
    {
      lock.unlock();
      EndDebugging();
      return;
    }

    if(debuggingContext == ctx)
      break;

    lock.unlock();
    while(true)
    {
      boost::mutex::scoped_lock lock2(debugging_mutex);
      if(!debuggingContext)
        break;
      lock2.unlock(); // dont wait 10 ms to unlock this
      // TODO, use mutex and conditional to wait
      // mutex here is known to cause deadlock with asio
      boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }

  }
  const char *section;
  int line = ctx->GetLineNumber(0, 0, &section);

  if(!section)
    return;




  // if command is continue (default) check breakpoint 
  if(nextDebugCommand == CONTINUE)
  {
    SectionData *d = GetSectionData(section);
    if(!d)
      return;

    if(d->breakpoints.count(line)) // hit a breakpoint
    {
      if(DebuggingStartedFunction)
        DebuggingStartedFunction();

      boost::mutex::scoped_lock lock(debugging_mutex);
      debuggingLine = line;
      debuggingSection = d->id;
      // this thread is now being debugged
      debuggingContext = ctx;
      lock.unlock();

      Debug(ctx, line, d);
    }
    else
    {
      boost::mutex::scoped_lock lock(debugging_mutex);
      if(ctx == debuggingContext)
      {
        lock.unlock();
        EndDebugging();
      }

    }

  }
  else
  { int ssize = ctx->GetCallstackSize();
  switch (nextDebugCommand)
  {
  case asPEEK::STEPOUT:
    if(ctx->GetCallstackSize() == 1)
    {
      nextDebugCommand = CONTINUE;

      boost::mutex::scoped_lock lock(debugging_mutex);
      if(ctx == debuggingContext)
      {
        lock.unlock();
        EndDebugging();
      }

    }
    else if(ctx->GetCallstackSize() < debuggingStackLevel )
    {
      SectionData *d = GetSectionData(section);
      if(!d)
        return;

      boost::mutex::scoped_lock lock(debugging_mutex);
      debuggingLine = line;
      debuggingSection = d->id;
      debuggingContext = ctx;
      lock.unlock();
      Debug(ctx, line, d);
    }
    else
    {
      SectionData *d = GetSectionData(section);
      if(!d)
        return;
      if(d->breakpoints.count(line)) // hit a breakpoint
      {
        boost::mutex::scoped_lock lock(debugging_mutex);
        debuggingLine = line;
        debuggingSection = d->id;
        // this thread is now being debugged
        debuggingContext = ctx;
        lock.unlock();
        Debug(ctx, line, d);
      }
    }
    break;
  case asPEEK::STEPIN:
    {
      SectionData *d = GetSectionData(section);
      if(!d)
        return;
      boost::mutex::scoped_lock lock(debugging_mutex);
      debuggingLine = line;
      debuggingSection = d->id;
      debuggingContext = ctx;
      lock.unlock();
      Debug(ctx, line, d);
    }
    break;
  case asPEEK::STEPOVER:
    if(ctx->GetCallstackSize() <= debuggingStackLevel )
    {
      SectionData *d = GetSectionData(section);
      if(!d)
        return;
      boost::mutex::scoped_lock lock(debugging_mutex);
      debuggingLine = line;
      debuggingSection = d->id;
      debuggingContext = ctx;
      lock.unlock();
      Debug(ctx, line, d);
    }
    else
    {
      SectionData *d = GetSectionData(section);
      if(!d)
        return;
      if(d->breakpoints.count(line)) // hit a breakpoint
      {
        boost::mutex::scoped_lock lock(debugging_mutex);
        debuggingLine = line;
        debuggingSection = d->id;
        // this thread is now being debugged
        debuggingContext = ctx;
        lock.unlock();
        Debug(ctx, line, d);
      }
    }
    break;
  default:
    break;
  }
  }


}

void asPEEK::Debug(asIScriptContext *ctx, int line, SectionData *section)
{


  for(std::map<websocketpp::server::connection_ptr, ClientData>::iterator it = listener->open_connections.begin(); it != listener->open_connections.end(); ++it)
  {
    if(ctx->GetCallstackSize() > 0)
    {
      SendThisObject(it->first, ctx);
      SendLocalVariables(it->first, ctx);
      SendCurrentLine(it->first);
      SendStack(it->first, ctx);
    }
  }

  while (!debugInterrupt) // wait commands from client
  {
    // TODO, use mutex and conditional to wait.
    // another likely deadlock if mutex is used 
    boost::this_thread::sleep(boost::posix_time::milliseconds(1));

    std::string command = "";
    boost::mutex::scoped_lock lock(debugCommands_mutex);
    if(!debugCommands.empty())
    {
      command = debugCommands[0];
      debugCommands.erase(debugCommands.begin());
    }
    lock.unlock();

    if(command != "")
    {
      if(command == "STOV")
      {
        nextDebugCommand = STEPOVER;
        debuggingStackLevel = ctx->GetCallstackSize();
        break; // break out of wait cycle
      }
      else if( command == "CONT")
      {
        nextDebugCommand = CONTINUE;
        break; // break out of wait cycle
      }
      else if(command == "STIN")
      {
        nextDebugCommand = STEPIN;
        debuggingStackLevel = ctx->GetCallstackSize();
        break; // break out of wait cycle
      }
      else if(command == "STOU")
      {
        nextDebugCommand = STEPOUT;
        debuggingStackLevel = ctx->GetCallstackSize();
        break; // break out of wait cycle
      }
    }

  }
}

void asPEEK::HandleMessage(asPEEK_Message &msg)
{
  if(isPaused)
    return;

  std::string command = msg.messageContents.substr(0,4);
  if (command == "ASGN")
  {

  }
  else if(command == "REQV")
  {
    std::vector<std::string> words;
    std::stringstream ss(msg.messageContents);
    std::string item;
    while (std::getline(ss, item, ' '))
      words.push_back(item);

    if(words.size() == 3)
    {
      int id;
      if(!asPEEK_UTILS::ToInt(words[1], id))
        return;

      if(!sections.count(id))
        return;

      std::string modName = sections[id].mod;
      asIScriptModule *module =  engine->GetModule(sections[id].mod.c_str());

      if(module)
      {
        if(tracked_modules.count(module))
        {
          SendVariable(words[2], module, msg.client);
        }
      }    
    }
  }
  else if(command == "GETV")
  {
    std::vector<std::string> words;
    std::stringstream ss(msg.messageContents);
    std::string item;
    while (std::getline(ss, item, '?')) // GETV parameters are seperated by ?
      words.push_back(item);

    // has only message type.
    if(words.size() < 3)
    {
      //msg.client->send("ERRO Missing variable name");
      listener->Send(msg.client, "ERRO Missing variable name");
      return;
    }

    asPEEK_Variable v = GetGlobalVariable(words);

    if(v.isValid())
    {
      if(v.tid & asTYPEID_SCRIPTOBJECT)
      {
        std::set<void*> previous;

        if(v.tid & asTYPEID_OBJHANDLE)
          previous.insert(*(void**) v.address);
        else
          previous.insert(v.address);

        std::stringstream ss2;
        ss2 << "VARV {";
        ss2 << "\"mod\":\"" << words[1] << "\",";
        ss2 << "\"name\":\"" << words[2] << "\", \"val\":";
        listener->Send(msg.client, ss2.str() + ToString(v, &previous) + "}");
      }
      else
      {
        std::stringstream ss2;
        ss2 << "VARV {";
        ss2 << "\"mod\":\"" << words[1] << "\",";
        ss2 << "\"name\":\"" << words[2] << "\", \"val\":";
        listener->Send(msg.client, ss2.str() + ToString(v) + "}");
      }
    }
    else
    {
      listener->Send(msg.client, "LOGW Could not find variable "+ words[2]);
    }

  }
  else if(command == "GETF")
  {
    std::vector<std::string> words;
    std::stringstream ss(msg.messageContents);
    std::string item;
    while (std::getline(ss, item, ' '))
      words.push_back(item);

    if(words.size() == 2)
      SendFile(words, msg.client);

  }
  else if(command == "BRKS")
  {
    std::vector<std::string> words;
    std::stringstream ss(msg.messageContents);
    std::string item;
    while (std::getline(ss, item, ' '))
      words.push_back(item);

    if(words.size() == 3)
    {
      SetBreakpoint(words);
    }
  }
  else if(command == "BRKR")
  {
    std::vector<std::string> words;
    std::stringstream ss(msg.messageContents);
    std::string item;
    while (std::getline(ss, item, ' '))
      words.push_back(item);

    if(words.size() == 3)
    {
      RemoveBreakpoint(words);
    }
  }
  else if(command == "STOV" || command == "STIN" || command == "CONT" || command == "STOU")
  {
    std::vector<std::string> words;
    std::stringstream ss(msg.messageContents);
    std::string item;
    while (std::getline(ss, item, ' '))
      words.push_back(item);

    boost::mutex::scoped_lock lock(debugging_mutex);
    if(debuggingContext)
    {   
      lock.unlock();
      boost::mutex::scoped_lock lock2(debugCommands_mutex);
      debugCommands.push_back(words[0]);
    }
  }
  else if(command == "SAVE")
  {
    // strip command and script section
    int spaceCount = 0;
    unsigned int i = 0;
    unsigned size = msg.messageContents.size();
    for(; i < size; ++i)
    {
      if(msg.messageContents[i] == ' ')
        spaceCount++;

      if(spaceCount == 2)
        break;
    }

    int sectionId;
    // get section id
    if(!asPEEK_UTILS::ToInt(msg.messageContents.substr(5, i-5), sectionId))
    {
      listener->Send(msg.client, "LOGE Unable to parse section id: "+ msg.messageContents.substr(5, i-5));
      return;
    }

    if(!sections.count(sectionId))
    {
      listener->Send(msg.client, "LOGE Section does not exist id: "+ sectionId);
      return;
    }

    msg.messageContents.erase(0, i+1);
    SectionData *d = &sections[sectionId];
    SaveSectionFunction(d->name, msg.messageContents);

    std::stringstream ss;
    ss << "SECM ";
    ss << sectionId;
    listener->SendToAll(ss.str());
  }
  else if(command == "EXCT")
  {
    if(tracked_modules.empty())
      return;

    // strip command and script section
    unsigned int moduleEnd = 0;
    unsigned int i = 0;
    unsigned size = msg.messageContents.size();
    for(; i < size; ++i)
    {
      if(msg.messageContents[i] == '?')
      {
        moduleEnd = i;
      }
    }

    if(moduleEnd == 0)
      return;

    std::string moduleName;
    // get module id
    moduleName  = msg.messageContents.substr(5, moduleEnd-5);

    asIScriptModule *module = 0;
    if(moduleName == "Any")
    {
      module = *tracked_modules.begin();
    }
    else
    {
      for(std::set<asIScriptModule*>::iterator it = tracked_modules.begin(); it != tracked_modules.end(); ++it)
      {
        if((*it)->GetName() == moduleName)
        {
          module = *it;
        }
      }
    }
    if(msg.messageContents.size() > moduleEnd+1)
      ExecuteScript(msg.client, module, msg.messageContents.substr(moduleEnd+1, msg.messageContents.size()));
  }
  else if(command == "RSTR")
  {
    if(RestartFunction)
    {
      boost::mutex::scoped_lock lock(debugging_mutex);
      if(!debuggingContext)
      {
        lock.unlock();
        boost::thread worker(RestartFunction);
      }
      else
      {
        lock.unlock();
        listener->Send(msg.client, "LOGE Currently debugging, can not restart.");
      }
    }
    else
    {
      listener->Send(msg.client, "LOGE Restart function is not defined");
    }
  }
}

void asPEEK::Update()
{

  while (!endAll)
  {
    boost::unique_lock<boost::mutex> lock(receivedMessages_mutex);

    // condition variable disabled for now
    /**/
    while(receivedMessages.empty())
    {
      receivedMessages_condition.wait(lock);
    }

    HandleMessage(receivedMessages.front());
    receivedMessages.pop_front();

  }

}

void asPEEK::RemoveContext(asIScriptContext* ctx)
{
  for(boost::unordered_map<int, asIScriptContext*>::iterator it = tracked_context.begin(); it != tracked_context.end() ; ++it)
  {
    if(it->second == ctx)
    {
      ContextIsInactive(ctx);

      tracked_context.erase(it);
      break;
    }
  }

}

void asPEEK::AddModule(asIScriptModule* module)
{
  tracked_modules.insert(module);
}

void asPEEK::RemoveModule(asIScriptModule* module)
{
  tracked_modules.erase(module);
}

asPEEK::~asPEEK()
{

  endAll = true;
  receivedMessages.push_back(asPEEK_Message()); // trick update thread to move on


  receivedMessages_condition.notify_one();

  updateThread.join();

  if(listener)
    listener->endpoint->stop();

  listenThread.join();

}

void asPEEK_Listener::Send(connection_ptr con, const std::string &message)
{
  con->send(message);
}

void asPEEK_Listener::SendToAll(const std::string &msg)
{
  for(std::map<connection_ptr, ClientData>::iterator it = open_connections.begin(); it != open_connections.end() ; ++it)
  {
    it->first->send(msg);
  }
}

void asPEEK_Listener::on_message(connection_ptr con, message_ptr msg)
{
  // release lock on connection, this thread (listen thread) does not use this connection anyway.
  // if we don't release it causes a deadlock.
  // like this: this thread hold con->m_lock and wants peek->receivedMessages_mutex. Update thread holds peek->receivedMessages_mutex and wants con->m_lock because it sends stuff to client with that connection
  // this thread does not send anything, ever. so we just release it here.
  con->m_lock.unlock();

  asPEEK_Message message;
  message.client = con;
  message.messageContents = msg->get_payload();

  boost::unique_lock<boost::mutex> lock(peek->receivedMessages_mutex);
  peek->receivedMessages.push_back(message);

  // condition variable disables for now, read about it in Update function
  peek->receivedMessages_condition.notify_one();
}

void asPEEK::SendVariable(const std::string &varname, asIScriptModule *mod, websocketpp::server::connection_ptr connection)
{

  std::vector<std::string> words;
  std::stringstream ss(varname);
  std::string item;
  while (std::getline(ss, item, '.'))
    words.push_back(item);

  if(words.empty())
    return;

  // this gets the most parent object
  asPEEK_Variable var = GetVariableByName(words, mod);

  if(!var.address)
    return;

  if(words.size() > 1)
  {
    // should be an object type or else client sent a fucked up string
    if(var.tid & asTYPEID_SCRIPTOBJECT)
    {
      // remove most parent
      words.erase(words.begin());
      if(var.tid & asTYPEID_OBJHANDLE)
      {
        var = GetChildOfObject(words, *(asIScriptObject**) var.address);
      }
      else
      {
        var = GetChildOfObject(words, (asIScriptObject*) var.address);
      }

      // send value
    }
  }

  {
    std::stringstream ss;
    ss.str("");
    ss << "REQV ";
    ss << varname;
    ss << " ";

    if(var.tid & asTYPEID_SCRIPTOBJECT)
    {
      std::set<void*> previous;

      if(var.tid & asTYPEID_OBJHANDLE)
        previous.insert(*(void**)var.address);
      else
        previous.insert(var.address);

      ss << ToString(var, &previous);
    }
    else
      ss << ToString(var);

    listener->Send(connection, ss.str());

  }
}

asPEEK_Variable asPEEK::GetMemberVariable(const std::string &varname, asIScriptObject *obj)
{
  unsigned int mcount = obj->GetPropertyCount();

  for(unsigned int i= 0; i < mcount; ++i)
  {
    if(obj->GetPropertyName(i) == varname)
    {
      return asPEEK_Variable( obj->GetAddressOfProperty(i), obj->GetPropertyTypeId(i) ); 
    }
  }

  return asPEEK_Variable();
}

asPEEK_Variable asPEEK::GetChildOfObject(std::vector<std::string> &member, asIScriptObject *obj)
{
  asPEEK_Variable v;
  for(unsigned int i = 0; i < member.size(); ++i)
  {
    v = GetMemberVariable(member[i], obj);

    if(!v.address)
      return asPEEK_Variable(); // we failed sorry :(

    if(v.tid & asTYPEID_SCRIPTOBJECT)
    {
      if(v.tid & asTYPEID_OBJHANDLE)
        obj = *(asIScriptObject**)v.address;
      else
        obj = (asIScriptObject*)v.address;
    }
    else // only acceptable if loop is over
    {
      if(i == member.size() -1)
        return v;
      else
        return asPEEK_Variable();
    }
  }

  return v;
}

asPEEK_Variable asPEEK::GetVariableAtLocalStack(const std::string &varname)
{
  boost::mutex::scoped_lock lock(debugging_mutex);
  // only if we current are stopped and debugging
  if(!debuggingContext)
    return asPEEK_Variable();

  unsigned int ssize = debuggingContext->GetCallstackSize();

  for(unsigned int i = 0; i < ssize; ++i)
  {
    int vcount = debuggingContext->GetVarCount(i);

    for(int j = 0; j < vcount ; ++j)
    {
      if(varname == debuggingContext->GetVarName(j, i))
      {
        return asPEEK_Variable(debuggingContext->GetAddressOfVar(j, i), debuggingContext->GetVarTypeId(j, i));
      }
    }
  }

  return asPEEK_Variable();
}

asPEEK_Variable asPEEK::GetVariableByName(const std::vector<std::string> &name, asIScriptModule *mod)
{

  // we should first determine if this object is local/global/member

  asPEEK_Variable var;
  std::string ns = ""; // namespace

  int pos = name[0].find("::");

  if(pos != std::string::npos)
  {
    ns = name[0].substr(0, pos);

    if(ns == "") // user might want global scope ::Myint
      ns = "::";
  }

  // only if it doesn't have namespace. only global variables have namespaces
  if(ns == "")
  {
    var = GetVariableAtLocalStack(name[0]);

    if(var.address)
      return var;


    boost::mutex::scoped_lock lock(debugging_mutex);
    if(debuggingContext)
    {
      if(debuggingContext->GetThisPointer())
      {
        asIScriptObject *t = (asIScriptObject*) debuggingContext->GetThisPointer();
        lock.unlock();
        var = GetMemberVariable(name[0], t);

        if(var.address)
          return var;
      }
    }
  }

  std::string modName = mod->GetName(); // look in all modules
  if(ns == "")
    var = GetGlobalVariable(name[0], modName, ns); // see if this is a global variable
  else
    var = GetGlobalVariable(name[0].substr(pos+2, name[0].length()), modName, ns); // see if this is a global variable

  if(var.address)
    return var;

  return asPEEK_Variable();
}

void asPEEK::SendLocalVariables(websocketpp::server::connection_ptr con, asIScriptContext *ctx)
{
  std::stringstream ss;

  ss << "LOCV [";

  unsigned int ssize = ctx->GetCallstackSize();

  for(unsigned int i = 0; i < ssize; ++i)
  {
    ss << "{";
    int varCount = ctx->GetVarCount(i);

    for(int i2 = 0; i2 < varCount; ++i2)
    {
      void *addr = ctx->GetAddressOfVar(i2, i);

      int tid = ctx->GetVarTypeId(i2, i);
      ss << "\"";
      ss << ctx->GetVarName(i2, i);
      ss << "\"";
      ss << ":";

      if(!addr)
      {
        ss << "\"undefined\"";
      }
      else
      {
        if(tid & asTYPEID_SCRIPTOBJECT)
        {
          std::set<void*> previous;
          if(tid & asTYPEID_OBJHANDLE)
            previous.insert(*(void**)addr);
          else
            previous.insert(addr);

          ss << ToString(asPEEK_Variable(addr, tid), &previous);
        }
        else
          ss << ToString(asPEEK_Variable(addr, tid));
      }

      if(i2 != (varCount - 1))
        ss << ",";

    }

    ss << "}";

    if(i != ssize - 1)
      ss << ",";

  }

  ss << "]";

  // con->send(ss.str());
  listener->Send(con, ss.str());
}

void asPEEK::ExecuteScript(websocketpp::server::connection_ptr connection, asIScriptModule *mod, const std::string &script)
{
  if(mod)
  {
    std::string funcCode = "void asPEEK_Execute() {\n";
    funcCode += script;
    funcCode += "\n;}";

    // Compile the function that can be executed
    asIScriptFunction *func = 0;
    int r = mod->CompileFunction("ExecuteString", funcCode.c_str(), -1, 0, &func);
    if( r < 0 )
    {
      listener->Send(connection, "EXCT Was not able to compile");
      return;
    }

    // If no context was provided, request a new one from the engine
    asIScriptContext *execCtx = engine->CreateContext();
    r = execCtx->Prepare(func);
    if( r < 0 )
    {
      func->Release();
      execCtx->Release();
      listener->Send(connection, "EXCT Execution failed.");
      return;
    }

    // Execute the function
    r = execCtx->Execute();

    // Clean up
    func->Release();
    execCtx->Release();

    listener->Send(connection, "EXCT Execution Successful.");
  }
}

void asPEEK::SendStack(websocketpp::server::connection_ptr con, asIScriptContext *ctx)
{
  unsigned int ssize = ctx->GetCallstackSize();

  std::stringstream ss;
  ss << "STCK [";
  for(unsigned int i = 0; i < ssize; ++i)
  {

    const char *sectionName;
    int column;
    int lineNumber = ctx->GetLineNumber(i, &column, &sectionName);

    if(!sectionName)
      continue;

    if(!sectionIds.count(sectionName))
      continue;

    ss << "{";

    ss << "\"l\":" << lineNumber << ",";
    ss << "\"c\":" << column << ",";
    ss << "\"s\":" << sectionIds[sectionName] << ",";
    ss << "\"f\":\"" << ctx->GetFunction(i)->GetDeclaration()<< "\"";
    ss << "}";

    if(i != ssize -1)
    {
      ss << ",";
    }
  }
  ss << "]";

  listener->Send(con, ss.str());

}

void asPEEK::SendThisObject(websocketpp::server::connection_ptr con, asIScriptContext *ctx)
{
  asIScriptObject *obj = (asIScriptObject*) ctx->GetThisPointer();

  if(obj)
  {
    std::stringstream ss;
    ss << "THIS ";
    ss << ctx->GetCallstackSize();
    ss << " {";
    unsigned int pcount = obj->GetPropertyCount();

    for(unsigned int i = 0; i< pcount; ++i)
    {
      int tid = obj->GetPropertyTypeId(i);
      ss << "\""<< obj->GetPropertyName(i) << "\"";
      ss << ":";

      void *v = obj->GetAddressOfProperty(i);
      if(!v)
      {
        ss << "{\"addr\":\"null\"}";
      }
      else
      {
        if(tid & asTYPEID_SCRIPTOBJECT)
        {
          std::set<void*> previous;

          previous.insert(obj);

          if(tid & asTYPEID_OBJHANDLE)
            previous.insert(*(void**)v);
          else
            previous.insert(v);

          ss << ToString(asPEEK_Variable(v, tid), &previous);
        }
        else
        {
          ss << ToString(asPEEK_Variable(v, tid));
        }
      }
      if(i < (pcount-1))
        ss<<",";
    }

    ss <<"}";

    //con->send(ss.str());
    listener->Send(con, ss.str());
  }
}

asPEEK_Variable asPEEK::GetGlobalVariable(std::vector<std::string> &msg)
{
  // this part slices var name. player.location.x will be [player, location, x]
  std::vector<std::string> words;
  std::stringstream ss(msg[2]);
  std::string item;
  while (std::getline(ss, item, '.'))
    words.push_back(item);

  std::string ns = ""; // namespace
  int pos = words[0].find("::");
  if(pos != std::string::npos)
  {
    ns = words[0].substr(0, pos);

    if(ns == "") // user might want global scope ::Myint
      ns = "::";
  }

  if(words.empty()) // empty string
  {
    return asPEEK_Variable();
  }
  else if(words.size() == 1) // then this is global variable
  {
    if(ns == "")
      return GetGlobalVariable(words[0], msg[1], ns);
    else
      return GetGlobalVariable(words[0].substr(pos+2, words[0].length()), msg[1], ns);
  }
  else // this variable is part of an object
  {
    asPEEK_Variable parent;
    if(ns == "")
      parent = GetGlobalVariable(words[0], msg[1], ns);
    else
      parent = GetGlobalVariable(words[0].substr(pos+2, words[0].length()), msg[1], ns);

    if(parent.isValid())
    {
      if(!parent.isCPPObject()) // if it is a c++ object we cant loop over its members.
      {
        // go up the tree until we find address of the highest level object
        for(unsigned int i = 1; i < words.size(); ++i)
        {
          parent = GetMemberVariable(parent, words[i]);
        }
      }
      else
      {
        // if it is a c++ object cant do anything.
        return asPEEK_Variable();
      }

    }

    return parent;
  }

}

asPEEK_Variable asPEEK::GetMemberVariable(const asPEEK_Variable &parent, const std::string &name)
{

  asIScriptObject *obj = (asIScriptObject*) parent.address;

  unsigned int pcount = obj->GetPropertyCount();

  for(unsigned int i = 0; i < pcount; ++i)
  {
    if(obj->GetPropertyName(i) == name)
    {
      int tid = obj->GetPropertyTypeId(i);

      // if this object is an handle than get its address first
      if(tid & asTYPEID_OBJHANDLE)
        return asPEEK_Variable(*(void**)obj->GetAddressOfProperty(i), tid);
      else
        return asPEEK_Variable(obj->GetAddressOfProperty(i), tid);
    }
  }

  return asPEEK_Variable();
}

asPEEK_Variable asPEEK::GetGlobalVariable(const std::string &msg, std::string &modName, const std::string &ns)
{
  if(modName == "*")
  {
    for(std::set<asIScriptModule*>::iterator it = tracked_modules.begin(); it != tracked_modules.end(); ++it)
    {

      if(ns != "")
        (*it)->SetDefaultNamespace(ns.c_str());

      int idx = (*it)->GetGlobalVarIndexByName(msg.c_str());

      if(idx >= 0)
      {
        asPEEK_Variable v;
        (*it)->GetGlobalVar(idx, 0, 0, &v.tid, 0);
        v.address = (*it)->GetAddressOfGlobalVar(idx);

        modName = (*it)->GetName();

        if(ns != "")
          (*it)->SetDefaultNamespace("");
        return v;
      }
      /**/
      if(ns != "")
        (*it)->SetDefaultNamespace("");

    }
  }
  else
  {

    asIScriptModule *mod = engine->GetModule(modName.c_str());

    if(ns != "")
      mod->SetDefaultNamespace(ns.c_str());

    if(mod)
    {
      int idx = mod->GetGlobalVarIndexByName(msg.c_str());

      if(idx >= 0)
      {
        asPEEK_Variable v;
        mod->GetGlobalVar(idx, 0, 0, &v.tid, 0);
        v.address = mod->GetAddressOfGlobalVar(idx);

        if(ns != "")
          mod->SetDefaultNamespace("");

        return v;
      }
    }

    if(ns != "")
      mod->SetDefaultNamespace("");
  }

  return asPEEK_Variable();

}

std::string asPEEK::PrimitiveToString(const asPEEK_Variable &var)
{
  if(var.tid > asTYPEID_DOUBLE) // this is enumeration
  {
    std::stringstream ss;
    ss << "\"";
    ss << *(asUINT*) var.address;

    bool found = false;
    int ecount = engine->GetEnumCount();
    for(int i = 0; i < ecount; ++i)
    {
      int tid;
      engine->GetEnumByIndex(i, &tid);
      if(var.tid == tid)
      {
        found  = true;
        const char *c = engine->GetEnumValueByIndex(tid, i, 0);
        if(c)
          ss << " (" << c << ")";
      }
    }

    //TODO, this part needs improvent, we are looping over all modules' enums to find a name.
    // would be good if we knew beforehand which module we look it in
    if(!found)
    {
      for(std::set<asIScriptModule*>::iterator it = tracked_modules.begin(); it != tracked_modules.end(); ++it)
      {
        ecount = (*it)->GetEnumCount();

        for(int i = 0; i < ecount; ++i)
        {
          int tid;
          (*it)->GetEnumByIndex(i, &tid);
          if(var.tid == tid)
          {
            const char *c = (*it)->GetEnumValueByIndex(tid, i, 0);
            if(c)
              ss << " (" << c << ")";
          }
        }
      }
    }

    ss << "\"";
    return ss.str();
  }
  else
  {
    switch (var.tid)
    {
    case asTYPEID_BOOL:
      if(*(bool*)var.address)
        return "true";
      else
        return "false";
    case asTYPEID_FLOAT:
      {
        std::stringstream ss;
        ss << *(float*)var.address;
        return ss.str();
      }
    case asTYPEID_DOUBLE:
      {
        std::stringstream ss;
        ss << *(double*)var.address;
        return ss.str();
      }
    case asTYPEID_INT8:
      {
        std::stringstream ss;
        ss << *(char*)var.address;
        return ss.str();
      }
    case asTYPEID_INT16:
      {
        std::stringstream ss;
        ss << *(short*)var.address;
        return ss.str();
      }
    case asTYPEID_INT32:
      {
        std::stringstream ss;
        ss << *(int*)var.address;
        return ss.str();
      }
    case asTYPEID_INT64:
      {
        std::stringstream ss;
        ss << *(long*)var.address;
        return ss.str();
      }
    case asTYPEID_UINT8:
      {
        std::stringstream ss;
        ss << *(unsigned char*)var.address;
        return ss.str();
      }
    case asTYPEID_UINT16:
      {
        std::stringstream ss;
        ss << *(unsigned short*)var.address;
        return ss.str();
      }
    case asTYPEID_UINT32:
      {
        std::stringstream ss;
        ss << *(unsigned int*)var.address;
        return ss.str();
      }
    case asTYPEID_UINT64:
      {
        std::stringstream ss;
        ss << *(unsigned long*)var.address;
        return ss.str();
      }
    default:
      return "\"Primitive?\"";
      break;
    }
  }

}

void asPEEK::ScriptObjectToString(asIScriptObject *obj, std::stringstream &ss, std::set<void*> *previous)
{
  if(!obj)
  { 
    ss << "{\"addr\":\"null\"}";
    return;
  }
  ss << "{"; // every object starts with curly open
  ss << "\"_\":\""<< obj->GetObjectType()->GetName() <<"\"";

  unsigned int pcount = obj->GetPropertyCount();

  if(pcount)
    ss << ",";

  for(unsigned int i = 0; i < pcount; ++i)
  {

    ss << "\"" << obj->GetPropertyName(i) <<"\":";
    int tid = obj->GetPropertyTypeId(i);

    if(tid & asTYPEID_SCRIPTOBJECT)
    {
      if(previous)
      {
        void *v = 0;
        void *t = obj->GetAddressOfProperty(i);
        if(tid & asTYPEID_OBJHANDLE)
          v = *(void**)obj->GetAddressOfProperty(i);
        else
          v = obj->GetAddressOfProperty(i);

        if(!v)
        {
          ss << "{\"addr\":\"null\"}";

          if(i != (pcount - 1) )
            ss << ","; // comma between each member, but not to last one

          continue;
        }
        else
        {
          if(previous->count(v))
          {		
            ss << "{\"[REPEAT]\":\"0x";
            ss << v << "\"}";

            if(i != (pcount - 1) )
              ss << ","; // comma between each member, but not to last one 

            continue;
          }
          else
          {
            previous->insert(v);  
          }
        }
      }
    }

    asPEEK_Variable v(obj->GetAddressOfProperty(i), tid);

    ss << ToString(v, previous);

    if(i != (pcount - 1) )
      ss << ","; // comma between each member, but not to last one 

  }

  ss << "}"; // every object ends with curly close
}

std::string asPEEK::ToString(const asPEEK_Variable &var, std::set<void*> *previous)
{

  if(var.tid & asTYPEID_SCRIPTOBJECT)
  {
    if(var.tid & asTYPEID_OBJHANDLE)
    {
      std::stringstream ss;
      ScriptObjectToString(*(asIScriptObject**)(var.address), ss, previous);
      return ss.str();
    }
    else
    {
      std::stringstream ss;
      ScriptObjectToString((asIScriptObject*)(var.address), ss, previous);
      return ss.str();
    }
  }
  else if(var.tid & asTYPEID_TEMPLATE)
  {
#ifdef asPEEK_HASARRAY
    std::string s = engine->GetTypeDeclaration(var.tid);
    if(s.substr(0, 5) == "array")
      return ArrayToString(var);
    else
      return "{}";
#else
    std::stringstream ss;
    ss << "{\"addr\":\"0x";
    if(var.tid & asTYPEID_OBJHANDLE)
      ss << *(void**)(var.address);
    else
      ss << (var.address);

    ss<< "\"}";
    return ss.str();
#endif
  }
  else if(var.tid & asTYPEID_APPOBJECT)
  {
    int tid = var.tid;
    bool isItHandle = false;

    if(var.tid & asTYPEID_OBJHANDLE) // it may be a handle
    {
      tid |= asTYPEID_OBJHANDLE; // strip handle part
      isItHandle = true;
    }

    if(specialTypes.count(tid))
    {
      std::string s = "";
      if(isItHandle)
      {
        s += conversionMethods[specialTypes[tid]](*(void**)var.address);
      }
      else
      {
        s += conversionMethods[specialTypes[tid]](var.address);
      }
      return s;
    }
    else
    {
      // just print its address if we don't know how to convert to JSON
      std::stringstream ss;

      ss << "{\"addr\":\"0x";
      if(isItHandle)
        ss << *(void**)(var.address);
      else
        ss << (var.address);

      ss<< "\"}";

      return ss.str();
    }
  }
  else
    return PrimitiveToString(var);

}

void asPEEK::AddSpecialTypeConversion(int type_id, boost::function<std::string(void*)> fPtr)
{
  specialTypes[type_id] = specialTypeCount;
  conversionMethods[specialTypeCount] = fPtr;
  specialTypeCount++;
}
#ifdef asPEEK_HASARRAY
std::string asPEEK::ArrayToString(const asPEEK_Variable &var)
{
  std::stringstream ss;

  CScriptArray *arr = (CScriptArray*) var.address;

  int subTypeId = arr->GetElementTypeId();

  ss << "[";

  asIObjectType* t = engine->GetObjectTypeById(arr->GetArrayTypeId());

  ss << "{\"_\":\""<< t->GetName() <<"\"}";

  unsigned int size = arr->GetSize();

  if(size > 0)
    ss << ",";

  for(unsigned int i = 0; i < size; ++i)
  {

    asPEEK_Variable v(arr->At(i), subTypeId);

    if(subTypeId & asTYPEID_SCRIPTOBJECT)
    {
      std::set<void*> previous;

      if(subTypeId & asTYPEID_OBJHANDLE)
        previous.insert(*(void**)arr->At(i));
      else
        previous.insert(arr->At(i));

      ss << ToString(v, &previous);
    }
    else
      ss << ToString(v);

    if(i != (size-1) )
      ss << ",";

  }
  ss << "]";

  return ss.str();
}
#endif
void asPEEK_Listener::on_open(connection_ptr connection)
{
  //connection->send("WELCOME!");

  std::stringstream endpoint;
  endpoint << connection;
  open_connections[connection] = ClientData(endpoint.str());

  peek->NewClient(connection);
}


void asPEEK_Listener::on_close(connection_ptr con) 
{
  std::map<connection_ptr, ClientData>::iterator it = open_connections.find(con);

  if (it == open_connections.end()) 
  {
    // this client has already disconnected, we can ignore this.
    // this happens during certain types of disconnect where there is a
    // deliberate "soft" disconnection preceding the "hard" socket read
    // fail or disconnect ack message.
    return;
  }

  open_connections.erase(it);

}

void asPEEK::SendModuleNames(websocketpp::server::connection_ptr con)
{
  std::stringstream ss;
  ss << "MODL [";
  for(std::set<asIScriptModule*>::iterator it = tracked_modules.begin(); it != tracked_modules.end();)
  {
    ss << "\"" << (*it)->GetName() << "\"";

    ++it;
    if(it != tracked_modules.end())
      ss << ",";
  }
  ss << "]";

  //con->send(modules);
  listener->Send(con, ss.str());
}

void asPEEK::SendSectionNames(websocketpp::server::connection_ptr con)
{
  std::stringstream ss;
  ss << "SCLS [";

  for(boost::unordered_map<int, SectionData>::iterator it = sections.begin(); it != sections.end(); )
  {  
    ss << "{";
    ss << "\"id\":";
    ss << it->first;
    ss << ",\"name\":\"";
    ss << it->second.name << "\",";
    ss << "\"mod\":\"" << it->second.mod << "\"";
    ss << "}";

    ++it;
    if(it != sections.end())
      ss << ",";
  }

  ss << "]";
  listener->Send(con, ss.str());

  for(boost::unordered_map<int, SectionData>::iterator it = sections.begin(); it != sections.end(); ++it)
  {
    for(std::set<int>::iterator it2 = it->second.breakpoints.begin(); it2 != it->second.breakpoints.end(); ++it2)
    {
      std::stringstream ss2;
      ss2 << "BSET ";
      ss2 << it->first;
      ss2 << " ";
      ss2 << *(it2);
      listener->Send(con, ss2.str());
    }
  }

}

void asPEEK::SendContexts(websocketpp::server::connection_ptr con)
{
  std::stringstream ss;
  ss << "CTXL";

  for(boost::unordered_map<int, asIScriptContext*>::iterator it = tracked_context.begin(); it != tracked_context.end(); ++it)
  {
    ss << " ";
    ss << it->first;

  }

  //con->send(ss.str());
  listener->Send(con, ss.str());
}

void asPEEK::AddScriptSection(const std::string &name, const std::string &mod)
{
  SectionData d;
  d.id = sectionCount;

  d.name = name;

  // replace backwards slashes with forward ones, javascript thinks they are escape squences
  // you may remove this at your own risk
  std::replace(d.name.begin(), d.name.end(), '\\', '/');

  d.mod = mod;
  sections[sectionCount] = d;

  sectionIds[name] = sectionCount;

  sectionCount++;
}
void asPEEK::RemoveScriptSection(const std::string &name)
{
  if(sectionIds.count(name))
  {
    int id = sectionIds[name];
    sectionIds.erase(name);
    sections.erase(id);
  }


}

void asPEEK::SendFile(const std::vector<std::string> &words, websocketpp::server::connection_ptr con)
{
  if(LoadSectionFunction)
  {
    std::string fileContents = "";

    std::stringstream ss;
    int secId;
    ss << words[1];
    ss >> secId;

    if(secId !=0)
    {
      if(sections.count(secId))
      {
        LoadSectionFunction(sections[secId].name, fileContents);

        std::string msg = "FILE ";
        msg += words[1];
        msg += " ";
        msg += fileContents;

        listener->Send(con, msg);
      }
    }

  }
  else
  {
    listener->Send(con, "LOGE You did not define a LoadSection function. Can not send section.");
  }
}

void asPEEK::SetBreakpoint(const std::vector<std::string> &words)
{

  int secId;
  int line;
  bool isSet = false;

  if(asPEEK_UTILS::ToInt(words[1], secId))
  {

    if(asPEEK_UTILS::ToInt(words[2], line))
    {
      if(sections.count(secId)) // section should exist
      {
        if(!sections[secId].breakpoints.count(line))
        {
          sections[secId].breakpoints.insert(line);
          isSet = true;
        }

      }
    }

  }

  if(isSet)
  {
    std::stringstream ss;
    ss << "BSET ";
    ss << secId;
    ss << " ";
    ss << line;
    listener->SendToAll(ss.str());
  }

}

void asPEEK::RemoveBreakpoint(const std::vector<std::string> &words)
{

  int secId;
  int line;
  bool isRemoved = false;

  if(asPEEK_UTILS::ToInt(words[1], secId))
  {

    if(asPEEK_UTILS::ToInt(words[2], line))
    {
      if(sections.count(secId)) // section should exist
      {
        if(sections[secId].breakpoints.count(line))
        {
          sections[secId].breakpoints.erase(line);
          isRemoved = true;
        }

      }
    }

  }

  if(isRemoved)
  {
    std::stringstream ss;
    ss << "BREM ";
    ss << secId;
    ss << " ";
    ss << line;
    listener->SendToAll(ss.str());
  }

}

void asPEEK::NewClient(websocketpp::server::connection_ptr connection)
{

  SendModuleNames(connection);

  SendSectionNames(connection);

  SendContexts(connection);

  boost::mutex::scoped_lock lock(debugging_mutex);
  if(debuggingContext)
  {
    SendLocalVariables(connection, debuggingContext);
    SendCurrentLine(connection);
    SendStack(connection, debuggingContext);
  }

}

void asPEEK::SendCurrentLine(websocketpp::server::connection_ptr con)
{
  if(debuggingContext &&
    debuggingSection &&
    debuggingLine
    )
  {
    std::stringstream ss;
    ss << "HITL ";
    ss << debuggingSection;
    ss << " ";
    ss << debuggingLine;

    listener->Send(con, ss.str());
  }
}

void asPEEK::SendLogMessageToAll(const std::string &msg)
{
  listener->SendToAll("LOGI "+ msg);
}

void asPEEK::SendMessageToAll(const std::string &msg)
{
  listener->SendToAll(msg);
}

void asPEEK::Pause()
{
  boost::mutex::scoped_lock lock(debugging_mutex);
  if(debuggingContext)
  {   
    debuggingLine = 0;
    debuggingSection = 0;
    nextDebugCommand = CONTINUE;
    debuggingContext = 0;
  }

  isPaused = true;
}

void asPEEK::Resume()
{
  isPaused = false;
}

bool asPEEK::AddBreakpoint(const std::string section, int line)
{
  if(!sectionIds.count(section))
    return false;

  SectionData *d = &sections[sectionIds[section]];

  if(d->breakpoints.count(line))
    return false;

  d->breakpoints.insert(line);
  return true;
}

bool asPEEK::RemoveBreakpoint(const std::string section, int line)
{
  if(!sectionIds.count(section))
    return false;

  SectionData *d = &sections[sectionIds[section]];

  if(d->breakpoints.count(line))
  {
    d->breakpoints.erase(line);
    return true;
  }
  return false;
}

void asPEEK::ContextIsInactive(asIScriptContext *ctx)
{
  boost::mutex::scoped_lock lock(debugging_mutex);
  if(ctx == debuggingContext)
  {
    lock.unlock(); // because EndDebugging also needs debugging_mutex
    EndDebugging();
  }
}

void asPEEK::EndDebugging()
{
  boost::mutex::scoped_lock lock(debugging_mutex);
  debuggingLine = 0;
  debuggingSection = 0;
  nextDebugCommand = CONTINUE;
  debuggingContext = 0;
  lock.unlock();

  if(DebuggingEndedFunction)
    DebuggingEndedFunction();

  listener->SendToAll("CONT");
}