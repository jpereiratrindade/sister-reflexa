// SPDX-License-Identifier: GPL-3.0-or-later
// AG-RFX-MVP0-001: Minimal Vertical Slice — EvidenceBundle + Process Vector
// Evaluator: deterministic-baseline v0.1 (PROVISIONAL / NOT SCIENTIFICALLY VALIDATED)

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cmath>
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
#include <cstring>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

// ---------------------------------------------------------------------------
// Domain structs
// ---------------------------------------------------------------------------

struct Event { std::string id,title,type,valid_time,context,registered_at; };
struct Claim { std::string id,event_id,text,registered_at; };

// Evidence: 9 fields (backward-compat: contributions may be empty)
struct Evidence {
    std::string id,claim_id,kind,valid_time,source_ref,digest,content,registered_at,contributions;
};

struct Relation { std::string id,from_id,relation,to_id,note,valid_time,registered_at; };

// Human qualitative evaluation (existing — unchanged)
struct Evaluation {
    std::string id,event_id,verdict,t,e,d,k,a,r,p,f,uncertainty,summary,rubric,registered_at;
};

// New: EvidenceBundle — frozen snapshot of evidence at a point in time
// snapshot field stores serialised evidence content (inline, frozen at creation time)
struct EvidenceBundle {
    std::string bundle_id,event_id,created_at;
    std::string evidence_count; // stored as string for TSV simplicity
    std::string content_digest; // FNV-1a hex of snapshot
    std::string snapshot;       // serialised frozen evidence (percent-encoded block)
};

// New: Assessment — deterministic-baseline result bound to a bundle
struct Assessment {
    std::string assessment_id,bundle_id,event_id,evaluator,assessed_at;
    // 7 process vector dimensions (stored as strings, 6 decimal places)
    std::string exposure,interaction,appropriation,incorporation,propagation,reflexivity,stabilization;
    std::string status; // "experimental"
};

// ---------------------------------------------------------------------------
// Process vector dimensions (the 7 provisional dimensions)
// ---------------------------------------------------------------------------

static constexpr std::array<std::string_view,7> DIMS = {
    "exposure","interaction","appropriation","incorporation",
    "propagation","reflexivity","stabilization"
};

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

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

// FNV-1a 64-bit hash — deterministic, not cryptographic, used as content digest
uint64_t fnv1a(std::string_view data) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : data) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string digest_hex(std::string_view data) {
    std::ostringstream o;
    o << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << fnv1a(data);
    return o.str();
}

// ---------------------------------------------------------------------------
// Contribution parsing
// Contributions stored as "dim:support:confidence,dim:support:confidence,..."
// e.g. "exposure:0.8:0.9,interaction:-0.2:0.5"
// ---------------------------------------------------------------------------

struct Contribution { std::string dim; double support; double confidence; };

std::vector<Contribution> parse_contributions(const std::string& s) {
    std::vector<Contribution> out;
    if (s.empty()) return out;
    size_t pos=0;
    while (pos<s.size()) {
        auto comma = s.find(',', pos);
        auto item = (comma==std::string::npos) ? s.substr(pos) : s.substr(pos, comma-pos);
        auto c1 = item.find(':');
        if (c1!=std::string::npos) {
            auto c2 = item.find(':', c1+1);
            if (c2!=std::string::npos) {
                try {
                    Contribution c;
                    c.dim = item.substr(0, c1);
                    c.support = std::stod(item.substr(c1+1, c2-c1-1));
                    c.confidence = std::stod(item.substr(c2+1));
                    c.support = std::clamp(c.support, -1.0, 1.0);
                    c.confidence = std::clamp(c.confidence, 0.0, 1.0);
                    out.push_back(std::move(c));
                } catch(...) { /* skip malformed */ }
            }
        }
        if (comma==std::string::npos) break;
        pos = comma+1;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Serialise a frozen evidence snapshot
// Format: one evidence item per line: id|content|contributions
// This is frozen at bundle creation time — independent of future mutations
// ---------------------------------------------------------------------------

std::string serialise_snapshot(const std::vector<Evidence>& items) {
    std::ostringstream o;
    for (auto& e : items) {
        o << pct_encode(e.id) << '|' << pct_encode(e.content) << '|' << pct_encode(e.contributions) << '\n';
    }
    return o.str();
}

// ---------------------------------------------------------------------------
// Deterministic baseline evaluator v0.1
// PROVISIONAL / EXPERIMENTAL / NOT SCIENTIFICALLY VALIDATED
//
// For each dimension d:
//   vector[d] = clamp(0,1, 0.5 + Σ(support_i * confidence_i) / max(1, n_d))
// where n_d = count of contributions for dimension d.
// If n_d = 0: vector[d] = 0.0 (no information on this dimension).
// ---------------------------------------------------------------------------

std::map<std::string,double> evaluate_snapshot(const std::string& snapshot) {
    // Parse snapshot lines: pct_encoded(id)|pct_encoded(content)|pct_encoded(contributions)
    std::map<std::string,double> sums;
    std::map<std::string,int> counts;
    for (auto d : DIMS) { sums[std::string(d)]=0.0; counts[std::string(d)]=0; }

    std::istringstream ss(snapshot);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        auto p1 = line.find('|');
        auto p2 = (p1!=std::string::npos) ? line.find('|', p1+1) : std::string::npos;
        if (p1==std::string::npos || p2==std::string::npos) continue;
        auto contrib_enc = line.substr(p2+1);
        auto contrib_str = pct_decode(contrib_enc);
        auto contribs = parse_contributions(contrib_str);
        for (auto& c : contribs) {
            if (sums.count(c.dim)) {
                sums[c.dim] += c.support * c.confidence;
                counts[c.dim]++;
            }
        }
    }

    std::map<std::string,double> vec;
    for (auto d : DIMS) {
        auto ds = std::string(d);
        int n = counts[ds];
        if (n == 0) {
            vec[ds] = 0.0;
        } else {
            vec[ds] = std::clamp(0.5 + sums[ds] / static_cast<double>(n), 0.0, 1.0);
        }
    }
    return vec;
}

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

class Store {
public:
    explicit Store(fs::path root): root_(std::move(root)) {
        fs::create_directories(root_);
        for (auto n : {"events.tsv","claims.tsv","evidence.tsv","relations.tsv",
                       "evaluations.tsv","bundles.tsv","assessments.tsv"})
            ensure_file(root_/n);
    }

    // --- existing add methods (unchanged) ---

    std::string add_event(const std::map<std::string,std::string>& f) {
        auto id=next_id("events.tsv","evt");
        append("events.tsv", {id,get(f,"title"),get(f,"type"),get(f,"valid_time"),get(f,"context"),now_iso()});
        return id;
    }
    std::string add_claim(const std::map<std::string,std::string>& f) {
        auto id=next_id("claims.tsv","clm");
        append("claims.tsv", {id,get(f,"event_id"),get(f,"text"),now_iso()}); return id;
    }
    // Evidence now has 9 fields (backward-compat: contributions optional)
    std::string add_evidence(const std::map<std::string,std::string>& f) {
        auto id=next_id("evidence.tsv","evd");
        append("evidence.tsv", {id,get(f,"claim_id"),get(f,"kind"),get(f,"valid_time"),
            get(f,"source_ref"),get(f,"digest"),get(f,"content"),now_iso(),get(f,"contributions")});
        return id;
    }
    std::string add_relation(const std::map<std::string,std::string>& f) {
        auto id=next_id("relations.tsv","rel");
        append("relations.tsv", {id,get(f,"from_id"),get(f,"relation"),get(f,"to_id"),
            get(f,"note"),get(f,"valid_time"),now_iso()}); return id;
    }
    std::string add_evaluation(const std::map<std::string,std::string>& f) {
        auto id=next_id("evaluations.tsv","eva");
        append("evaluations.tsv", {id,get(f,"event_id"),get(f,"verdict"),
            get(f,"t"),get(f,"e"),get(f,"d"),get(f,"k"),get(f,"a"),get(f,"r"),get(f,"p"),get(f,"f"),
            get(f,"uncertainty"),get(f,"summary"),"rubric-0.1",now_iso()}); return id;
    }

    // --- new: EvidenceBundle ---

    // Freeze a bundle: snapshot all evidence for this event right now.
    // The snapshot is stored inline — future evidence mutations do not affect it.
    std::string freeze_bundle(const std::string& event_id) {
        // Collect all evidence for this event (via claims)
        auto all_claims = claims();
        auto all_evidence = evidence();
        std::vector<Evidence> event_evidence;
        for (auto& c : all_claims) {
            if (c.event_id != event_id) continue;
            for (auto& ev : all_evidence) {
                if (ev.claim_id == c.id) event_evidence.push_back(ev);
            }
        }
        // Also allow direct event_id on evidence (for MVP simplicity)
        // In this MVP, evidence is linked via claim_id only. That is fine.

        auto snapshot = serialise_snapshot(event_evidence);
        auto digest   = digest_hex(snapshot);
        auto id       = next_id("bundles.tsv","bnd");
        auto count    = std::to_string(event_evidence.size());
        // Store snapshot percent-encoded as single field
        append("bundles.tsv", {id, event_id, now_iso(), count, digest, snapshot});
        return id;
    }

    // Assess a bundle using the deterministic-baseline evaluator
    std::string assess_bundle(const std::string& bundle_id) {
        auto bundles_all = bundles();
        const EvidenceBundle* bnd = nullptr;
        for (auto& b : bundles_all) { if (b.bundle_id==bundle_id) { bnd=&b; break; } }
        if (!bnd) throw std::runtime_error("bundle not found: " + bundle_id);

        auto vec = evaluate_snapshot(bnd->snapshot);
        auto id  = next_id("assessments.tsv","asr");
        auto fmt = [](double v){ std::ostringstream o; o<<std::fixed<<std::setprecision(6)<<v; return o.str(); };
        append("assessments.tsv", {id, bundle_id, bnd->event_id,
            "deterministic-baseline-v0.1", now_iso(),
            fmt(vec["exposure"]), fmt(vec["interaction"]), fmt(vec["appropriation"]),
            fmt(vec["incorporation"]), fmt(vec["propagation"]),
            fmt(vec["reflexivity"]), fmt(vec["stabilization"]),
            "experimental"});
        return id;
    }

    // --- read methods ---

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
        for (auto& v: rows("evidence.tsv")) {
            if(v.size()==8) out.push_back({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],""});
            else if(v.size()>=9) out.push_back({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8]});
        }
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
    std::vector<EvidenceBundle> bundles() const {
        std::vector<EvidenceBundle> out;
        for (auto& v: rows("bundles.tsv")) {
            if(v.size()>=6) out.push_back({v[0],v[1],v[2],v[3],v[4],v[5]});
        }
        return out;
    }
    std::vector<Assessment> assessments() const {
        std::vector<Assessment> out;
        for (auto& v: rows("assessments.tsv")) {
            if(v.size()>=13) {
                Assessment a;
                a.assessment_id=v[0]; a.bundle_id=v[1]; a.event_id=v[2];
                a.evaluator=v[3]; a.assessed_at=v[4];
                a.exposure=v[5]; a.interaction=v[6]; a.appropriation=v[7];
                a.incorporation=v[8]; a.propagation=v[9];
                a.reflexivity=v[10]; a.stabilization=v[11];
                a.status=v.size()>=13?v[12]:"experimental";
                out.push_back(std::move(a));
            }
        }
        return out;
    }

    // --- JSON serialisation ---

    std::string state_json() const {
        std::ostringstream o;
        o << "{\"system\":{\"name\":\"SisTer Reflexa\",\"version\":\"" << SISTER_REFLEXA_VERSION
          << "\",\"score_policy\":\"NO_AGGREGATE_SCORE\",\"model\":\"NOT_TRAINED\"},";
        auto es=events(); o << "\"events\":[";
        for(size_t i=0;i<es.size();++i){ if(i)o<<','; auto& x=es[i]; o<<"{\"id\":\""<<json_escape(x.id)<<"\",\"title\":\""<<json_escape(x.title)<<"\",\"type\":\""<<json_escape(x.type)<<"\",\"valid_time\":\""<<json_escape(x.valid_time)<<"\",\"context\":\""<<json_escape(x.context)<<"\",\"registered_at\":\""<<json_escape(x.registered_at)<<"\"}"; }
        o << "],\"claims\":["; auto cs=claims();
        for(size_t i=0;i<cs.size();++i){if(i)o<<',';auto& x=cs[i];o<<"{\"id\":\""<<json_escape(x.id)<<"\",\"event_id\":\""<<json_escape(x.event_id)<<"\",\"text\":\""<<json_escape(x.text)<<"\",\"registered_at\":\""<<json_escape(x.registered_at)<<"\"}";}
        o << "],\"evidence\":["; auto vs=evidence();
        for(size_t i=0;i<vs.size();++i){if(i)o<<',';auto& x=vs[i];o<<"{\"id\":\""<<json_escape(x.id)<<"\",\"claim_id\":\""<<json_escape(x.claim_id)<<"\",\"kind\":\""<<json_escape(x.kind)<<"\",\"valid_time\":\""<<json_escape(x.valid_time)<<"\",\"source_ref\":\""<<json_escape(x.source_ref)<<"\",\"digest\":\""<<json_escape(x.digest)<<"\",\"content\":\""<<json_escape(x.content)<<"\",\"contributions\":\""<<json_escape(x.contributions)<<"\",\"registered_at\":\""<<json_escape(x.registered_at)<<"\"}";}
        o << "],\"relations\":["; auto rs=relations();
        for(size_t i=0;i<rs.size();++i){if(i)o<<',';auto& x=rs[i];o<<"{\"id\":\""<<json_escape(x.id)<<"\",\"from_id\":\""<<json_escape(x.from_id)<<"\",\"relation\":\""<<json_escape(x.relation)<<"\",\"to_id\":\""<<json_escape(x.to_id)<<"\",\"note\":\""<<json_escape(x.note)<<"\",\"valid_time\":\""<<json_escape(x.valid_time)<<"\",\"registered_at\":\""<<json_escape(x.registered_at)<<"\"}";}
        o << "],\"evaluations\":["; auto ev=evaluations();
        for(size_t i=0;i<ev.size();++i){if(i)o<<',';auto& x=ev[i];o<<"{\"id\":\""<<json_escape(x.id)<<"\",\"event_id\":\""<<json_escape(x.event_id)<<"\",\"verdict\":\""<<json_escape(x.verdict)<<"\",\"T\":\""<<json_escape(x.t)<<"\",\"E\":\""<<json_escape(x.e)<<"\",\"D\":\""<<json_escape(x.d)<<"\",\"K\":\""<<json_escape(x.k)<<"\",\"A\":\""<<json_escape(x.a)<<"\",\"R\":\""<<json_escape(x.r)<<"\",\"P\":\""<<json_escape(x.p)<<"\",\"F\":\""<<json_escape(x.f)<<"\",\"uncertainty\":\""<<json_escape(x.uncertainty)<<"\",\"summary\":\""<<json_escape(x.summary)<<"\",\"rubric\":\""<<json_escape(x.rubric)<<"\",\"registered_at\":\""<<json_escape(x.registered_at)<<"\"}";}
        o << "],\"bundles\":["; auto bs=bundles();
        for(size_t i=0;i<bs.size();++i){if(i)o<<',';auto& x=bs[i];o<<"{\"bundle_id\":\""<<json_escape(x.bundle_id)<<"\",\"event_id\":\""<<json_escape(x.event_id)<<"\",\"created_at\":\""<<json_escape(x.created_at)<<"\",\"evidence_count\":"<<json_escape(x.evidence_count)<<",\"content_digest\":\""<<json_escape(x.content_digest)<<"\"}";}
        o << "],\"assessments\":["; auto as=assessments();
        for(size_t i=0;i<as.size();++i){if(i)o<<',';auto& x=as[i];o<<"{\"assessment_id\":\""<<json_escape(x.assessment_id)<<"\",\"bundle_id\":\""<<json_escape(x.bundle_id)<<"\",\"event_id\":\""<<json_escape(x.event_id)<<"\",\"evaluator\":\""<<json_escape(x.evaluator)<<"\",\"assessed_at\":\""<<json_escape(x.assessed_at)<<"\",\"vector\":{\"exposure\":"<<x.exposure<<",\"interaction\":"<<x.interaction<<",\"appropriation\":"<<x.appropriation<<",\"incorporation\":"<<x.incorporation<<",\"propagation\":"<<x.propagation<<",\"reflexivity\":"<<x.reflexivity<<",\"stabilization\":"<<x.stabilization<<"},\"status\":\""<<json_escape(x.status)<<"\"}";}
        o << "]}";
        return o.str();
    }

    std::string bundle_json(const std::string& bundle_id) const {
        auto bs = bundles();
        for (auto& b : bs) {
            if (b.bundle_id != bundle_id) continue;
            std::ostringstream o;
            o << "{\"bundle_id\":\"" << json_escape(b.bundle_id)
              << "\",\"event_id\":\"" << json_escape(b.event_id)
              << "\",\"created_at\":\"" << json_escape(b.created_at)
              << "\",\"evidence_count\":" << json_escape(b.evidence_count)
              << ",\"content_digest\":\"" << json_escape(b.content_digest)
              << "\",\"snapshot\":\"" << json_escape(b.snapshot) << "\"}";
            return o.str();
        }
        return "{\"error\":\"not found\"}";
    }

    std::string event_detail_json(const std::string& event_id) const {
        auto es = events();
        const Event* evt = nullptr;
        for (auto& e : es) { if (e.id==event_id) { evt=&e; break; } }
        if (!evt) return "{\"error\":\"not found\"}";
        auto all_claims = claims();
        auto all_evidence = evidence();
        auto all_bundles = bundles();
        auto all_asr = assessments();
        std::ostringstream o;
        o << "{\"event\":{\"id\":\"" << json_escape(evt->id) << "\",\"title\":\"" << json_escape(evt->title) << "\",\"type\":\"" << json_escape(evt->type) << "\"},";
        o << "\"claims\":[";
        bool first=true;
        for (auto& c : all_claims) { if(c.event_id!=event_id)continue; if(!first)o<<','; first=false; o<<"{\"id\":\""<<json_escape(c.id)<<"\",\"text\":\""<<json_escape(c.text)<<"\"}";}
        o << "],\"evidence\":["; first=true;
        for (auto& c : all_claims) {
            if(c.event_id!=event_id)continue;
            for (auto& ev : all_evidence) {
                if(ev.claim_id!=c.id)continue;
                if(!first) o<<',';
                first=false;
                o<<"{\"id\":\""<<json_escape(ev.id)<<"\",\"content\":\""<<json_escape(ev.content)<<"\",\"contributions\":\""<<json_escape(ev.contributions)<<"\",\"registered_at\":\""<<json_escape(ev.registered_at)<<"\"}";
            }
        }
        o << "],\"bundles\":["; first=true;
        for (auto& b : all_bundles) {
            if(b.event_id!=event_id)continue;
            if(!first) o<<',';
            first=false;
            o<<"{\"bundle_id\":\""<<json_escape(b.bundle_id)<<"\",\"created_at\":\""<<json_escape(b.created_at)<<"\",\"evidence_count\":"<<json_escape(b.evidence_count)<<",\"content_digest\":\""<<json_escape(b.content_digest)<<"\"}";
        }
        o << "],\"assessments\":["; first=true;
        for (auto& a : all_asr) {
            if(a.event_id!=event_id)continue;
            if(!first) o<<',';
            first=false;
            o<<"{\"assessment_id\":\""<<json_escape(a.assessment_id)<<"\",\"bundle_id\":\""<<json_escape(a.bundle_id)<<"\",\"evaluator\":\""<<json_escape(a.evaluator)<<"\",\"assessed_at\":\""<<json_escape(a.assessed_at)<<"\",\"vector\":{\"exposure\":"<<a.exposure<<",\"interaction\":"<<a.interaction<<",\"appropriation\":"<<a.appropriation<<",\"incorporation\":"<<a.incorporation<<",\"propagation\":"<<a.propagation<<",\"reflexivity\":"<<a.reflexivity<<",\"stabilization\":"<<a.stabilization<<"},\"status\":\""<<json_escape(a.status)<<"\"}";
        }
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

// ---------------------------------------------------------------------------
// HTTP utilities (unchanged from bootstrap)
// ---------------------------------------------------------------------------

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
    std::ostringstream h; h<<"HTTP/1.1 "<<status<<' '<<reason<<"\r\nContent-Type: "<<type<<"\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
    auto head=h.str(); ::send(fd,head.data(),head.size(),0); ::send(fd,body.data(),body.size(),0);
}

std::string content_type(const fs::path& p) {
    auto e=p.extension().string();
    if(e==".html")return"text/html; charset=utf-8";
    if(e==".js")return"application/javascript; charset=utf-8";
    if(e==".css")return"text/css; charset=utf-8";
    return"text/plain; charset=utf-8";
}

// Extract a path segment: /api/events/evt-000001/bundles → ("evt-000001", "/api/events/{id}/bundles")
// Returns matched id if the path matches pattern prefix/{id}/suffix
std::optional<std::string> extract_id(const std::string& path, const std::string& prefix, const std::string& suffix) {
    if (path.rfind(prefix, 0) != 0) return std::nullopt;
    auto after = path.substr(prefix.size());
    if (suffix.empty()) { return after; }
    auto pos = after.find(suffix);
    if (pos == std::string::npos) return std::nullopt;
    return after.substr(0, pos);
}

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------

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
            const auto& m=req->method; const auto& p=req->path;

            // --- GET routes ---
            if(m=="GET" && (p=="/api/health" || p=="/api/status"))
                send_response(c,200,"OK","application/json; charset=utf-8",
                    "{\"status\":\"READY\",\"completeness\":\"INCOMPLETE\",\"system\":\"SisTer Reflexa\",\"phase\":\"MVP-0\"}");
            else if(m=="GET" && p=="/api/state")
                send_response(c,200,"OK","application/json; charset=utf-8",store.state_json());
            else if(m=="GET" && p=="/api/model/status")
                send_response(c,200,"OK","application/json; charset=utf-8",
                    "{\"model\":\"TinyReflexiveLM\",\"status\":\"NOT_TRAINED\",\"authority\":\"NONE\","
                    "\"note\":\"MVP-0 collects adjudicable evaluation snapshots; it does not fake a model.\"}");
            else if(m=="GET" && p.rfind("/api/events/",0)==0 && p.find("/",12)!=std::string::npos) {
                // GET /api/events/{id}
                auto id = p.substr(12, p.find("/",12)-12);
                if (id.empty()) send_response(c,400,"Bad Request","application/json","{\"error\":\"missing id\"}");
                else send_response(c,200,"OK","application/json; charset=utf-8",store.event_detail_json(id));
            }
            else if(m=="GET" && p.rfind("/api/bundles/",0)==0) {
                auto id = p.substr(13);
                if (id.empty()) send_response(c,400,"Bad Request","application/json","{\"error\":\"missing id\"}");
                else send_response(c,200,"OK","application/json; charset=utf-8",store.bundle_json(id));
            }
            // --- POST routes ---
            else if(m=="POST" && p=="/api/events") {
                auto f=parse_form(req->body); auto id=store.add_event(f);
                send_response(c,201,"Created","application/json; charset=utf-8","{\"id\":\""+json_escape(id)+"\",\"append_only\":true}");
            }
            else if(m=="POST" && p=="/api/claims") {
                auto f=parse_form(req->body); auto id=store.add_claim(f);
                send_response(c,201,"Created","application/json; charset=utf-8","{\"id\":\""+json_escape(id)+"\",\"append_only\":true}");
            }
            else if(m=="POST" && p=="/api/evidence") {
                auto f=parse_form(req->body); auto id=store.add_evidence(f);
                send_response(c,201,"Created","application/json; charset=utf-8","{\"id\":\""+json_escape(id)+"\",\"append_only\":true}");
            }
            else if(m=="POST" && p=="/api/relations") {
                auto f=parse_form(req->body); auto id=store.add_relation(f);
                send_response(c,201,"Created","application/json; charset=utf-8","{\"id\":\""+json_escape(id)+"\",\"append_only\":true}");
            }
            else if(m=="POST" && p=="/api/evaluations") {
                auto f=parse_form(req->body); auto id=store.add_evaluation(f);
                send_response(c,201,"Created","application/json; charset=utf-8","{\"id\":\""+json_escape(id)+"\",\"append_only\":true}");
            }
            // POST /api/events/{id}/bundles — freeze bundle
            else if(m=="POST" && p.rfind("/api/events/",0)==0 && p.rfind("/bundles")!=std::string::npos) {
                auto event_id = extract_id(p, "/api/events/", "/bundles");
                if (!event_id) { send_response(c,400,"Bad Request","application/json","{\"error\":\"bad path\"}"); }
                else {
                    auto bnd_id = store.freeze_bundle(*event_id);
                    auto bj = store.bundle_json(bnd_id);
                    send_response(c,201,"Created","application/json; charset=utf-8",bj);
                }
            }
            // POST /api/bundles/{id}/assessments — assess bundle
            else if(m=="POST" && p.rfind("/api/bundles/",0)==0 && p.rfind("/assessments")!=std::string::npos) {
                auto bundle_id = extract_id(p, "/api/bundles/", "/assessments");
                if (!bundle_id) { send_response(c,400,"Bad Request","application/json","{\"error\":\"bad path\"}"); }
                else {
                    auto asr_id = store.assess_bundle(*bundle_id);
                    send_response(c,201,"Created","application/json; charset=utf-8","{\"assessment_id\":\""+json_escape(asr_id)+"\",\"status\":\"experimental\",\"evaluator\":\"deterministic-baseline-v0.1\",\"note\":\"PROVISIONAL-NOT-SCIENTIFICALLY-VALIDATED\"}");
                }
            }
            // Static files
            else if(m=="GET") {
                fs::path rel=p=="/"?"index.html":p.substr(1);
                if(rel.string().find("..")!=std::string::npos){ send_response(c,400,"Bad Request","text/plain","bad path"); }
                else { fs::path fp=project_root/"web"/rel; if(fs::exists(fp)&&fs::is_regular_file(fp)) send_response(c,200,"OK",content_type(fp),read_file(fp)); else send_response(c,404,"Not Found","text/plain","not found"); }
            }
            else send_response(c,405,"Method Not Allowed","text/plain","method not allowed");
        } catch(const std::exception& e){ send_response(c,500,"Internal Server Error","application/json","{\"error\":\""+json_escape(e.what())+"\"}"); }
        close(c);
    }
    close(s); return 0;
}

// ---------------------------------------------------------------------------
// Tests (T1–T7)
// ---------------------------------------------------------------------------

int self_test() {
    auto root=fs::temp_directory_path()/"sister-reflexa-self-test"; fs::remove_all(root); Store s(root);
    auto evt=s.add_event({{"title","Dia de campo"},{"type","field_day"},{"valid_time","2026-08-17"},{"context","pratica inicial"}});
    auto clm=s.add_claim({{"event_id",evt},{"text","pratica alterada"}});
    auto evd=s.add_evidence({{"claim_id",clm},{"kind","document"},{"valid_time","2026-08-18"},{"source_ref","ata"},{"digest","sha256:test"},{"content","registro posterior"},{"contributions","exposure:0.8:0.9"}});
    (void)evd;
    auto eva=s.add_evaluation({{"event_id",evt},{"verdict","PARTIAL"},{"t","HIGH"},{"e","HIGH"},{"d","MED"},{"k","LOW"},{"a","MED"},{"r","LOW"},{"p","NA"},{"f","LOW"},{"uncertainty","HIGH"},{"summary","primeiro snapshot"}});
    (void)eva;
    if(s.events().size()!=1 || s.claims().size()!=1 || s.evidence().size()!=1 || s.evaluations().size()!=1) return 2;
    auto j=s.state_json(); if(j.find("NO_AGGREGATE_SCORE")==std::string::npos || j.find("PARTIAL")==std::string::npos) return 3;
    Store again(root); if(again.events().size()!=1) return 4;
    fs::remove_all(root); std::cout<<"self-test PASS\n"; return 0;
}

// T2: bundle creation
int test_bundle_creation() {
    auto root=fs::temp_directory_path()/"sister-reflexa-t2"; fs::remove_all(root); Store s(root);
    auto evt=s.add_event({{"title","T2 Event"},{"type","test"}});
    auto clm=s.add_claim({{"event_id",evt},{"text","T2 claim"}});
    s.add_evidence({{"claim_id",clm},{"content","evidence A"},{"contributions","exposure:0.5:1.0"}});

    auto bnd_id = s.freeze_bundle(evt);
    auto bs = s.bundles();
    if (bs.empty()) { std::cerr<<"T2: no bundles created\n"; return 2; }
    auto& b = bs[0];
    if (b.bundle_id != bnd_id) { std::cerr<<"T2: bundle id mismatch\n"; return 3; }
    if (b.evidence_count != "1") { std::cerr<<"T2: wrong evidence count: "<<b.evidence_count<<"\n"; return 4; }
    if (b.content_digest.empty() || b.content_digest == "fnv1a64:0000000000000000") { std::cerr<<"T2: bad digest\n"; return 5; }
    if (b.snapshot.empty()) { std::cerr<<"T2: empty snapshot\n"; return 6; }
    fs::remove_all(root);
    std::cout<<"T2 bundle-creation PASS\n"; return 0;
}

// T3: temporal preservation — critical
int test_temporal() {
    auto root=fs::temp_directory_path()/"sister-reflexa-t3"; fs::remove_all(root); Store s(root);
    auto evt=s.add_event({{"title","T3 Event"},{"type","test"}});
    auto clm=s.add_claim({{"event_id",evt},{"text","T3 claim"}});
    // t1: add evidence with content A
    s.add_evidence({{"claim_id",clm},{"content","conteudoA"},{"contributions","exposure:0.8:0.9"}});

    // freeze bundle B1 at t1
    auto b1_id = s.freeze_bundle(evt);
    auto bs1 = s.bundles();
    if (bs1.size()!=1) { std::cerr<<"T3: expected 1 bundle after t1, got "<<bs1.size()<<"\n"; return 2; }
    auto b1_digest = bs1[0].content_digest;
    auto b1_snapshot = bs1[0].snapshot;
    auto b1_count = bs1[0].evidence_count;

    // t2: add new evidence (different content) — simulates mutation/addition
    s.add_evidence({{"claim_id",clm},{"content","conteudoB"},{"contributions","interaction:0.3:0.6"}});

    // B1 must not have changed: re-read store
    Store s2(root);
    auto bs_after = s2.bundles();
    if (bs_after.empty()) { std::cerr<<"T3: bundles disappeared after adding evidence\n"; return 3; }
    // Find B1 (first bundle)
    bool found_b1 = false;
    for (auto& b : bs_after) {
        if (b.bundle_id == b1_id) {
            if (b.content_digest != b1_digest) {
                std::cerr<<"T3: B1 digest changed! expected="<<b1_digest<<" got="<<b.content_digest<<"\n"; return 4;
            }
            if (b.snapshot != b1_snapshot) {
                std::cerr<<"T3: B1 snapshot changed after adding new evidence\n"; return 5;
            }
            if (b.evidence_count != b1_count) {
                std::cerr<<"T3: B1 evidence_count changed\n"; return 6;
            }
            found_b1 = true;
        }
    }
    if (!found_b1) { std::cerr<<"T3: B1 not found after re-read\n"; return 7; }

    // freeze B2 at t2 (should include both evidence items)
    auto b2_id = s2.freeze_bundle(evt);
    auto bs_b2 = s2.bundles();
    const EvidenceBundle* b2 = nullptr;
    for (auto& b : bs_b2) { if (b.bundle_id==b2_id) { b2=&b; break; } }
    if (!b2) { std::cerr<<"T3: B2 not found\n"; return 8; }
    if (b2->evidence_count != "2") { std::cerr<<"T3: B2 should have 2 evidence items, got "<<b2->evidence_count<<"\n"; return 9; }
    if (b2->content_digest == b1_digest) { std::cerr<<"T3: B1 and B2 have same digest — temporal closure failed\n"; return 10; }
    // Snapshot stores pct_encoded content: "conteudoA" is pct_encode("conteudoA") = "conteudoA" (no special chars)
    if (b2->snapshot.find("conteudoA")==std::string::npos) { std::cerr<<"T3: B2 missing conteudoA\n"; return 11; }
    if (b2->snapshot.find("conteudoB")==std::string::npos) { std::cerr<<"T3: B2 missing conteudoB\n"; return 12; }
    if (b1_snapshot.find("conteudoB")!=std::string::npos) { std::cerr<<"T3: B1 snapshot contains conteudoB — temporal closure breached\n"; return 13; }

    fs::remove_all(root);
    std::cout<<"T3 temporal-preservation PASS\n"; return 0;
}

// T4: deterministic assessment
int test_deterministic() {
    auto root=fs::temp_directory_path()/"sister-reflexa-t4"; fs::remove_all(root); Store s(root);
    auto evt=s.add_event({{"title","T4 Event"},{"type","test"}});
    auto clm=s.add_claim({{"event_id",evt},{"text","T4 claim"}});
    s.add_evidence({{"claim_id",clm},{"content","evidence X"},{"contributions","exposure:0.6:0.8,interaction:0.4:0.7"}});
    auto bnd_id = s.freeze_bundle(evt);

    // Assess twice
    auto asr1 = s.assess_bundle(bnd_id);
    auto asr2 = s.assess_bundle(bnd_id);

    auto as_all = s.assessments();
    if (as_all.size() < 2) { std::cerr<<"T4: fewer than 2 assessments\n"; return 2; }

    const Assessment* a1=nullptr, *a2=nullptr;
    for (auto& a : as_all) {
        if (a.assessment_id==asr1) a1=&a;
        if (a.assessment_id==asr2) a2=&a;
    }
    if (!a1||!a2) { std::cerr<<"T4: assessments not found\n"; return 3; }
    if (a1->exposure!=a2->exposure || a1->interaction!=a2->interaction ||
        a1->appropriation!=a2->appropriation || a1->incorporation!=a2->incorporation ||
        a1->propagation!=a2->propagation || a1->reflexivity!=a2->reflexivity ||
        a1->stabilization!=a2->stabilization) {
        std::cerr<<"T4: vectors differ between two assessments of same bundle\n"; return 4;
    }
    fs::remove_all(root);
    std::cout<<"T4 deterministic PASS\n"; return 0;
}

// T5: restart reproducibility
int test_restart() {
    auto root=fs::temp_directory_path()/"sister-reflexa-t5"; fs::remove_all(root);
    std::string bnd_id, asr_id;
    std::string exposure_before;
    {
        Store s(root);
        auto evt=s.add_event({{"title","T5 Event"},{"type","test"}});
        auto clm=s.add_claim({{"event_id",evt},{"text","T5 claim"}});
        s.add_evidence({{"claim_id",clm},{"content","persistent evidence"},{"contributions","reflexivity:0.7:0.9"}});
        bnd_id = s.freeze_bundle(evt);
        asr_id = s.assess_bundle(bnd_id);
        auto as_all = s.assessments();
        for (auto& a : as_all) { if(a.assessment_id==asr_id) exposure_before=a.reflexivity; }
    }
    // Simulate restart: new Store instance from same root
    {
        Store s(root);
        auto bs = s.bundles();
        bool found_bnd = false;
        for (auto& b : bs) { if(b.bundle_id==bnd_id) { found_bnd=true; break; } }
        if (!found_bnd) { std::cerr<<"T5: bundle not found after restart\n"; return 2; }
        auto as_all = s.assessments();
        bool found_asr = false;
        for (auto& a : as_all) {
            if (a.assessment_id==asr_id) {
                if (a.reflexivity!=exposure_before) { std::cerr<<"T5: vector changed after restart\n"; return 3; }
                found_asr=true; break;
            }
        }
        if (!found_asr) { std::cerr<<"T5: assessment not found after restart\n"; return 4; }
    }
    fs::remove_all(root);
    std::cout<<"T5 restart-reproducibility PASS\n"; return 0;
}

// T6: API vertical slice (in-process test of Store layer, no sockets needed)
int test_api_slice() {
    auto root=fs::temp_directory_path()/"sister-reflexa-t6"; fs::remove_all(root); Store s(root);
    // event
    auto evt=s.add_event({{"title","Dia de campo sobre manejo pastoril"},{"type","field_day"},{"valid_time","2026-08-18"},{"context","manejo inicial"}});
    if(evt.empty()){ std::cerr<<"T6: no event\n"; return 2; }
    // claim
    auto clm=s.add_claim({{"event_id",evt},{"text","participantes alteraram forma de decidir manejo"}});
    if(clm.empty()){ std::cerr<<"T6: no claim\n"; return 3; }
    // evidence
    auto evd=s.add_evidence({{"claim_id",clm},{"kind","field_notes"},{"content","facilitador registrou mudanca na linguagem dos participantes"},{"contributions","exposure:0.9:0.85,interaction:0.7:0.75,appropriation:0.5:0.6"}});
    if(evd.empty()){ std::cerr<<"T6: no evidence\n"; return 4; }
    // bundle
    auto bnd=s.freeze_bundle(evt);
    if(bnd.empty()){ std::cerr<<"T6: no bundle\n"; return 5; }
    // assessment
    auto asr=s.assess_bundle(bnd);
    if(asr.empty()){ std::cerr<<"T6: no assessment\n"; return 6; }
    // verify vector
    auto as_all=s.assessments();
    bool found=false;
    for(auto& a:as_all){
        if(a.assessment_id!=asr)continue;
        found=true;
        // exposure should be > 0.5 given support=0.9, confidence=0.85
        double exp=std::stod(a.exposure);
        if(exp<=0.5){ std::cerr<<"T6: expected exposure>0.5, got "<<exp<<"\n"; return 7; }
        if(a.status!="experimental"){ std::cerr<<"T6: wrong status\n"; return 8; }
        if(a.evaluator!="deterministic-baseline-v0.1"){ std::cerr<<"T6: wrong evaluator\n"; return 9; }
    }
    if(!found){ std::cerr<<"T6: assessment not found\n"; return 10; }
    // verify state_json includes bundles and assessments
    auto j=s.state_json();
    if(j.find("bundles")==std::string::npos){ std::cerr<<"T6: bundles missing from state\n"; return 11; }
    if(j.find("assessments")==std::string::npos){ std::cerr<<"T6: assessments missing from state\n"; return 12; }
    fs::remove_all(root);
    std::cout<<"T6 api-vertical-slice PASS\n"; return 0;
}

// T7: web server availability (starts server, checks /api/health via socket)
int test_web(const fs::path& project_root, int port=18092) {
    // Fork a server, connect to it, check response, then kill
    pid_t pid = fork();
    if (pid < 0) { std::cerr<<"T7: fork failed\n"; return 2; }
    if (pid == 0) {
        // Child: run server (we use a temp data dir to avoid polluting real data)
        auto tmp_root = fs::temp_directory_path()/"sister-reflexa-t7";
        fs::create_directories(tmp_root/"data");
        fs::create_directories(tmp_root/"web");
        // Copy web files if they exist
        auto src_web = project_root/"web";
        if (fs::exists(src_web)) {
            for (auto& e : fs::recursive_directory_iterator(src_web)) {
                auto rel = fs::relative(e.path(), src_web);
                if (e.is_regular_file()) {
                    fs::create_directories((tmp_root/"web"/rel).parent_path());
                    fs::copy_file(e.path(), tmp_root/"web"/rel, fs::copy_options::overwrite_existing);
                }
            }
        }
        // Run server
        run_server(tmp_root, "127.0.0.1", port);
        std::exit(0);
    }
    // Parent: wait a moment then connect
    ::usleep(200000); // 200ms
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { ::kill(pid, SIGTERM); ::waitpid(pid,nullptr,0); std::cerr<<"T7: socket failed\n"; return 3; }
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    int attempts = 10;
    while (attempts-- > 0) {
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) break;
        ::usleep(50000);
    }
    if (attempts < 0) {
        ::close(fd); ::kill(pid, SIGTERM); ::waitpid(pid,nullptr,0);
        std::cerr<<"T7: could not connect to server\n"; return 4;
    }
    const char* req = "GET /api/health HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n";
    ::send(fd, req, strlen(req), 0);
    std::string resp; std::array<char,4096> buf{};
    while(true){ auto n=::recv(fd,buf.data(),buf.size(),0); if(n<=0)break; resp.append(buf.data(),static_cast<size_t>(n)); }
    ::close(fd);
    ::kill(pid, SIGTERM);
    int wstatus; ::waitpid(pid,&wstatus,0);
    fs::remove_all(fs::temp_directory_path()/"sister-reflexa-t7");
    if (resp.find("READY")==std::string::npos) {
        std::cerr<<"T7: expected READY in response, got: "<<resp.substr(0,200)<<"\n"; return 5;
    }
    std::cout<<"T7 web-availability PASS\n"; return 0;
}

} // namespace

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    try {
        const fs::path binary_path = fs::weakly_canonical(fs::path(argv[0]).parent_path());
        if(argc==2 && std::string_view(argv[1])=="--status") {
            std::cout<<"SisTer Reflexa\ncore=READY\ncompleteness=INCOMPLETE\nphase=MVP-0\nmodel=NOT_TRAINED\nscore_policy=NO_AGGREGATE_SCORE\n"; return 0;
        }
        if(argc==2 && std::string_view(argv[1])=="--self-test") return self_test();
        if(argc==2 && std::string_view(argv[1])=="--test-bundle-creation") return test_bundle_creation();
        if(argc==2 && std::string_view(argv[1])=="--test-temporal") return test_temporal();
        if(argc==2 && std::string_view(argv[1])=="--test-deterministic") return test_deterministic();
        if(argc==2 && std::string_view(argv[1])=="--test-restart") return test_restart();
        if(argc==2 && std::string_view(argv[1])=="--test-api-slice") return test_api_slice();
        if(argc==2 && std::string_view(argv[1])=="--test-web") {
            // Project root is two levels up from binary (build/sister-reflexa → .)
            fs::path project_root = binary_path.parent_path();
            return test_web(project_root);
        }
        if(argc>=2 && std::string_view(argv[1])=="--serve") {
            fs::path root=fs::current_path(); std::string host="127.0.0.1"; int port=8092;
            if(argc>=3) host=argv[2];
            if(argc>=4) port=std::stoi(argv[3]);
            return run_server(root,host,port);
        }
        std::cout<<"Usage:\n"
                   "  sister-reflexa --status\n"
                   "  sister-reflexa --self-test\n"
                   "  sister-reflexa --test-bundle-creation\n"
                   "  sister-reflexa --test-temporal\n"
                   "  sister-reflexa --test-deterministic\n"
                   "  sister-reflexa --test-restart\n"
                   "  sister-reflexa --test-api-slice\n"
                   "  sister-reflexa --test-web\n"
                   "  sister-reflexa --serve [host] [port]\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"ERROR: "<<e.what()<<'\n'; return 1; }
}
