#include "star_launcher.hpp"
#include "star_disc.hpp"
#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
namespace {
namespace fs=std::filesystem;
void ConfigureRenderer(){
#ifdef _WIN32
 const char* rawBase=SDL_GetBasePath();if(!rawBase)throw std::runtime_error(SDL_GetError());
 fs::path base=fs::u8path(rawBase),mesa=base/"Mesa"/"opengl32.dll";
 const char* requested=std::getenv("STAR_RENDERER");
 const bool native=requested&&SDL_strcasecmp(requested,"OpenGL")==0;
 const bool vulkan=!native&&requested&&SDL_strcasecmp(requested,"Vulkan")==0;
 if(vulkan){
  if(!fs::is_regular_file(mesa)||!fs::is_regular_file(base/"Mesa"/"libgallium_wgl.dll"))throw std::runtime_error("Arquivos do modo Vulkan ausentes na pasta Mesa");
  _putenv_s("STAR_RENDERER","Vulkan");_putenv_s("GALLIUM_DRIVER","zink");
  _putenv_s("STAR_REQUIRE_VULKAN_GPU","1");_putenv_s("LIBGL_ALWAYS_SOFTWARE","0");_putenv_s("ZINK_USE_LAVAPIPE","");
  std::string library=mesa.u8string();SDL_SetHint(SDL_HINT_OPENGL_LIBRARY,library.c_str());
  std::string path=(base/"Mesa").u8string()+";"+(std::getenv("PATH")?std::getenv("PATH"):"");_putenv_s("PATH",path.c_str());
 }else if(native){
  const char* windows=std::getenv("WINDIR");
  if(windows){auto library=(fs::u8path(windows)/"System32"/"opengl32.dll").u8string();SDL_SetHint(SDL_HINT_OPENGL_LIBRARY,library.c_str());}
  _putenv_s("STAR_REQUIRE_VULKAN_GPU","");
 }
#endif
}

fs::path Preferences(){char* raw=SDL_GetPrefPath("StarSuccessor","NativePort");if(!raw)throw std::runtime_error(SDL_GetError());auto p=fs::u8path(raw);SDL_free(raw);return p;}
struct Picked {std::atomic<bool> done{false};std::string path,error;};
fs::path SelectImage(){
 if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
 Picked picked;const SDL_DialogFileFilter filters[]={{"Star Successor (RVZ ou ISO)","rvz;iso"}};
 SDL_ShowOpenFileDialog([](void* data,const char* const* list,int){auto& p=*static_cast<Picked*>(data);if(list&&list[0])p.path=list[0];if(!list)p.error=SDL_GetError();p.done.store(true,std::memory_order_release);},&picked,nullptr,filters,1,nullptr,false);
 while(!picked.done.load(std::memory_order_acquire)){SDL_PumpEvents();SDL_Delay(10);}
 if(!picked.error.empty())throw std::runtime_error(picked.error);return fs::u8path(picked.path);
}
fs::path Import(const fs::path& source,const fs::path& cache){
 if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
 SDL_Window* window=SDL_CreateWindow("Preparando Star Successor...",640,120,0);if(!window)throw std::runtime_error(SDL_GetError());
 std::atomic<bool> done{false},cancel{false};std::atomic<unsigned> percent{0};std::string error;fs::path result;
 std::thread worker([&]{try{result=StarDisc::ImportImage(source,cache,[&](size_t i,size_t n,const std::string&){if(cancel.load())throw std::runtime_error("Importacao cancelada");percent.store(unsigned(i*100/n));});}catch(const std::exception& e){error=e.what();}done.store(true,std::memory_order_release);});
 unsigned shown=~0u;
 while(!done.load(std::memory_order_acquire)){
  SDL_Event event;while(SDL_PollEvent(&event))if(event.type==SDL_EVENT_QUIT)cancel=true;
  unsigned value=percent.load();if(shown!=value){shown=value;std::string title="Preparando Star Successor - "+std::to_string(value)+"%";SDL_SetWindowTitle(window,title.c_str());}
  if(auto* surface=SDL_GetWindowSurface(window)){SDL_FillSurfaceRect(surface,nullptr,SDL_MapSurfaceRGB(surface,24,30,43));SDL_Rect bar{24,50,int(592*value/100),20};SDL_FillSurfaceRect(surface,&bar,SDL_MapSurfaceRGB(surface,72,196,239));SDL_UpdateWindowSurface(window);}SDL_Delay(30);
 }
 worker.join();SDL_DestroyWindow(window);if(!error.empty())throw std::runtime_error(error);return result;
}
}
int StarPrepareContent(int argc,char** argv,std::string& discRoot,std::string& nandRoot){
 try {
  ConfigureRenderer();
  if(argc>=2&&std::string(argv[1])=="--help"){std::puts("Star Successor native port\n  --game <arquivo.rvz|arquivo.iso|pasta-extraida>\n  --import-only <imagem> <destino>\n  Esc: configuracoes do port");return 0;}
  if(argc==4&&std::string(argv[1])=="--import-only"){StarDisc::ImportImage(fs::u8path(argv[2]),fs::u8path(argv[3]),[](size_t i,size_t n,const std::string& name){std::printf("[%zu/%zu] %s\n",i,n,name.c_str());});return 0;}
  fs::path source;if(argc==3&&std::string(argv[1])=="--game")source=fs::absolute(fs::u8path(argv[2]));else if(argc>1)throw std::runtime_error("Argumentos invalidos; use --help");
  bool development=StarDisc::ValidateTree("game/extracted/DATA");fs::path storage=development?fs::absolute("game"):Preferences();fs::create_directories(storage);
  fs::path root,cache=storage/"imported/R2VE01";
  if(source.empty()&&development)root=fs::absolute("game/extracted/DATA");
  else if(source.empty()&&StarDisc::ValidateTree(cache))root=cache;
  else {
   if(source.empty())source=SelectImage();if(source.empty())return 0;
   if(fs::is_directory(source)){if(!StarDisc::ValidateTree(source))throw std::runtime_error("A pasta nao contem a versao USA suportada");root=source;}else root=Import(source,cache);
  }
  discRoot=root.u8string();nandRoot=(storage/"nand").u8string();
  if(const char* testNand=std::getenv("STAR_TEST_NAND_ROOT")){
   fs::path path=fs::u8path(testNand);
   if(!path.is_absolute()||!fs::is_directory(path))throw std::runtime_error("Pasta temporaria de teste NAND invalida");
   nandRoot=path.u8string();
  }
  if(!development)fs::current_path(storage);
  std::printf("[game] %s\n",discRoot.c_str());return -1;
 }catch(const std::exception& e){std::fprintf(stderr,"[load] %s\n",e.what());SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Star Successor",e.what(),nullptr);return 1;}
}
