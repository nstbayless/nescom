#include <cstdio>
#include <list>
#include <map>
#include <set>

#include "dataarea.hh"
#include "assemble.hh"
#include "object.hh"
#include "relocdata.hh"
#include "warning.hh"

#define PROGNAME "snescom"

class Object::Segment
{
    /// RELOCATION DATA (FOR O65 OUTPUT) ///
public:
    typedef Relocdata<std::string> RT; RT R;
public:
    void CloseSegment();


    /// COMPILETIME SYMBOLS AND REFERENCES ///
private:
    class Extern
    {
        // This record saves a references to a label.
        unsigned pos;

        char type;        // "prefix"
        long value;       // what's added to it

        std::string ref;
        // On which scope level this was created on
        unsigned level;
        
    public:
        Extern(unsigned o, char t, long v,
               const std::string& r)
          : pos(o),
            type(t), value(v), ref(r), level() { }
    
        char GetType() const { return type; }
        long GetValue() const { return value; }
        
        unsigned GetPos() const { return pos; }

        const std::string& GetName() const { return ref; }
        
        void SetScopeLevel(unsigned n) { level = n; }
        unsigned GetLevel() const { return level; }

        void Dump() const;
    };
    
    class Fixup
    {
        // This record saves a references to an address.
        unsigned pos;

        char type;        // "prefix"
        long value;       // what's added to it

        SegmentSelection targetseg;
        unsigned targetoffset;
    public:
        Fixup(unsigned o, char t, long v,
              SegmentSelection s2, unsigned o2)
          : pos(o),
            type(t), value(v),
            targetseg(s2), targetoffset(o2) { }
    

        char GetType() const { return type; }
        long GetValue() const { return value; }

        unsigned GetPos() const { return pos; }
        SegmentSelection GetTargetSeg() const { return targetseg; }
        unsigned GetTargetOffset() const { return targetoffset; }

        void Dump() const;
    };
    
    std::list<Extern> Externs;
    std::list<Fixup> Fixups;
public:
    void CheckExterns(unsigned CurScope, const Object& obj);
    void AddExtern(char prefix, const std::string& ref,
                      long value, unsigned CurScope);
    void DumpExterns(const char *segname) const;
    void DumpFixups(const char *segname) const;



    /// MEMORY ///
private:
    unsigned Position;
    DataArea Data;
public:
    void AddByte(unsigned char byte);
    void SetByte(unsigned offset, unsigned char byte);
    unsigned char GetByte(unsigned offset) const;
    unsigned GetPos() const;
    void SetPos(unsigned newpos);
    unsigned GetBase() const { return Data.GetBase(); }
    unsigned GetSize() const { return Data.GetSize(); }
    
    unsigned FindNextBlob(unsigned where, unsigned& length) const;
    
    const std::vector<unsigned char> GetContent() const;
    const std::vector<unsigned char> GetContent(unsigned a,unsigned l) const;


    /// LABELS ///
public:
    typedef std::map<std::string, unsigned> LabelList;
    typedef std::map<unsigned, LabelList> LabelMap;
private:
    LabelMap labels;

    std::set<std::string> UnusedLabels;
    void MarkLabelUsed(const std::string& s) { UnusedLabels.erase(s); }
public:
    const LabelMap& GetLabels() const { return labels; }
    LabelList& GetLabels(unsigned level) { return labels[level]; }

    void ClearLabels(unsigned level);
    
    void DefineLabel(unsigned level, const std::string& name);
    void DefineLabel(unsigned level, const std::string& name, unsigned value);
    
    bool FindLabel(const std::string& name, unsigned level, unsigned& result) const;
    bool FindLabel(const std::string& name, unsigned& result) const;
    bool FindLabel(const std::string& name) const;
    
    void UndefineLabel(const std::string& name);

    void DumpLabels(const char *segname) const;


    /// GENERIC ///
public:
    Segment(): Position(0) {}
private:
    Segment(const Segment&);
    //void operator=(const Segment&);

public:
    void ClearMost()
    {
        *this = Segment();
    }    
};

void Object::Segment::AddByte(unsigned char byte)
{
    //std::fprintf(stderr, "Generated byte %02X\n", byte);
    Data.WriteByte(Position++, byte);
}

void Object::Segment::SetByte(unsigned offset, unsigned char byte)
{
    Data.WriteByte(offset, byte);
}

unsigned char Object::Segment::GetByte(unsigned offset) const
{
    return Data.GetByte(offset);
}

unsigned Object::Segment::GetPos() const
{
    return Position;
}

void Object::Segment::SetPos(unsigned newpos)
{
    Position = newpos;
}

unsigned Object::Segment::FindNextBlob(unsigned where, unsigned& length) const
{
    return Data.FindNextBlob(where, length);
}

const std::vector<unsigned char> Object::Segment::GetContent() const
{
    return Data.GetContent();
}

const std::vector<unsigned char> Object::Segment::GetContent(unsigned a, unsigned l) const
{
    return Data.GetContent(a, l);
}

void Object::Segment::ClearLabels(unsigned level)
{
    LabelList& level_labels = GetLabels(level);
    
    for(LabelList::const_iterator
        i = level_labels.begin(); i !=level_labels.end(); ++i)
    {
        if(UnusedLabels.find(i->first) != UnusedLabels.end())
        {
            if(MayWarn("unused-label"))
            {
                std::fprintf(stderr,
                    "Warning: Unused label '%s'\n",
                        i->first.c_str());
            }
            UnusedLabels.erase(i->first);
        }
    }
    
    level_labels.clear();
}

bool Object::Segment::FindLabel(const std::string& name, unsigned level,
                                unsigned& result) const
{
    LabelMap::const_iterator i = labels.find(level);
    if(i == labels.end()) return false;
    LabelList::const_iterator j = i->second.find(name);
    if(j != i->second.end()) { result = j->second; return true; }
    return false;
}

bool Object::Segment::FindLabel(const std::string& name) const
{
    for(LabelMap::const_iterator
        i = labels.begin();
        i != labels.end(); ++i)
    {
        LabelList::const_iterator j = i->second.find(name);
        if(j != i->second.end()) return true;
    }
    return false;
}

bool Object::Segment::FindLabel(const std::string& name,
                                unsigned& result) const
{
    for(LabelMap::const_iterator
        i = labels.begin();
        i != labels.end(); ++i)
    {
        LabelList::const_iterator j = i->second.find(name);
        if(j != i->second.end()) { result = j->second; return true; }
    }
    return false;
}

void Object::Segment::DefineLabel(unsigned level, const std::string& name, unsigned value)
{
    UnusedLabels.insert(name);
    labels[level][name] = value;
}

void Object::Segment::DefineLabel(unsigned level, const std::string& name)
{
    DefineLabel(level, name, GetPos());
}

void Object::Segment::UndefineLabel(const std::string& label)
{
    for(LabelMap::iterator
        i = labels.begin();
        i != labels.end(); ++i)
    {
        i->second.erase(label);
    }
}

void Object::Segment::DumpLabels(const char *segname) const
{
    bool first = true;
    for(LabelMap::const_iterator
        i = labels.begin();
        i != labels.end(); ++i)
    {
        // level -> labels
        
        for(LabelList::const_iterator
            j = i->second.begin();
            j != i->second.end();
            ++j)
        {
            // name -> address
            
            if(first)
            {
                std::fprintf(stderr, "Labels in the %4s segment:\n", segname);
                first = false;
            }
            
            std::fprintf(stderr, " %04X ", j->second);
            for(unsigned a=0; a<i->first; ++a) std::fprintf(stderr, "+");
            std::fprintf(stderr, "%s\n", j->first.c_str());
        }
    }
}

void Object::Segment::Extern::Dump() const
{
    std::fprintf(stderr, " %04X %c%s", pos, type, ref.c_str());
    if(value) std::fprintf(stderr, "%+ld", value);
    std::fprintf(stderr, "\n");
}

void Object::Segment::Fixup::Dump() const
{
    std::fprintf(stderr, " %04X %cfixup", pos, type);
    if(value) std::fprintf(stderr, "%+ld", value);
    std::fprintf(stderr, " to %d:%04X\n", (int)targetseg, targetoffset);
}

void Object::Segment::CheckExterns(unsigned CurScope, const Object& obj)
{
    // Resolve all externs so far.
    // It's ok if not all are resolvable.
    
    for(std::list<Extern>::iterator
        j, i=Externs.begin(); i!=Externs.end(); i=j)
    {
        j=i; ++j;
        
        // Skip it, if it's not its time yet
        if(i->GetLevel() < CurScope) continue;
        
        const std::string& ref = i->GetName();
        
        for(unsigned scope=CurScope; scope-- > 0; )
        {
            unsigned targetaddr=0;
            SegmentSelection targetseg;
            if(obj.FindLabel(ref, scope, targetseg, targetaddr))
            {
                MarkLabelUsed(ref);
                
                const unsigned pos = i->GetPos();
                const char prefix = i->GetType();
                const long value  = i->GetValue();
                
                Fixup newref(pos, prefix, value, targetseg, targetaddr);
                Fixups.push_back(newref);
                Externs.erase(i);
                break;
            }
        }
    }
}

void Object::Segment::AddExtern(char prefix, const std::string& ref,
                                long value, unsigned CurScope)
{
    const unsigned pos = GetPos();
    Extern newext(pos, prefix, value, ref);
    newext.SetScopeLevel(CurScope);
    Externs.push_back(newext);
}

void Object::Segment::DumpExterns(const char *segname) const
{
    if(Externs.empty()) return;
    std::fprintf(stderr, "Externs in the %4s segment:\n", segname);
    
    std::list<Extern>::const_iterator i;
    for(i=Externs.begin(); i!=Externs.end(); ++i)
        i->Dump();
}

void Object::Segment::DumpFixups(const char *segname) const
{
    if(Fixups.empty()) return;
    std::fprintf(stderr, "Fixups in the %4s segment:\n", segname);
    
    std::list<Fixup>::const_iterator i;
    for(i=Fixups.begin(); i!=Fixups.end(); ++i)
        i->Dump();
}

void Object::Segment::CloseSegment()
{
    for(std::list<Extern>::const_iterator
        i=Externs.begin(); i!=Externs.end(); ++i)
    {
        const Extern& ref = *i;
        
        const unsigned     address = ref.GetPos();
              long           value = ref.GetValue();
        const std::string&    name = ref.GetName();
        
        switch(ref.GetType())
        {
            case FORCE_LOBYTE:
            {
                R.R16lo.AddReloc(address, name);

                SetByte(address, value & 0xFF);
                break;
            }
            case FORCE_HIBYTE:
            {
                RT::R16hi_t::Type data(address, value & 0xFF);
                R.R16hi.AddReloc(data, name);

                SetByte(address, (value >> 8) & 0xFF);
                break;
            }
            case FORCE_ABSWORD:
            {
                R.R16.AddReloc(address, name);

                SetByte(address,   value & 0xFF);
                SetByte(address+1, (value >> 8) & 0xFF);
                break;
            }
            case FORCE_LONG:
            {
                R.R24.AddReloc(address, name);

                SetByte(address,   value & 0xFF);
                SetByte(address+1, (value >> 8) & 0xFF);
                SetByte(address+2, (value >> 16) & 0xFF);
                break;
            }
            case FORCE_SEGBYTE:
            {
                RT::R24seg_t::Type data(address, value & 0xFFFF);
                
                R.R24seg.AddReloc(data, name);

                SetByte(address, (value >> 16) & 0xFF);
                break;
            }
            case FORCE_REL8:
            {
                std::fprintf(stderr,
                    "Error: Unresolved short relative '%s'\n", name.c_str()
                            );                                             
                break;
            }
            case FORCE_REL16:
            {
                std::fprintf(stderr,
                    "Error: Unresolved near relative '%s'\n", name.c_str()
                            );
                break;
            }
        }
    }
    
    for(std::list<Fixup>::const_iterator
        i=Fixups.begin(); i!=Fixups.end(); ++i)
    {
        const Fixup& ref = *i;
        
        const unsigned     address = ref.GetPos();
              long           value = ref.GetValue();
        const SegmentSelection seg = ref.GetTargetSeg();
        const unsigned        offs = ref.GetTargetOffset();
        
        value += offs;

        switch(ref.GetType())
        {
            case FORCE_LOBYTE:
            {
                R.R16lo.AddFixup(seg, address);
                SetByte(address, value & 0xFF);
                break;
            }
            case FORCE_HIBYTE:
            {
                RT::R16hi_t::Type data(address, value & 0xFF);
                R.R16hi.AddFixup(seg, data);
                SetByte(address, (value >> 8) & 0xFF);
                break;
            }
            case FORCE_ABSWORD:
            {
                R.R16.AddFixup(seg, address);
                SetByte(address,   value & 0xFF);
                SetByte(address+1, (value >> 8) & 0xFF);
                break;
            }
            case FORCE_LONG:
            {
                R.R24.AddFixup(seg, address);
                SetByte(address,   value & 0xFF);
                SetByte(address+1, (value >> 8) & 0xFF);
                SetByte(address+2, (value >> 16) & 0xFF);
                break;
            }
            case FORCE_SEGBYTE:
            {
                RT::R24seg_t::Type data(address, value & 0xFFFF);
                R.R24seg.AddFixup(seg, data);
                SetByte(address, (value >> 16) & 0xFF);
                break;
            }
            case FORCE_REL8:
            {
                const long diff = value - (long)address - 1;
                
                int threshold = 0;
                extern bool already_reprocessed;
                if(!already_reprocessed) threshold = 20;
                
                if(diff < -0x80+threshold || diff >= 0x80-threshold)
                {
                    std::fprintf(stderr,
                        "Error: Short jump out of range (%ld)\n", diff);
                }
                
                SetByte(address, diff & 0xFF);
                
                break;
            }
            case FORCE_REL16:
            {
                const long diff = value - (long)address - 2;
                
                if(diff < -0x8000 || diff >= 0x8000)
                {
                    std::fprintf(stderr,
                        "Error: Near jump out of range (%ld)\n", diff);
                }
                
                SetByte(address,   diff & 0xFF);
                SetByte(address+1, (diff >> 8) & 0xFF);
                
                break;
            }
        }
    }
    
    //Externs.clear();
    //Fixups.clear();
}

Object::Segment& Object::GetSeg()
{
    switch(CurSegment)
    {
        case CODE: return *code;
        case DATA: return *data;
        case ZERO: return *zero;
        case BSS: return *bss;
    }
    return *code;
}

const Object::Segment& Object::GetSeg() const
{
    switch(CurSegment)
    {
        case CODE: return *code;
        case DATA: return *data;
        case ZERO: return *zero;
        case BSS: return *bss;
    }
    return *code;
}

bool Object::FindLabel(const std::string& s) const
{
    return code->FindLabel(s)
        || data->FindLabel(s)
        || zero->FindLabel(s)
        || bss->FindLabel(s);
}

bool Object::FindLabel(const std::string& name, unsigned level,
                       SegmentSelection& seg, unsigned& result) const
{
    if(code->FindLabel(name, level, result)) { seg=CODE; return true; }
    if(data->FindLabel(name, level, result)) { seg=DATA; return true; }
    if(zero->FindLabel(name, level, result)) { seg=ZERO; return true; }
    if(bss->FindLabel(name, level, result)) { seg=BSS; return true; }
    return false;
}

bool Object::FindLabel(const std::string& name,
                       SegmentSelection& seg, unsigned& result) const
{
    if(code->FindLabel(name, result)) { seg=CODE; return true; }
    if(data->FindLabel(name, result)) { seg=DATA; return true; }
    if(zero->FindLabel(name, result)) { seg=ZERO; return true; }
    if(bss->FindLabel(name, result)) { seg=BSS; return true; }
    return false;
}

void Object::StartScope()
{
    ++CurScope;
}

void Object::EndScope()
{
    code->CheckExterns(CurScope, *this);
    data->CheckExterns(CurScope, *this);
    zero->CheckExterns(CurScope, *this);
    bss->CheckExterns(CurScope, *this);

    if(CurScope > 0)
    {
        // Forget the labels of this level.
        
        // But never forget the global-level labels,
        // because they are to become public.
        if(CurScope > 1)
        {
            code->ClearLabels(CurScope-1);
            data->ClearLabels(CurScope-1);
            zero->ClearLabels(CurScope-1);
            bss->ClearLabels(CurScope-1);
        }
    }
    --CurScope;
}

void Object::AddExtern(char prefix, const std::string& ref, long value)
{
    GetSeg().AddExtern(prefix, ref, value, CurScope);
}

void Object::DefineLabel(const std::string& label)
{
    DefineLabel(label, GetPos());
}

unsigned Object::GetPos() const
{
    return GetSeg().GetPos();
}

void Object::DefineLabel(const std::string& label, unsigned value)
{
    std::string s = label;
    // Find out which scope to define it in
    unsigned scopenum = CurScope-1;
    if(s[0] == '+')
    {
        // global label
        s = s.substr(1);
        scopenum = 0;
    }
    while(s[0] == '&' && CurScope > 0)
    {
        s = s.substr(1);
        --scopenum;
    }
    
    if(FindLabel(s))
    {
        std::fprintf(stderr, "Error: Label '%s' already defined\n", s.c_str());
        return;
    }
    
    GetSeg().DefineLabel(scopenum, s, value);
}

void Object::SetPos(unsigned newpos)
{
    GetSeg().SetPos(newpos);
}

void Object::UndefineLabel(const std::string& label)
{
    code->UndefineLabel(label);
    data->UndefineLabel(label);
    zero->UndefineLabel(label);
    bss->UndefineLabel(label);
}

void Object::CloseSegments()
{
    code->CloseSegment();
    data->CloseSegment();
    zero->CloseSegment();
    bss->CloseSegment();
}

namespace
{
    void PutC(unsigned char c, std::FILE* fp)
    {
        // 8-bit output.
        std::fputc(c, fp);
    }
    void PutS(const void* s, unsigned n, std::FILE* fp)
    {
        std::fwrite(s, n, 1, fp);
        //for(unsigned a=0; a<n; ++a) PutC(s[a], fp);
    }
    void PutW(unsigned short w, std::FILE* fp)
    {
        // 16-bit lsb-first O65 output.
        PutC(w & 0xFF, fp);
        PutC(w >> 8,  fp);
    }
    void PutMW(unsigned short w, std::FILE* fp)
    {
        // 16-bit msb-first IPS output.
        PutC(w >> 8,  fp);
        PutC(w & 0xFF, fp);
    }
    void PutD(unsigned int w, std::FILE* fp)
    {
        // 32-bit lsb-first O65 output.
        PutW(w & 0xFFFF, fp);
        PutW(w >> 16,    fp);
    }
    void PutL(unsigned int w, std::FILE* fp)
    {
        // 24-bit msb-first IPS output.
        PutC((w >> 16) & 0xFF, fp);
        PutC((w >> 8) & 0xFF, fp);
        PutC(w & 0xFF, fp);
    }
    void PutWD(unsigned int w, std::FILE* fp, bool Use32)
    {
        // 16 or 32-bit lsb-first O65 output.
        if(Use32) PutD(w, fp); else PutW(w, fp);
    }
    void PutCustomHeader(std::FILE* fp, int type, int param1, int param2)
    {
        PutC(7,      fp); // length: 1+1 + 1 + 4
        PutC(type,   fp);
        PutC(param1, fp);
        PutD(param2, fp);
    }
    void PutCustomHeader(std::FILE* fp, int type, const std::string& s)
    {
        PutC(s.size()+3, fp); // length: 1+1+string+1
        PutC(type,       fp);
        PutS(s.c_str(),  s.size()+1, fp);
    }
    
    struct Unresolved
    {
        std::map<std::string, unsigned> str2num;
        std::vector<std::string> num2str;
        unsigned size() const { return num2str.size(); }
        
        void Add(const std::string& name)
        {
            if(str2num.find(name) != str2num.end()) return;
            str2num[name] = num2str.size();
            num2str.push_back(name);
        }
        
        void Put(std::FILE* fp, bool use32)
        {
            PutWD(size(), fp, use32);
            for(unsigned a=0; a<size(); ++a)
                PutS(num2str[a].c_str(), num2str[a].size()+1, fp);
        }
        
        unsigned Find(const std::string& s) const
        {
            return str2num.find(s)->second;
        }
    public:
        Unresolved(): str2num(), num2str() { }
    };
    
    unsigned char GetSegmentID(SegmentSelection s)
    {
        return s;
    }
    
    typedef std::map<unsigned, std::string> RelocMap;
    
    void BuildReloc(const Object::Segment& seg,
                    struct Unresolved& syms)
    {
        #define WalkList(type, which) \
            for(Object::Segment::RT::type##_t::which##List::const_iterator \
                i = seg.R.type.which##s.begin(); \
                i != seg.R.type.which##s.end(); \
                ++i)

        WalkList(R16lo, Reloc) syms.Add(i->second);
        WalkList(R16, Reloc)   syms.Add(i->second);
        WalkList(R24, Reloc)   syms.Add(i->second);
        WalkList(R16hi, Reloc) syms.Add(i->second);
        WalkList(R24seg, Reloc)syms.Add(i->second);
    }
    
    void PutReloc(const Object::Segment& seg,
                  struct Unresolved& syms,
                  std::FILE* fp)
    {
        // Address-sorted table of relocs in binary format.
        RelocMap relocs;
        
        #define WalkList(type, which) \
            for(Object::Segment::RT::type##_t::which##List::const_iterator \
                i = seg.R.type.which##s.begin(); \
                i != seg.R.type.which##s.end(); \
                ++i)

        WalkList(R16lo, Fixup)
        {
            std::string result;
            result += char(0x20 | GetSegmentID(i->first));
            relocs[i->second] = result;
        }

        WalkList(R16, Fixup)
        {
            std::string result;
            result += char(0x80 | GetSegmentID(i->first));
            relocs[i->second] = result;
        }

        WalkList(R24, Fixup)
        {
            std::string result;
            result += char(0xC0 | GetSegmentID(i->first));
            relocs[i->second] = result;
        }

        WalkList(R16hi, Fixup)
        {
            std::string result;
            result += char(0x40 | GetSegmentID(i->first));
            result += char(i->second.second);
            relocs[i->second.first] = result;
        }

        WalkList(R24seg, Fixup)
        {
            std::string result;
            result += char(0xA0 | GetSegmentID(i->first));
            result += char(i->second.second & 255);
            result += char(i->second.second >> 8);
            relocs[i->second.first] = result;
        }

        WalkList(R16lo, Reloc)
        {
            std::string result;
            unsigned n = syms.Find(i->second);
            result += char(0x20);
            result += char(n & 255);
            result += char(n >> 8);
            relocs[i->first] = result;
        }
        
        WalkList(R16, Reloc)
        {
            std::string result;
            unsigned n = syms.Find(i->second);
            result += char(0x80);
            result += char(n & 255);
            result += char(n >> 8);
            relocs[i->first] = result;
        }
        
        WalkList(R24, Reloc)
        {
            std::string result;
            unsigned n = syms.Find(i->second);
            result += char(0xC0);
            result += char(n & 255);
            result += char(n >> 8);
            relocs[i->first] = result;
        }

        WalkList(R16hi, Reloc)
        {
            std::string result;
            unsigned n = syms.Find(i->second);
            result += char(0x40);
            result += char(n & 255);
            result += char(n >> 8);
            result += char(i->first.second);
            relocs[i->first.first] = result;
        }

        WalkList(R24seg, Reloc)
        {
            std::string result;
            unsigned n = syms.Find(i->second);
            result += char(0xA0);
            result += char(n & 255);
            result += char(n >> 8);
            result += char(i->first.second & 255);
            result += char(i->first.second >> 8);
            relocs[i->first.first] = result;
        }
        
        int addr = -1;
        for(RelocMap::const_iterator i = relocs.begin(); i != relocs.end(); ++i)
        {
            int new_addr = i->first;
            int diff = new_addr - addr;
            if(diff <= 0)
            {
                std::fprintf(stderr, "Eep, diff=%d\n", diff);
            }
            while(diff > 254)
            {
                PutC(255, fp);
                diff -= 254;
            }
            PutC(diff, fp);
            addr = new_addr;
            PutS(i->second.data(), i->second.size(), fp);
        }
        PutC(0, fp);
    }
    
    void PutLabels(const Object::Segment& seg,
                   SegmentSelection segtype,
                   std::FILE* fp,
                   bool use32)
    {
        const unsigned char segid = GetSegmentID(segtype);
        
        typedef Object::Segment::LabelMap LabelMap;
        const LabelMap& labels = seg.GetLabels();
        
        // Count labels
        unsigned count = 0;
        for(LabelMap::const_iterator i = labels.begin(); i != labels.end(); ++i)
        {
            count += i->second.size();
        }
        
        PutWD(count, fp, use32);
        
        // Put labels
        for(LabelMap::const_iterator i = labels.begin(); i != labels.end(); ++i)
        {
            for(Object::Segment::LabelList::const_iterator
                j = i->second.begin();
                j != i->second.end();
                ++j)
            {
                unsigned addr           = j->second;
                const std::string& name = j->first;
                
                PutS(name.c_str(), name.size()+1, fp);
                PutC(segid, fp);
                PutWD(addr, fp, use32);
            }
        }
    }
    
    const std::pair<unsigned, std::string> BuildGlobalPatch
       (const std::string& varname, unsigned addr)
    {
        std::string patch(varname);
        patch += (char)0;
        patch += (char)((addr) & 0xFF);
        patch += (char)((addr >> 8) & 0xFF);
        patch += (char)((addr >> 16) & 0x3F);
        return make_pair(IPS_ADDRESS_GLOBAL, patch);
    }
    
    const std::pair<unsigned, std::string> BuildExternPatch
       (unsigned addr,
        const std::string& varname,
        unsigned size)
    {
        std::string patch(varname);
        patch += (char)0;
        patch += (char)((addr) & 0xFF);
        patch += (char)((addr >> 8) & 0xFF);
        patch += (char)((addr >> 16) & 0x3F);
        patch += (char)(size & 0xFF);
        return make_pair(IPS_ADDRESS_EXTERN, patch);
    }

    void IPSwriteSeg(const Object::Segment& seg,
                     std::FILE* fp)
    {
        typedef Object::Segment::LabelMap LabelMap;
        const LabelMap& labels = seg.GetLabels();
        
        std::list<std::pair<unsigned, std::string> > patches;
        
        // Put labels (this is DarkForce's extension)
        for(LabelMap::const_iterator i = labels.begin(); i != labels.end(); ++i)
        {
            for(Object::Segment::LabelList::const_iterator
                j = i->second.begin();
                j != i->second.end();
                ++j)
            {
                unsigned addr           = j->second;
                const std::string& name = j->first;
                patches.push_back(BuildGlobalPatch(name, addr));
            }
        }
        
        /* Ignore fixups. IPS is not meant to be relocated... */
        /* It's safe to ignore them silently. */
        
        #define WalkList(type, which) \
            for(Object::Segment::RT::type##_t::which##List::const_iterator \
                i = seg.R.type.which##s.begin(); \
                i != seg.R.type.which##s.end(); \
                ++i)

        WalkList(R16lo, Reloc)
        {
            patches.push_back(BuildExternPatch(i->first, i->second, 1));
        }
        
        WalkList(R16, Reloc)
        {
            patches.push_back(BuildExternPatch(i->first, i->second, 2));
        }
        
        WalkList(R24, Reloc)
        {
            patches.push_back(BuildExternPatch(i->first, i->second, 3));
        }
        
        if(!seg.R.R16hi.Relocs.empty())
        {
            fprintf(stderr, "Error: Hi-byte-type externs aren't supported in IPS format.\n");
        }
        if(!seg.R.R24seg.Relocs.empty())
        {
            fprintf(stderr, "Error: Segment-type externs aren't supported in IPS format.\n");
        }

        for(std::list<std::pair<unsigned, std::string> >::const_iterator
            i = patches.begin();
            i != patches.end();
            ++i)
        {
            PutL(i->first, fp);
            PutMW(i->second.size(), fp);
            PutS(i->second.data(), i->second.size(), fp);
        }

        unsigned addr = 0;
        for(;;)
        {
            unsigned size;
            addr = seg.FindNextBlob(addr, size);
            if(!size) break;
            
            for(unsigned left = size; left > 0; )
            {
                unsigned count = 20000;
                if(count > left) count = left;
                
                //fprintf(stderr, "Writing %u @ %06X\n", count, addr);
                
                if(addr == IPS_EOF_MARKER)
                {
                    fprintf(stderr,
                        "Error: IPS doesn't allow patches that go to $%X\n", addr);
                }
                else if(addr == IPS_ADDRESS_EXTERN)
                {
                    fprintf(stderr,
                        "Error: Address $%X is reserved for IPS_ADDRESS_EXTERN\n", addr);
                }
                else if(addr == IPS_ADDRESS_GLOBAL)
                {
                    fprintf(stderr,
                        "Error: Address $%X is reserved for IPS_ADDRESS_GLOBAL\n", addr);
                }
                else if(addr > 0xFFFFFF)
                {
                    fprintf(stderr,
                        "Error: Address $%X is too big for IPS format\n", addr);
                }
                
                PutL(addr & 0x3FFFFF, fp);
                PutMW(count, fp);
                
                std::vector<unsigned char> data = seg.GetContent(addr, count);
                
                PutS(&data[0], count, fp);
                
                left -= count;
                addr += count;
            }
        }
    }
};

void Object::WriteO65(std::FILE* fp)
{
    /* Building the map now so it can be used in the use32 test */
    Unresolved externs;
    BuildReloc(*code, externs);
    BuildReloc(*data, externs);
    BuildReloc(*zero, externs);
    BuildReloc( *bss, externs);
    
    /* Test whether 32bitness is needed */
    bool use32 = false;
    if(code->GetBase() > 0xFFFF
    || code->GetSize() > 0xFFFF
    || data->GetBase() > 0xFFFF
    || data->GetSize() > 0xFFFF
    || bss->GetBase() > 0xFFFF
    || bss->GetSize() > 0xFFFF
    || zero->GetBase() > 0xFFFF
    || zero->GetSize() > 0xFFFF
    || externs.size() > 0xFFFF
      )
    {
        use32 = true;
        if(MayWarn("use32"))
        {
            fprintf(stderr, "Warning: Writing a 32-bit object file\n");
        }
    }
    
    unsigned short Mode
        = 0x8000   // 65816
        | 0x1000;  // object, not exe
    
    if(use32) Mode |= 0x2000; // Use 32-bit addresses
    
    // Put O65 headerl
    PutS("\1\0o65\0", 6, fp);
    
    // Put Mode
    PutW(Mode, fp);
    
    //text
    PutWD(code->GetBase(), fp, use32);
    PutWD(code->GetSize(), fp, use32);
    //data
    PutWD(data->GetBase(), fp, use32);
    PutWD(data->GetSize(), fp, use32);
    //bss
    PutWD(bss->GetBase(), fp, use32);
    PutWD(bss->GetSize(), fp, use32);
    //zero
    PutWD(zero->GetBase(), fp, use32);
    PutWD(zero->GetSize(), fp, use32);
    
    // stack size - 0 = undefined
    PutWD(0x0000, fp, use32);
    
    switch(Linkage.type)
    {
        case LinkageWish::LinkInGroup:
            PutCustomHeader(fp, 10, 1, Linkage.GetGroup());
            break;
        case LinkageWish::LinkThisPage:
            PutCustomHeader(fp, 10, 2, Linkage.GetPage());
            break;
        
        default: /* ignore */ break;
    }
    
    PutCustomHeader(fp, 2, PROGNAME" "VERSION);
    
    // end custom headers
    PutC(0, fp);
    
    std::fwrite(&code->GetContent()[0], code->GetSize(), 1, fp);
    std::fwrite(&data->GetContent()[0], data->GetSize(), 1, fp);
    
    externs.Put(fp, use32);
    
    PutReloc(*code, externs, fp);
    PutReloc(*data, externs, fp);
    
    PutLabels(*code, CODE, fp, use32);
    PutLabels(*data, DATA, fp, use32);
    PutLabels(*zero, ZERO, fp, use32);
    PutLabels( *bss, BSS,  fp, use32);
}

void Object::WriteIPS(std::FILE* fp)
{
    if(Linkage.type != LinkageWish::LinkAnywhere)
    {
        fprintf(stderr, "Warning: IPS file is never relocated - .link statement ignored.\n");
    }
    
    PutS("PATCH", 5, fp);
    
    IPSwriteSeg(*code, fp);
    IPSwriteSeg(*data, fp);
    IPSwriteSeg(*bss,  fp);
    IPSwriteSeg(*zero, fp);
    
    PutS("EOF", 3, fp);
}

void Object::Dump()
{
    DumpLabels();
    DumpExterns();
    //DumpFixups();
}

void Object::GenerateByte(unsigned char byte)
{
    GetSeg().AddByte(byte);
}


void Object::DumpLabels() const
{
    code->DumpLabels("TEXT");
    data->DumpLabels("DATA");
    zero->DumpLabels("ZERO");
    bss->DumpLabels("BSS");
}

void Object::DumpExterns() const
{
    code->DumpExterns("TEXT");
    data->DumpExterns("DATA");
    zero->DumpExterns("ZERO");
    bss->DumpExterns("BSS");
}

void Object::DumpFixups() const
{
    code->DumpFixups("TEXT");
    data->DumpFixups("DATA");
    zero->DumpFixups("ZERO");
    bss->DumpFixups("BSS");
}

void Object::ClearMost()
{
    CurScope = 0;
    CurSegment = CODE;
    
    code->ClearMost();
    data->ClearMost();
    zero->ClearMost();
    bss->ClearMost();
}

Object::Object()
    : code(new Segment),
      data(new Segment),
      zero(new Segment),
      bss(new Segment),
      CurScope(0), CurSegment(CODE),
      Linkage()
{
}

Object::~Object()
{
    delete code;
    delete data;
    delete zero;
    delete bss;
}
