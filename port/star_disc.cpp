// Native Wii image import. RVZ layout/packing adapted from MIT-licensed
// wiikit/rvz.py and disc.py. Only assets are read here; CPU execution remains
// the statically translated Wiicompiled program.
#include "star_disc.hpp"
#include "../tooling/zstd-source/zstd-1.5.7/lib/zstd.h"
extern "C" {
#include "../tooling/asset-deps/tiny-AES-c-1.0.0/aes.h"
}
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>
namespace StarDisc { namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<uint8_t>;
constexpr uint64_t Sector=0x8000,Payload=0x7C00;
uint32_t U32(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
uint64_t U64(const uint8_t* p){return uint64_t(U32(p))<<32|U32(p+4);}
void Need(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
uint64_t HashFile(const fs::path& path){
 std::ifstream f(path,std::ios::binary);if(!f)return 0;uint64_t h=14695981039346656037ull;std::array<char,65536> b;
 while(f){f.read(b.data(),b.size());for(std::streamsize i=0;i<f.gcount();++i)h=(h^uint8_t(b[i]))*1099511628211ull;}return h;
}
struct Segment {uint32_t first,count,group,groups;};
struct Part {Segment segments[2];};
struct Group {uint64_t offset;uint32_t size,packed;};
struct RawRegion {uint64_t start,end;uint32_t group,groups;};
struct Lfg {
 uint32_t b[521]{};size_t pos=0;
 void Forward(){for(size_t i=0;i<32;++i)b[i]^=b[i+489];for(size_t i=32;i<521;++i)b[i]^=b[i-32];}
 explicit Lfg(const uint8_t* seed){for(int i=0;i<17;++i)b[i]=U32(seed+4*i);for(int i=17;i<521;++i)b[i]=(b[i-17]<<23)^(b[i-16]>>9)^b[i-1];for(auto& w:b)w=(w&0xFF00FFFFu)|((w>>2)&0x00FF0000u);for(int i=0;i<4;++i)Forward();}
 void Skip(size_t n){pos+=n;while(pos>=2084){Forward();pos-=2084;}}
 void Read(uint8_t* out,size_t n){for(size_t i=0;i<n;++i){out[i]=uint8_t(b[pos/4]>>(24-8*(pos%4)));if(++pos==2084){Forward();pos=0;}}}
};
class Image {
 std::ifstream file;uint64_t fileSize=0,discSize=0;bool rvz=false;uint32_t compression=0,chunk=0;
 Bytes discHead;std::vector<Part> parts;std::vector<Group> groups;std::vector<RawRegion> regions;std::map<size_t,Bytes> cache;
 uint64_t partition=0,dataOffset=0,payloadSize=0,startSector=0,lastCluster=~0ull;const Part* part=nullptr;std::array<uint8_t,16> titleKey{};Bytes cluster;
 Bytes File(uint64_t off,size_t n){Need(off<=fileSize && n<=fileSize-off,"Imagem truncada ou offsets invalidos");Bytes b(n);file.clear();file.seekg(off);file.read((char*)b.data(),n);Need(size_t(file.gcount())==n,"Falha de leitura da imagem");return b;}
 Bytes Decompress(Bytes data){
  if(compression==0)return data;Need(compression==5,"Este RVZ usa uma compressao nao suportada. Use RVZ com Zstandard.");
  auto size=ZSTD_getFrameContentSize(data.data(),data.size());Need(size!=ZSTD_CONTENTSIZE_ERROR,"Bloco Zstandard invalido");
  size_t capacity=size==ZSTD_CONTENTSIZE_UNKNOWN?std::max<size_t>(chunk*2,2<<20):size;Need(capacity<=64u*1024*1024,"Bloco RVZ excede o limite");
  Bytes out(capacity);size_t n=ZSTD_decompress(out.data(),out.size(),data.data(),data.size());Need(!ZSTD_isError(n),"Falha ao descompactar RVZ");out.resize(n);return out;
 }
 const Bytes& GetGroup(size_t index,uint64_t dataOff,uint32_t sectors){
  auto found=cache.find(index);if(found!=cache.end())return found->second;Need(index<groups.size(),"Indice de grupo RVZ invalido");
  const auto g=groups[index];size_t want=sectors?size_t(sectors*Payload):chunk;Bytes out;
  if(!(g.size&0x7FFFFFFFu))out.resize(want);
  else {
   bool compressed=(g.size>>31)!=0;Bytes data=File(g.offset,g.size&0x7FFFFFFFu);if(compressed)data=Decompress(std::move(data));size_t at=0;
   if(sectors){for(size_t i=0;i<std::max<uint32_t>(1,chunk/(Sector*64));++i){Need(at+2<=data.size(),"Tabela de hashes RVZ truncada");size_t n=(uint32_t(data[at])<<8)|data[at+1];at+=2+22*n;Need(at<=data.size(),"Excecao de hash RVZ invalida");}if(!compressed)at=(at+3)&~size_t(3);}
   Need(at<=data.size(),"Cabecalho de grupo RVZ invalido");
   if(!g.packed)out.assign(data.begin()+at,data.end());
   else while(at<data.size()){
    Need(at+4<=data.size(),"Bloco de empacotamento RVZ truncado");uint32_t length=U32(data.data()+at);at+=4;size_t n=length&0x7FFFFFFFu;
    Need(n<=want && out.size()<=want-n,"Bloco RVZ excede o tamanho esperado");size_t begin=out.size();out.resize(begin+n);
    if(length>>31){Need(at+68<=data.size(),"Semente RVZ truncada");Lfg generator(data.data()+at);at+=68;generator.Skip(dataOff%Sector);generator.Read(out.data()+begin,n);}
    else {Need(at+n<=data.size(),"Dados RVZ truncados");std::memcpy(out.data()+begin,data.data()+at,n);at+=n;}
    dataOff+=n;
   }
  }
  Need(out.size()<=want,"Grupo RVZ maior que o esperado");out.resize(want);if(cache.size()>64)cache.clear();return cache.emplace(index,std::move(out)).first->second;
 }
public:
 Bytes ticket,tmd,header,region;
 explicit Image(const fs::path& path):file(path,std::ios::binary){
  Need(bool(file),"Nao foi possivel abrir a imagem");file.seekg(0,std::ios::end);fileSize=file.tellg();Need(fileSize>=0x100,"Imagem pequena demais");Bytes h=File(0,0x48);rvz=std::memcmp(h.data(),"RVZ\1",4)==0;
  if(rvz){
   uint32_t dsize=U32(h.data()+12);Need(dsize>=0xDC&&dsize<=4096,"Cabecalho RVZ invalido");discSize=U64(h.data()+0x24);auto d=File(0x48,dsize);compression=U32(d.data()+4);chunk=U32(d.data()+12);Need(chunk>=Sector && chunk<=64u*1024*1024 && chunk%Sector==0,"Tamanho de bloco RVZ invalido");
   discHead.assign(d.begin()+0x10,d.begin()+0x90);uint32_t npart=U32(d.data()+0x90),psize=U32(d.data()+0x94);Need(npart<=32&&psize>=48&&psize<=4096,"Particoes RVZ invalidas");auto pt=File(U64(d.data()+0x98),size_t(npart)*psize);
   for(uint32_t i=0;i<npart;++i){Part p{};for(int k=0;k<2;++k){auto* e=pt.data()+size_t(i)*psize+16+16*k;p.segments[k]={U32(e),U32(e+4),U32(e+8),U32(e+12)};}parts.push_back(p);}
   uint32_t nr=U32(d.data()+0xB4);Need(nr<=100000,"Tabela RVZ excessiva");auto rt=Decompress(File(U64(d.data()+0xB8),U32(d.data()+0xC0)));Need(rt.size()==size_t(nr)*24,"Tabela de regioes RVZ invalida");
   for(uint32_t i=0;i<nr;++i){auto* e=rt.data()+24*i;uint64_t off=U64(e);regions.push_back({off-off%Sector,off+U64(e+8),U32(e+16),U32(e+20)});}
   uint32_t ng=U32(d.data()+0xC4);Need(ng<=10000000,"Tabela de grupos RVZ excessiva");auto gt=Decompress(File(U64(d.data()+0xC8),U32(d.data()+0xD0)));Need(gt.size()==size_t(ng)*12,"Tabela de grupos RVZ invalida");
   for(uint32_t i=0;i<ng;++i){auto* e=gt.data()+12*i;groups.push_back({uint64_t(U32(e))<<2,U32(e+4),U32(e+8)});}
  } else {discSize=fileSize;Need(std::memcmp(h.data(),"WBFS",4)!=0,"Use ISO ou RVZ; WBFS ainda nao e suportado");}
  header=ReadDisc(0,0x100);Need(std::memcmp(header.data(),"R2VE01",6)==0&&U32(header.data()+0x18)==0x5D1C9EA3,"Esta versao do port requer Star Successor USA (R2VE01)");
  bool selected=false;auto partitionGroups=ReadDisc(0x40000,32);
  for(int g=0;g<4&&!selected;++g){uint32_t count=U32(partitionGroups.data()+8*g);Need(count<1024,"Tabela de particoes invalida");auto table=ReadDisc(uint64_t(U32(partitionGroups.data()+8*g+4))<<2,size_t(count)*8);for(uint32_t i=0;i<count;++i)if(U32(table.data()+8*i+4)==0){partition=uint64_t(U32(table.data()+8*i))<<2;selected=true;break;}}
  Need(selected,"Particao DATA nao encontrada");auto t=ReadDisc(partition,0x2C0);ticket.assign(t.begin(),t.begin()+0x2A4);uint32_t tmdSize=U32(t.data()+0x2A4);Need(tmdSize<=1<<20,"TMD invalido");tmd=ReadDisc(partition+(uint64_t(U32(t.data()+0x2A8))<<2),tmdSize);region=ReadDisc(0x4E000,0x20);
  dataOffset=uint64_t(U32(t.data()+0x2B8))<<2;uint64_t physicalSize=uint64_t(U32(t.data()+0x2BC))<<2;payloadSize=(physicalSize/Sector)*Payload;startSector=(partition+dataOffset)/Sector;
  if(rvz){for(auto& p:parts)if(p.segments[0].first==startSector){part=&p;break;}Need(part,"Payload DATA nao encontrado no RVZ");}
  else {
   static constexpr uint8_t common[16]={0xEB,0xE4,0x2A,0x22,0x5E,0x85,0x93,0xE4,0x48,0xD9,0xC5,0x45,0x73,0x81,0xAA,0xF7};Need(t[0x1F1]==0,"Chave de disco nao suportada");uint8_t iv[16]{};std::memcpy(iv,t.data()+0x1DC,8);std::memcpy(titleKey.data(),t.data()+0x1BF,16);AES_ctx aes;AES_init_ctx_iv(&aes,common,iv);AES_CBC_decrypt_buffer(&aes,titleKey.data(),16);
  }
 }
 Bytes ReadDisc(uint64_t off,size_t n){
  if(!rvz)return File(off,n);Need(off<=discSize&&n<=discSize-off,"Leitura fora da imagem RVZ");Bytes out(n);size_t done=0;
  while(done<n){uint64_t pos=off+done;if(pos<0x80){size_t take=std::min<size_t>(n-done,0x80-pos);std::memcpy(out.data()+done,discHead.data()+pos,take);done+=take;continue;}
   bool found=false;for(const auto& r:regions)if(r.start<=pos&&pos<r.end){uint64_t gi=(pos-r.start)/chunk,within=(pos-r.start)%chunk;Need(gi<r.groups,"Grupo de regiao RVZ fora dos limites");const auto& b=GetGroup(r.group+gi,pos-within,0);size_t take=std::min<uint64_t>({n-done,r.end-pos,chunk-within});std::memcpy(out.data()+done,b.data()+within,take);done+=take;found=true;break;}
   if(found)continue;uint64_t next=discSize;for(auto& r:regions)if(r.start>pos)next=std::min(next,r.start);for(auto& p:parts)for(auto& s:p.segments){uint64_t start=uint64_t(s.first)*Sector;if(s.count&&start<=pos&&pos<start+uint64_t(s.count)*Sector)throw std::runtime_error("Leitura bruta em uma particao RVZ");if(s.count&&start>pos)next=std::min(next,start);}Need(next>pos,"Mapa RVZ invalido");done+=std::min<uint64_t>(n-done,next-pos);
  }return out;
 }
 Bytes Read(uint64_t off,size_t n){
  Need(off<=payloadSize&&n<=payloadSize-off,"Leitura fora da particao DATA");Bytes out(n);size_t done=0;
  while(done<n){uint64_t pos=off+done;
   if(!rvz){uint64_t index=pos/Payload,within=pos%Payload;if(index!=lastCluster){auto raw=File(partition+dataOffset+index*Sector,Sector);cluster.assign(raw.begin()+0x400,raw.end());AES_ctx aes;AES_init_ctx_iv(&aes,titleKey.data(),raw.data()+0x3D0);AES_CBC_decrypt_buffer(&aes,cluster.data(),cluster.size());lastCluster=index;}size_t take=std::min<size_t>(n-done,Payload-within);std::memcpy(out.data()+done,cluster.data()+within,take);done+=take;continue;}
   uint64_t sector=pos/Payload+startSector,within=pos%Payload;bool found=false;uint32_t per=chunk/Sector;
   for(auto& s:part->segments)if(s.first<=sector&&sector<uint64_t(s.first)+s.count){uint64_t gi=(sector-s.first)/per,gs=(sector-s.first)%per;Need(gi<s.groups,"Grupo de payload RVZ fora dos limites");uint32_t sectors=std::min<uint64_t>(per,uint64_t(s.first)+s.count-(s.first+gi*per));uint64_t base=(s.first+gi*per-startSector)*Payload;const auto& b=GetGroup(s.group+gi,base,sectors);size_t at=gs*Payload+within,take=std::min<uint64_t>(n-done,uint64_t(sectors)*Payload-at);Need(take>0,"Payload RVZ vazio");std::memcpy(out.data()+done,b.data()+at,take);done+=take;found=true;break;}
   Need(found,"Payload RVZ truncado");
  }return out;
 }
};
void Write(const fs::path& path,const Bytes& bytes){fs::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary);Need(bool(f),"Nao foi possivel criar arquivo de cache");f.write((const char*)bytes.data(),bytes.size());Need(bool(f),"Falha ao gravar cache; verifique o espaco livre");}
struct CodeFile {const char* file;uint64_t hash;};
const CodeFile codeFiles[]={
 {"sys/main.dol",0xB85598501FB77874ull},
 {"files/stg00.rso",0xC578834D58FC3DA3ull},{"files/stg01.rso",0xCA8641C18C80FDE7ull},
 {"files/stg02.rso",0x670C8244905A3D86ull},{"files/stg03.rso",0xC76D1557F102C3FFull},
 {"files/stg04.rso",0xDB574B3EDC79DDDDull},{"files/stg05.rso",0xD97AFCB833D26EF6ull},
 {"files/stg06.rso",0xEB1FD8A3F666F899ull},{"files/stg07.rso",0x62A81AA6E86B2106ull}};
} // namespace
bool ValidateTree(const fs::path& directory){
 try {
  for(auto& f:codeFiles)if(HashFile(directory/fs::u8path(f.file))!=f.hash)return false;
  std::ifstream input(directory/"sys/fst.bin",std::ios::binary);if(!input)return false;
  Bytes fst((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());if(fst.size()<12||fst.size()>16u*1024*1024)return false;
  uint32_t total=U32(fst.data()+8);if(!total||uint64_t(total)*12>fst.size())return false;
  std::vector<std::pair<uint32_t,fs::path>> stack{{total,{}}};
  for(uint32_t i=1;i<total;++i){auto* e=fst.data()+12*i;while(!stack.empty()&&i>=stack.back().first)stack.pop_back();if(stack.empty())return false;
   size_t off=size_t(total)*12+(U32(e)&0xFFFFFF);if(off>=fst.size())return false;auto end=std::find(fst.begin()+off,fst.end(),uint8_t(0));if(end==fst.end())return false;
   std::string name(fst.begin()+off,end);if(name.empty()||name=="."||name==".."||name.find_first_of("/\\:")!=std::string::npos)return false;
   auto relative=stack.back().second/fs::u8path(name);
   if(e[0]){uint32_t until=U32(e+8);if(until<=i||until>stack.back().first)return false;stack.push_back({until,relative});}
   else {auto path=directory/"files"/relative;if(!fs::is_regular_file(path)||fs::file_size(path)!=U32(e+8))return false;}
  }
  return fs::is_regular_file(directory/"sys/boot.bin")&&fs::file_size(directory/"sys/boot.bin")>=0x440;
 }catch(...){return false;}
}
fs::path ImportImage(const fs::path& image,const fs::path& finalDestination,Progress progress){
 const fs::path destination=fs::u8path(finalDestination.u8string()+".importing");
 if(ValidateTree(finalDestination)&&fs::is_regular_file(finalDestination/"star-successor-import.complete"))return finalDestination;
 Need(!fs::exists(finalDestination)||fs::is_empty(finalDestination),"O destino ja contem arquivos; escolha outra pasta");
 if(fs::exists(destination)){Need(fs::is_regular_file(destination/"star-successor-import.pending"),"Pasta temporaria de importacao ja existe");fs::remove_all(destination);}
 fs::create_directories(destination);Write(destination/"star-successor-import.pending",Bytes{'R','2','V','E','0','1'});
 Image disc(image);auto boot=disc.Read(0,0x440);uint64_t dol=uint64_t(U32(boot.data()+0x420))<<2,fstOff=uint64_t(U32(boot.data()+0x424))<<2;uint32_t fstSize=U32(boot.data()+0x428)<<2;Need(fstSize>=12&&fstSize<=16u*1024*1024,"FST invalida");auto fst=disc.Read(fstOff,fstSize);uint32_t total=U32(fst.data()+8);Need(total>0&&uint64_t(total)*12<=fst.size(),"Entradas da FST invalidas");
 auto dolHeader=disc.Read(dol,0x100);uint64_t dolSize=0;for(int i=0;i<18;++i)if(U32(dolHeader.data()+0x90+4*i))dolSize=std::max(dolSize,uint64_t(U32(dolHeader.data()+4*i))+U32(dolHeader.data()+0x90+4*i));Need(dolSize<=32u*1024*1024,"DOL invalido");auto appHeader=disc.Read(0x2440,0x20);uint64_t appSize=0x20ull+U32(appHeader.data()+0x14)+U32(appHeader.data()+0x18);Need(appSize<=4u*1024*1024,"Apploader invalido");
 Write(destination/"disc/header.bin",disc.header);Write(destination/"disc/region.bin",disc.region);Write(destination/"ticket.bin",disc.ticket);Write(destination/"tmd.bin",disc.tmd);Write(destination/"sys/boot.bin",boot);Write(destination/"sys/bi2.bin",disc.Read(0x440,0x2000));Write(destination/"sys/fst.bin",fst);Write(destination/"sys/main.dol",disc.Read(dol,dolSize));Write(destination/"sys/apploader.img",disc.Read(0x2440,appSize));
 std::vector<std::pair<uint32_t,fs::path>> stack{{total,{}}};size_t files=0;
 for(uint32_t i=1;i<total;++i){const uint8_t* e=fst.data()+12*i;while(!stack.empty()&&i>=stack.back().first)stack.pop_back();Need(!stack.empty(),"Diretorios FST invalidos");size_t nameOff=size_t(total)*12+(U32(e)&0xFFFFFF);Need(nameOff<fst.size(),"Nome FST invalido");auto end=std::find(fst.begin()+nameOff,fst.end(),uint8_t(0));Need(end!=fst.end(),"Nome FST truncado");std::string name(fst.begin()+nameOff,end);Need(!name.empty()&&name!="."&&name!=".."&&name.find_first_of("/\\:")==std::string::npos,"Nome de arquivo inseguro na FST");auto relative=stack.back().second/fs::u8path(name);
  if(e[0]){uint32_t until=U32(e+8);Need(until>i&&until<=stack.back().first,"Intervalo de diretorio FST invalido");stack.push_back({until,relative});continue;}
  uint64_t off=uint64_t(U32(e+4))<<2,left=U32(e+8);fs::path path=destination/"files"/relative;fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);Need(bool(out),"Nao foi possivel criar um arquivo do jogo");while(left){size_t take=std::min<uint64_t>(left,1<<20);auto bytes=disc.Read(off,take);out.write((const char*)bytes.data(),bytes.size());Need(bool(out),"Falha ao gravar arquivo do jogo");off+=take;left-=take;}++files;if(progress)progress(i,total,name);
 }
 Need(ValidateTree(destination),"Os arquivos de codigo diferem da versao USA suportada");Write(destination/"star-successor-import.complete",Bytes{'R','2','V','E','0','1','\n'});fs::remove(destination/"star-successor-import.pending");if(fs::exists(finalDestination))fs::remove(finalDestination);fs::rename(destination,finalDestination);return finalDestination;
}
} // namespace StarDisc
