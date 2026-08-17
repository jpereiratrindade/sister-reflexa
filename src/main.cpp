// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

struct Event { std::string id,title,type,valid_time,context,registered_at; };
struct Claim { std::string id,event_id,text,registered_at; };
struct Evidence { std::string id,claim_id,kind,valid_time,source_ref,digest,content,registered_at; };
struct Relation { std::string id,from_id,relation,to_id,note,valid_time,registered_at; };
struct Evaluation {
    std::string id,event_id,verdict,t,e,d,k,a,r,p,f,uncertainty,summary,rubric,registered_at;
};

std::string now_iso() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::string pct_encode(std::string_view in) {
    std::ostringstream out;
    out << std::uppercase << std::hex;
    for (unsigned char c : in) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c=='-' || c=='_' || c=='.' || c=='~') {
            out << static_cast<char>(c);
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return out.str();
}

int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

std::string pct_decode(std::string_view in, bool plus_space=false) {
    std::string out;
    out.reserve(in.size());
    for (size_t i=0; i<in.size(); ++i) {
        if (plus_space && in[i] == '+') { out.push_back(' '); continue; }
        if (in[i]=='%' && i+2<in.size()) {
            const int hi=hexval(in[i+1]), lo=hexval(in[i+2]);
            if (hi>=0 && lo>=0) { out.push_back(static_cast<char>((hi<<4)|lo)); i+=2; continue; }
        }
        out.push_back(in[i]);
    }
    return out;
}

std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> v;
    size_t start=0;
    while (true) {
        const auto pos=line.find('\t', start);
        v.push_back(pct_decode(line.substr(start, pos==std::string::npos ? std::string::npos : pos-start)));
        if (pos==std::string::npos) break;
        start=pos+1;
    }
    return v;
}

std::string json_escape(std::string_view in) {
    std::ostringstream o;
    for (unsigned char c : in) {
        switch(c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (c < 0x20) o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
                else o << static_cast<char>(c);
        }
    }
    return o.str();
}

std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + p.string());
    std::ostringstream s; s << in.rdbuf(); return s.str();
}

void ensure_file(const fs::path& p) {
    fs::create_directories(p.parent_path());
    if (!fs::exists(p)) { std::ofstream out(p); }
}

class Store {
public:
    explicit Store(fs::path root): root_(std::move(root)) {
        fs::create_directories(root_);
        for (auto n : {"events.tsv","claims.tsv","evidence.tsv","relations.tsv","evaluations.tsv"}) ensure_file(root_/n);
    }

    std::string add_event(const std::map<std::string,std::string>& f) {
        auto id=next_id("events.tsv","evt");
        append("events.tsv", {id,get(f,"title"),get(f,"type"),get(f,"valid_time"),get(f,"context"),now_iso()});
        return id;
    }
    std::string add_claim(const std::map<std::string,std::string>& f) {
        auto id=next_id("claims.tsv","clm");
        append("claims.tsv", {id,get(f,"event_id"),get(f,"text"),now_iso()}); return id;
    }
    std::string add_evidence(const std::map<std::string,std::string>& f) {
        auto id=next_id("evidence.tsv","evd");
        append("evidence.tsv", {id,get(f,"claim_id"),get(f,"kind"),get(f,"valid_time"),get(f,"source_ref"),get(f,"digest"),get(f,"content"),now_iso()}); return id;
    }
    std::string add_relation(const std::map<std::string,std::string>& f) {
        auto id=next_id("relations.tsv","rel");
        append("relations.tsv", {id,get(f,"from_id"),get(f,"relation"),get(f,"to_id"),get(f,"note"),get(f,"valid_time"),now_iso()}); return id;
    }
    std::string add_evaluation(const std::map<std::string,std::string>& f) {
        auto id=next_id("evaluations.tsv","eva");
        append("evaluations.tsv", {id,get(f,"event_id"),get(f,"verdict"),get(f,"t"),get(f,"e"),get(f,"d"),get(f,"k"),get(f,"a"),get(f,"r"),get(f,"p"),get(f,"f"),get(f,"uncertainty"),get(f,"summary"),"rubric-0.1",now_iso()}); return id;
    }

    std::vector<Event> events() const {
        std::vector<Event> out;
        for (auto& v: rows("events.tsv")) if(v.size()==6) out.push_back({v[0],v[1],v[2],v[3],v[4],v[5]});
        return out;
    }
    std::vector<Claim> claims() const {
        std::vector<Claim> out;
        for (auto& v: rows("claims.tsv")) { if(v.size()==4) out.push_back({v[0],v[1],v[2],v[3]}); }
        return out;
    }
    std::vector<Evidence> evidence() const {
        std::vector<Evidence> out;
        for (auto& v: rows("evidence.tsv")) { if(v.size()==8) out.push_back({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7]}); }
        return out;
    }
    std::vector<Relation> relations() const {
        std::vector<Relation> out;
        for (auto& v: rows("relations.tsv")) { if(v.size()==7) out.push_back({v[0],v[1],v[2],v[3],v[4],v[5],v[6]}); }
        return out;
    }
    std::vector<Evaluation> evaluations() const {
        std::vector<Evaluation> out;
        for (auto& v: rows("evaluations.tsv")) { if(v.size()==15) out.push_back({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13],v[14]}); }
        return out;
    }

    std::string state_json() const {
        std::ostringstream o;
        o << "{\"system\":{\"name\":\"SisTer Reflexa\",\"version\":\"" << SISTER_REFLEXA_VERSION
          << "\",\"score_policy\":\"NO_AGGREGATE_SCORE\",\"model\":\"NOT_TRAINED\"},";
        auto es=events(); o << "\"events\":[";
        for(size_t i=0;i<es.size();++i){ if(i)o<<','; auto& x=es[i]; o<<"{\"id\":\""<<json_escape(x.id)<<"\",\"title\":\""<<json_escape(x.title)<<"\",\"type\":\""<<json_escape(x.type)<<"\",\"valid_time\":\""<<json_escape(x.valid_time)<<"\",\"context\":\""<<json_escape(x.context)<<"\",\"registered_at\":\""<<json_escape(x.registered_at)<<"\"}"; }
        o << "],\"claims\":["; auto cs=claims();
        for(size_t i=0;i<cs.size();++i){if(i)o<<',';auto&x=cs[i];o<<"{\"id\":\""<<json_escape(x.id)<<"\",\"event_id\":\""<<json_escape(x.event_id)<<"\",\"text\":\""<<json_escape(x.text)<<"\",\"registered_at\":\""<<json_escape(x.registered_at)<<"\"}";}
        o << "],\"evidence\":["; auto vs=evidence();
        for(size_t i=0;i<vs.size();++i){if(i)o<<',';auto&x=vs[i];o<<"{\"id\":\""<<json_escape(x.id)<<"\",\"claim_id\":\""<<json_escape(x.claim_id)<<"\",\"kind\":\""<<json_escape(x.kind)<<"\",\"valid_time\":\""<<json_escape(x.valid_time)<<"\",\"source_ref\":\""<<json_escape(x.source_ref)<<"\",\"digest\":\""<<json_escape(x.digest)<<"\",\"content\":\""<<json_escape(x.content)<<"\",\"registered_at\":\""<<json_escape(x.registered_at)<<"\"}";}
        o << "],\"relations\":["; auto rs=relations();
        for(size_t i=0;i<rs.size();++i){if(i)o<<',';auto&x=rs[i];o<<"{\"id\":\""<<json_escape(x.id)<<"\",\"from_id\":\""<<json_escape(x.from_id)<<"\",\"relation\":\""<<json_escape(x.relation)<<"\",\"to_id\":\""<<json_escape(x.to_id)<<"\",\"note\":\""<<json_escape(x.note)<<"\",\"valid_time\":\""<<json_escape(x.valid_time)<<"\",\"registered_at\":\""<<json_escape(x.registered_at)<<"\"}";}
        o << "],\"evaluations\":["; auto ev=evaluations();
        for(size_t i=0;i<ev.size();++i){if(i)o<<',';auto&x=ev[i];o<<"{\"id\":\""<<json_escape(x.id)<<"\",\"event_id\":\""<<json_escape(x.event_id)<<"\",\"verdict\":\""<<json_escape(x.verdict)<<"\",\"T\":\""<<json_escape(x.t)<<"\",\"E\":\""<<json_escape(x.e)<<"\",\"D\":\""<<json_escape(x.d)<<"\",\"K\":\""<<json_escape(x.k)<<"\",\"A\":\""<<json_escape(x.a)<<"\",\"R\":\""<<json_escape(x.r)<<"\",\"P\":\""<<json_escape(x.p)<<"\",\"F\":\""<<json_escape(x.f)<<"\",\"uncertainty\":\""<<json_escape(x.uncertainty)<<"\",\"summary\":\""<<json_escape(x.summary)<<"\",\"rubric\":\""<<json_escape(x.rubric)<<"\",\"registered_at\":\""<<json_escape(x.registered_at)<<"\"}";}
        o << "]}";
        return o.str();
    }

private:
    fs::path root_;
    static std::string get(const std::map<std::string,std::string>& f, const std::string& k) {
        auto it=f.find(k); return it==f.end()?"":it->second;
    }
    void append(const std::string& name, const std::vector<std::string>& fields) {
        std::ofstream out(root_/name, std::ios::app);
        if(!out) throw std::runtime_error("cannot append " + name);
        for(size_t i=0;i<fields.size();++i){ if(i) out << '\t'; out << pct_encode(fields[i]); }
        out << '\n'; out.flush();
    }
    std::vector<std::vector<std::string>> rows(const std::string& name) const {
        std::vector<std::vector<std::string>> out; std::ifstream in(root_/name); std::string line;
        while(std::getline(in,line)){ if(!line.empty()) out.push_back(split_tab(line)); }
        return out;
    }
    std::string next_id(const std::string& name, const std::string& prefix) const {
        const auto n=rows(name).size()+1; std::ostringstream o; o<<prefix<<'-'<<std::setw(6)<<std::setfill('0')<<n; return o.str();
    }
};

std::map<std::string,std::string> parse_form(std::string_view body) {
    std::map<std::string,std::string> out; size_t start=0;
    while(start<=body.size()){
        auto amp=body.find('&',start); auto part=body.substr(start, amp==std::string_view::npos ? body.size()-start : amp-start);
        auto eq=part.find('=');
        auto k=pct_decode(part.substr(0,eq),true); auto v=eq==std::string_view::npos?"":pct_decode(part.substr(eq+1),true);
        if(!k.empty()) out[k]=v;
        if(amp==std::string_view::npos) break;
        start=amp+1;
    }
    return out;
}

struct Request { std::string method,path,body; };

std::optional<Request> receive_request(int fd) {
    std::string data; std::array<char,4096> buf{};
    while(data.find("\r\n\r\n")==std::string::npos && data.size()<1024*1024){ auto n=::recv(fd,buf.data(),buf.size(),0); if(n<=0)return std::nullopt; data.append(buf.data(),static_cast<size_t>(n)); }
    auto end=data.find("\r\n\r\n"); if(end==std::string::npos) return std::nullopt;
    auto header=data.substr(0,end); std::istringstream hs(header); std::string method,path,version; hs>>method>>path>>version;
    size_t content_length=0; std::string line; std::getline(hs,line);
    while(std::getline(hs,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        auto colon=line.find(':'); if(colon==std::string::npos)continue;
        auto key=line.substr(0,colon); std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(key=="content-length") content_length=static_cast<size_t>(std::stoul(line.substr(colon+1)));
    }
    std::string body=data.substr(end+4);
    while(body.size()<content_length){ auto n=::recv(fd,buf.data(),buf.size(),0); if(n<=0)break; body.append(buf.data(),static_cast<size_t>(n)); }
    if(body.size()>content_length)body.resize(content_length);
    auto q=path.find('?'); if(q!=std::string::npos) path.resize(q);
    return Request{method,path,body};
}

void send_response(int fd, int status, std::string_view reason, std::string_view type, const std::string& body) {
    std::ostringstream h; h<<"HTTP/1.1 "<<status<<' '<<reason<<"\r\nContent-Type: "<<type<<"\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n";
    auto head=h.str(); ::send(fd,head.data(),head.size(),0); ::send(fd,body.data(),body.size(),0);
}

std::string content_type(const fs::path& p) {
    auto e=p.extension().string(); if(e==".html")return"text/html; charset=utf-8"; if(e==".js")return"application/javascript; charset=utf-8"; if(e==".css")return"text/css; charset=utf-8"; return"text/plain; charset=utf-8";
}

int run_server(const fs::path& project_root, const std::string& host, int port) {
    Store store(project_root/"data");
    int s=::socket(AF_INET,SOCK_STREAM,0); if(s<0) throw std::runtime_error("socket failed");
    int yes=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(static_cast<uint16_t>(port));
    if(inet_pton(AF_INET,host.c_str(),&addr.sin_addr)!=1) throw std::runtime_error("invalid host");
    if(::bind(s,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0){ auto e=errno; close(s); throw std::runtime_error("bind failed errno="+std::to_string(e)); }
    if(::listen(s,16)<0){ close(s); throw std::runtime_error("listen failed"); }
    std::cout<<"SisTer Reflexa MVP-0\nhttp://"<<host<<':'<<port<<"\nlocal-only; Ctrl+C to stop\n";
    while(true){
        int c=::accept(s,nullptr,nullptr); if(c<0){ if(errno==EINTR)continue; break; }
        try {
            auto req=receive_request(c); if(!req){ close(c); continue; }
            if(req->method=="GET" && req->path=="/api/health") send_response(c,200,"OK","application/json; charset=utf-8","{\"status\":\"READY\",\"completeness\":\"INCOMPLETE\"}");
            else if(req->method=="GET" && req->path=="/api/state") send_response(c,200,"OK","application/json; charset=utf-8",store.state_json());
            else if(req->method=="GET" && req->path=="/api/model/status") send_response(c,200,"OK","application/json; charset=utf-8","{\"model\":\"TinyReflexiveLM\",\"status\":\"NOT_TRAINED\",\"authority\":\"NONE\",\"note\":\"MVP-0 collects adjudicable evaluation snapshots; it does not fake a model.\"}");
            else if(req->method=="POST" && req->path.rfind("/api/",0)==0){
                auto f=parse_form(req->body); std::string id;
                if(req->path=="/api/events") id=store.add_event(f);
                else if(req->path=="/api/claims") id=store.add_claim(f);
                else if(req->path=="/api/evidence") id=store.add_evidence(f);
                else if(req->path=="/api/relations") id=store.add_relation(f);
                else if(req->path=="/api/evaluations") id=store.add_evaluation(f);
                else { send_response(c,404,"Not Found","application/json","{\"error\":\"unknown endpoint\"}"); close(c); continue; }
                send_response(c,201,"Created","application/json; charset=utf-8","{\"id\":\""+json_escape(id)+"\",\"append_only\":true}");
            } else if(req->method=="GET") {
                fs::path rel=req->path=="/"?"index.html":req->path.substr(1);
                if(rel.string().find("..")!=std::string::npos){ send_response(c,400,"Bad Request","text/plain","bad path"); }
                else { fs::path p=project_root/"web"/rel; if(fs::exists(p)&&fs::is_regular_file(p)) send_response(c,200,"OK",content_type(p),read_file(p)); else send_response(c,404,"Not Found","text/plain","not found"); }
            } else send_response(c,405,"Method Not Allowed","text/plain","method not allowed");
        } catch(const std::exception& e){ send_response(c,500,"Internal Server Error","application/json","{\"error\":\""+json_escape(e.what())+"\"}"); }
        close(c);
    }
    close(s); return 0;
}

int self_test() {
    auto root=fs::temp_directory_path()/"sister-reflexa-self-test"; fs::remove_all(root); Store s(root);
    auto evt=s.add_event({{"title","Dia de campo"},{"type","field_day"},{"valid_time","2026-08-17"},{"context","pratica inicial"}});
    auto clm=s.add_claim({{"event_id",evt},{"text","pratica alterada"}});
    auto evd=s.add_evidence({{"claim_id",clm},{"kind","document"},{"valid_time","2026-08-18"},{"source_ref","ata"},{"digest","sha256:test"},{"content","registro posterior"}});
    (void)evd;
    auto eva=s.add_evaluation({{"event_id",evt},{"verdict","PARTIAL"},{"t","HIGH"},{"e","HIGH"},{"d","MED"},{"k","LOW"},{"a","MED"},{"r","LOW"},{"p","NA"},{"f","LOW"},{"uncertainty","HIGH"},{"summary","primeiro snapshot"}});
    (void)eva;
    if(s.events().size()!=1 || s.claims().size()!=1 || s.evidence().size()!=1 || s.evaluations().size()!=1) return 2;
    auto j=s.state_json(); if(j.find("NO_AGGREGATE_SCORE")==std::string::npos || j.find("PARTIAL")==std::string::npos) return 3;
    Store again(root); if(again.events().size()!=1) return 4;
    fs::remove_all(root); std::cout<<"self-test PASS\n"; return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if(argc==2 && std::string_view(argv[1])=="--status") {
            std::cout<<"SisTer Reflexa\ncore=READY\ncompleteness=INCOMPLETE\nphase=MVP-0\nmodel=NOT_TRAINED\nscore_policy=NO_AGGREGATE_SCORE\n"; return 0;
        }
        if(argc==2 && std::string_view(argv[1])=="--self-test") return self_test();
        if(argc>=2 && std::string_view(argv[1])=="--serve") {
            fs::path root=fs::current_path(); std::string host="127.0.0.1"; int port=8092;
            if(argc>=3) host=argv[2];
            if(argc>=4) port=std::stoi(argv[3]);
            return run_server(root,host,port);
        }
        std::cout<<"Usage:\n  sister-reflexa --status\n  sister-reflexa --self-test\n  sister-reflexa --serve [host] [port]\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"ERROR: "<<e.what()<<'\n'; return 1; }
}
