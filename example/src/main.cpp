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


#include "asPEEK.h"

#include <iostream>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <fstream>
#include <scriptstdstring\scriptstdstring.h>
#include <scriptarray\scriptarray.h>
#include <boost/thread/mutex.hpp>

using namespace std;

//#define THREADOUTPUT

asIScriptEngine *engine = 0;
asIScriptModule *mod = 0;
asIScriptModule *mod2 = 0;
asIScriptModule *mod3 = 0;
asPEEK *peek = 0;

bool endThreadExecution = false;

// thread for the script that is not debugged
boost::thread not_debugged_thread;
boost::thread thread1;
boost::thread thread2;
boost::thread thread3;
boost::thread thread4;

std::vector<std::pair<std::string, asIScriptModule*>> files;

boost::mutex gcMutex;

class Vector3
{
public:

  Vector3() 
  {
    x = 0;
    y = 0;
    z = 0;
  }

  ~Vector3() {
    int a = 1;
  }

  float x,y,z;

};

template<typename T>
void GenericDefaultConstructor(T *self)
{
  new (self) T();
}

template<typename T>
void GenericDestructor(T *self)
{
  ((T*) self)->~T();
}

void FillFiles()
{
  std::string scriptDirectory = "../scripts/";

  files.push_back(std::make_pair(scriptDirectory + "not_debugged.as", mod));
  files.push_back(std::make_pair(scriptDirectory + "file1.as", mod));
  files.push_back(std::make_pair(scriptDirectory + "file2.as", mod));
  files.push_back(std::make_pair(scriptDirectory + "file3.as", mod));
  files.push_back(std::make_pair(scriptDirectory + "file4.as", mod2));
  files.push_back(std::make_pair(scriptDirectory + "file5.as", mod2));
  files.push_back(std::make_pair(scriptDirectory + "file6.as", mod3));
  files.push_back(std::make_pair(scriptDirectory + "file7.as", mod3));
}

void RegisterVector3()
{
  int r = engine->RegisterObjectType("Vector3", sizeof(Vector3), asOBJ_VALUE | asOBJ_POD );
  assert( r >= 0);

  r = engine->RegisterObjectProperty("Vector3", "float x", offsetof(Vector3, x));
  assert( r >= 0);
  r = engine->RegisterObjectProperty("Vector3", "float y", offsetof(Vector3, y));
  assert( r >= 0);
  r = engine->RegisterObjectProperty("Vector3", "float z", offsetof(Vector3, z));
  assert( r >= 0);

  r = engine->RegisterObjectBehaviour("Vector3", asBEHAVE_CONSTRUCT, "void Vector3()", asFUNCTION(GenericDefaultConstructor<Vector3>), asCALL_CDECL_OBJLAST);
  assert( r >= 0);
  r = engine->RegisterObjectBehaviour("Vector3", asBEHAVE_DESTRUCT, "void Vector3()", asFUNCTION(GenericDestructor<Vector3>), asCALL_CDECL_OBJLAST);
  assert( r >= 0);

}

void Register()
{
  RegisterVector3();
  RegisterStdString(engine);
  RegisterScriptArray(engine, false);
}

void MessageCallback(const asSMessageInfo *msg, void *param)
{
  const char *type = "ERR ";
  if (msg->type == asMSGTYPE_WARNING)
    type = "WARN";
  else if (msg->type == asMSGTYPE_INFORMATION)
    type = "INFO";

  printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

bool LoadScriptFile(const std::string &filename, string &script)
{
  ifstream ifs;
  ifs.open(filename.c_str(), ifstream::in);

  if(!ifs.good())
    return false;

  int ch = ifs.get();
  while (ifs.good())
  {
    script += (char) ch;
    ch = ifs.get();
  }

  ifs.close();
  return true;
}

void SaveScriptFile(const std::string &filename, const std::string &script)
{
  ofstream ofs(filename.c_str(), ios::out);
  ofs << script;
  ofs.close();
}

void SetupAngelscript()
{
  int r;
  engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
  engine->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, false);
  r = engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
  assert( r >= 0);

  mod = engine->GetModule("module 1", asGM_ALWAYS_CREATE);
  mod2 = engine->GetModule("module 2", asGM_ALWAYS_CREATE);
  mod3 = engine->GetModule("module 3", asGM_ALWAYS_CREATE);
}

bool LoadTestFile(asPEEK *peek)
{
  FillFiles();

  // don't add not_debugged.as to asPEEK
  // we start from 1 beacuse of that
  for(unsigned int i = 0; i < files.size(); ++i)
  {
    string f;
    bool r = LoadScriptFile(files[i].first, f);
    if(!r) 
      return false;

    files[i].second->AddScriptSection(files[i].first.c_str(), f.c_str());

    // don't add not_debugged.as to asPEEK
    // we start from 1 beacuse of that
    if(i == 0)
      continue;

    peek->AddScriptSection(files[i].first, files[i].second->GetName());
  }

  files.clear();


  // ADD BREAKPOINT, before even client can connect
  //	peek->AddBreakpoint(filename, 30);

  int r = mod->Build();
  if(r < 0)
  {
    engine->DiscardModule(mod->GetName());
    mod = 0;
    return false;
  }

  r = mod2->Build();
  if(r < 0)
  {
    engine->DiscardModule(mod2->GetName());
    mod3 = 0;
    return false;
  }

  mod3->Build();
  if(r < 0)
  {
    engine->DiscardModule(mod3->GetName());
    mod3 = 0;
    return false;
  }

  return true;
}


std::string Vector3ToString(void *vec)
{
  Vector3 *v = (Vector3*) vec;

  std::stringstream ss;

  // Adds type name to package. This is optional, but it is nice to type of the object in debugger
  // Why _ character ? _ is not a valid angelscript variable name, but it is a valid JSON/JavaScript variable name.
  // There is no question about _ being the typename of the object.
  // Also it is short.
  ss << "{\"_\":\"Vector3\",";

  ss << "\"x\":" << v->x << ",\"y\":" << v->y << ",\"z\":" << v->z << "}";

  return ss.str();
}

/** What a function name*/
std::string StringToString(void *str)
{
  std::string *v = (std::string*) str;

  std::stringstream ss;

  ss << "{\"_\":\"string\",";

  ss << "\"*\":\"" << *v << "\"}";

  return ss.str();
}

void ExecuteFunction(asIScriptModule *module, const std::string &funcName)
{

  asIScriptFunction *main = module->GetFunctionByDecl(funcName.c_str());

  asIScriptContext *ctx = engine->CreateContext();

  peek->AddContext(ctx);

  while (!endThreadExecution)
  {
    // don't kill my cpu please
    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

    {
      //  garbage collection is not thread safe. 
      // in a normal application garbage collection if initiated in a more performance friendly way
      gcMutex.lock();
      engine->GarbageCollect(asGC_FULL_CYCLE);
      gcMutex.unlock();
    }

    if (main)
    {
      ctx->Prepare(main);
      ctx->Execute();
      ctx->Unprepare();
#ifdef THREADOUTPUT
      cout << funcName << "thread1 executing...\n";
#endif
    }
    else
    {
      cout << "Failed to run!" << funcName << "\n";
      break;
    }
  }

  ctx->Release();
  asThreadCleanup();
}

void Restart();

void DebuggingStarted()
{
  std::cout<<"Debugging started...\n";
}

void DebuggingEnded()
{
  std::cout<<"Debugging ended\n";
}

void Start()
{

  SetupAngelscript();

  Register();

  // create asPEEK
  peek = new asPEEK(engine, 9002);
  // called when Restart button on client is pressed.
  peek->RestartFunction = &Restart;
  // callback when debugging starts. stop your game internal clock or something in this
  peek->DebuggingStartedFunction = &DebuggingStarted;
  // callback when debugging end
  peek->DebuggingEndedFunction = &DebuggingEnded;
  // Load files
  bool isLoadedFiles = LoadTestFile(peek);

  // make sure all modules are compiled correntcly
  if(!isLoadedFiles  || !mod || !mod2 || !mod3)
  {
    delete peek;
    std::cout << "Error: Please fix the compile errors in script files!\n";
    return;
  }

  // function that loads a section 
  peek->LoadSectionFunction = &LoadScriptFile;
  // function that saves a section
  peek->SaveSectionFunction = &SaveScriptFile;

  // add modules to peek so that we can lookup global variables in them
  /**/
  peek->AddModule(mod);
  peek->AddModule(mod2);
  peek->AddModule(mod3);
  // special type conversions. if a type is not specified only its address will be visible
  peek->AddSpecialTypeConversion(engine->GetTypeIdByDecl("Vector3"), &Vector3ToString);
  peek->AddSpecialTypeConversion(engine->GetTypeIdByDecl("string"), &StringToString);

  // this function is called once
  asIScriptContext *ctx = engine->CreateContext();
  asIScriptFunction * func = mod->GetFunctionByDecl("void Once()");
  ctx->Prepare( func );
  ctx->Execute();
  ctx->Unprepare();
  ctx->Release();

  // start 4 script execution thread
  not_debugged_thread = boost::thread( boost::bind(&ExecuteFunction, mod, "int not_debugged()")); 
  thread1 = boost::thread( boost::bind(&ExecuteFunction, mod, "void thread1()")); 
  //thread2 = boost::thread( boost::bind(&ExecuteFunction, mod2, "void thread2()")); 
  //thread3 = boost::thread( boost::bind(&ExecuteFunction, mod3, "void thread3()")); 
  //thread4 = boost::thread( boost::bind(&ExecuteFunction, mod3, "void thread4()")); 

  // listen incoming connection.
  peek->Listen();
}

void Restart()
{
  endThreadExecution = true;

  not_debugged_thread.join();
  thread1.join();
  thread2.join();
  thread3.join();
  thread4.join();

  delete peek;

  engine->DiscardModule(mod->GetName());
  engine->Release();

  endThreadExecution = false;
  Start();

}


int main()
{
  {
    Start();

    std::cout << "Type 'e' then press Enter to exit peacefully\n";

    // wait for 'e' input to end demo
    char c;
    while(cin >> c)
    {
      if(c = 'e')
        break;
    }

    endThreadExecution = true;

    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();

    // clean up
    delete peek;

    engine->DiscardModule(mod->GetName());
    engine->Release();
  }


#ifdef _MSC_VER 
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
  _CrtDumpMemoryLeaks();
#endif 

  return 0;
}
