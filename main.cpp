#ifdef WIN_SIM
    #include <direct.h>
    #include <Windows.h>
#else
    #include <unistd.h>
#endif

#include <string>
#include <filesystem>
#include <thread>
#include <simLib/simLib.h>

// Following required to have Lua extension libraries work under Linux:
#ifdef LIN_SIM
    extern "C" {
        #include <lua.h>
        #include <lualib.h>
        #include <lauxlib.h>
    }
    void dummyFunction()
    {
        lua_State *L;
        L=luaL_newstate();
    }
#endif

static LIBRARY simLib=nullptr;
static bool autoStart=false;
static bool autoQuit=false;
static int stopDelay=0;
static std::string sceneOrModel;
static std::string appDir;

void unloadSimLib()
{
    simAddLog("CoppeliaSimClient",sim_verbosity_loadinfos,"unloading the CoppeliaSim library...");
    unloadSimLibrary(simLib);
    simLib=nullptr;
    simAddLog("CoppeliaSimClient",sim_verbosity_loadinfos,"done.");
}

bool loadSimLib()
{
    simAddLog("CoppeliaSimClient",sim_verbosity_loadinfos,"loading the CoppeliaSim library...");
    #ifdef WIN_SIM
        simLib=loadSimLibrary("coppeliaSim");
    #endif
    #ifdef MAC_SIM
        simLib=loadSimLibrary("@executable_path/libcoppeliaSim.dylib");
    #endif
    #ifdef LIN_SIM
        simLib=loadSimLibrary("libcoppeliaSim.so");
    #endif
    if (simLib!=NULL)
    {
        if (getSimProcAddresses(simLib)!=0)
            simAddLog("CoppeliaSimClient",sim_verbosity_loadinfos,"done.");
        else
        {
            simAddLog("CoppeliaSimClient",sim_verbosity_errors,"could not find all required functions in the CoppeliaSim library.");
            unloadSimLib();
        }
    }
    else
        simAddLog("CoppeliaSimClient",sim_verbosity_errors,"could not find or correctly load the CoppeliaSim library.");
    return(simLib!=nullptr);
}

bool endsWith(const std::string& value,const std::string& ending)
{
    bool retVal=false;
    if (ending.size()<=value.size())
        retVal=std::equal(ending.rbegin(),ending.rend(),value.rbegin());
    return(retVal);
}

void simThreadStartAddress()
{
    #ifdef WIN_SIM
        timeBeginPeriod(1);
    #endif

    simInit(appDir.c_str(),0);
    bool autoStarted=false;
    int simulationRunCnt=0;
    while (!simGetExitRequest())
    {
        if (sceneOrModel.size()>0)
        {
            if ( endsWith(sceneOrModel,".ttt")||endsWith(sceneOrModel,".simscene.xml") )
            {
                if (simLoadScene(sceneOrModel.c_str())==-1)
                    simAddLog("CoppeliaSimClient",sim_verbosity_errors,"scene could not be opened.");
            }
            if ( endsWith(sceneOrModel,".ttm")||endsWith(sceneOrModel,".simmodel.xml"))
            {
                if (simLoadModel(sceneOrModel.c_str())==-1)
                    simAddLog("CoppeliaSimClient",sim_verbosity_errors,"model could not be opened.");
            }
            sceneOrModel.clear();
        }
        if (autoStart)
        {
            autoStart=false;
            simStartSimulation();
            autoStarted=true;
        }
        int simState=simGetSimulationState();
        if (simState==sim_simulation_advancing_firstafterstop)
            simulationRunCnt++;
        if ( (simState==sim_simulation_stopped)&&(simulationRunCnt==1)&&autoQuit )
            simPostExitRequest(); // notifiy the GUI
        if ( (stopDelay>0)&& autoStarted && (simGetSimulationTime()*1000>double(stopDelay)) )
        {
            stopDelay=0;
            simStopSimulation();
        }

        simLoop(nullptr,0);
    }
    simCleanup();

    #ifdef WIN_SIM
        timeEndPeriod(1);
    #endif
}

int main(int argc,char* argv[])
{
    int exitCode=255;
    std::filesystem::path path(argv[0]);
    appDir=path.parent_path().string();
    _chdir(appDir.c_str());
    if (loadSimLib())
    {
        int options=sim_gui_all;
        for (int i=1;i<argc;i++)
        {
            std::string arg(argv[i]);
            if (arg[0]=='-')
            {
                if (arg.length()>=2)
                {
                    if (arg[1]=='h')
                    {
                        options|=sim_gui_all|sim_gui_headless;
                        options-=sim_gui_all;
                    }
                    if (arg[1]=='s')
                    {
                        autoStart=true;
                        stopDelay=0;
                        unsigned int p=2;
                        while (arg.length()>p)
                        {
                            stopDelay*=10;
                            if ((arg[p]>='0')&&(arg[p]<='9'))
                                stopDelay+=arg[p]-'0';
                            else
                            {
                                stopDelay=0;
                                break;
                            }
                            p++;
                        }
                    }
                    if (arg[1]=='q')
                        autoQuit=true;
                    if (arg[1]=='c')
                    {
                        std::string tmp;
                        tmp.assign(arg.begin()+2,arg.end());
                        simSetStringParam(sim_stringparam_startupscriptstring,tmp.c_str());
                    }
                    if (arg[1]=='v')
                    {
                        std::string tmp;
                        tmp.assign(arg.begin()+2,arg.end());
                        simSetStringParam(sim_stringparam_verbosity,tmp.c_str());
                    }
                    if (arg[1]=='w')
                    {
                        std::string tmp;
                        tmp.assign(arg.begin()+2,arg.end());
                        simSetStringParam(sim_stringparam_statusbarverbosity,tmp.c_str());
                    }
                    if (arg[1]=='x')
                    {
                        std::string tmp;
                        tmp.assign(arg.begin()+2,arg.end());
                        simSetStringParam(sim_stringparam_dlgverbosity,tmp.c_str());
                    }
                    if ((arg[1]=='a')&&(arg.length()>2))
                    {
                        std::string tmp;
                        tmp.assign(arg.begin()+2,arg.end());
                        simSetStringParam(sim_stringparam_additional_addonscript1,tmp.c_str()); // normally, never call API functions before simRunSimulator!!
                    }
                    if ((arg[1]=='b')&&(arg.length()>2))
                    {
                        std::string tmp;
                        tmp.assign(arg.begin()+2,arg.end());
                        simSetStringParam(sim_stringparam_additional_addonscript2,tmp.c_str()); // normally, never call API functions before simRunSimulator!!
                    }
                    if ((arg[1]=='g')&&(arg.length()>2))
                    {
                        static int cnt=0;
                        std::string tmp;
                        tmp.assign(arg.begin()+2,arg.end());
                        if (cnt<9)
                            simSetStringParam(sim_stringparam_app_arg1+cnt,tmp.c_str()); // normally, never call API functions before simRunSimulator!!
                        cnt++;
                    }
                    if ((arg[1]=='G')&&(arg.length()>3))
                    {
                        size_t pos=arg.find('=',3);
                        if ( (pos!=std::string::npos)&&(pos!=arg.length()-1) )
                        {
                            std::string key(arg.begin()+2,arg.begin()+pos);
                            std::string param(arg.begin()+pos+1,arg.end());
                            simSetNamedStringParam(key.c_str(),param.c_str(),int(param.size()));
                        }
                    }
                }
            }
            else
            {
                if (sceneOrModel.size()==0)
                    sceneOrModel=arg;
            }
        }

        simAddLog("CoppeliaSimClient",sim_verbosity_loadinfos,"launching CoppeliaSim...");
        std::thread simThread(simThreadStartAddress);
        simRunGui(options);
        simThread.join();
        exitCode=0;
        simGetInt32Param(sim_intparam_exitcode,&exitCode);
        unloadSimLib();
    }
    return(exitCode);
}
