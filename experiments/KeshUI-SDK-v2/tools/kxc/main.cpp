#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

struct Attribute { std::string name; std::string value; };
struct Node {
    std::string name;
    std::vector<Attribute> attributes;
    std::vector<std::unique_ptr<Node>> children;
    std::string text;
    int line{1};
};

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open input: " + path);
    std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}

std::string Trim(std::string_view v) {
    std::size_t a=0,b=v.size();
    while(a<b && std::isspace(static_cast<unsigned char>(v[a])))++a;
    while(b>a && std::isspace(static_cast<unsigned char>(v[b-1])))--b;
    return std::string(v.substr(a,b-a));
}

std::string DecodeEntities(std::string s) {
    struct P { const char* from; const char* to; };
    for (auto p : {P{"&lt;","<"},P{"&gt;",">"},P{"&quot;","\""},P{"&apos;","'"},P{"&amp;","&"}}) {
        std::size_t pos=0; while((pos=s.find(p.from,pos))!=std::string::npos){s.replace(pos,std::strlen(p.from),p.to);pos+=std::strlen(p.to);}
    }
    return s;
}

class Parser {
public:
    explicit Parser(std::string source):source_(std::move(source)){}
    std::unique_ptr<Node> Parse() {
        SkipTrivia();
        auto root=ParseNode();
        SkipTrivia();
        if(pos_!=source_.size()) Error("unexpected trailing content");
        return root;
    }
private:
    [[noreturn]] void Error(const std::string& m) const { throw std::runtime_error("XAML line " + std::to_string(line_) + ": " + m); }
    bool Starts(std::string_view s) const { return source_.compare(pos_,s.size(),s)==0; }
    char Peek() const { return pos_<source_.size()?source_[pos_]:'\0'; }
    char Get(){if(pos_>=source_.size())Error("unexpected end of file");char c=source_[pos_++];if(c=='\n')++line_;return c;}
    void SkipSpace(){while(std::isspace(static_cast<unsigned char>(Peek())))Get();}
    void SkipTrivia(){
        for(;;){
            SkipSpace();
            if(Starts("<?")){pos_+=2;while(pos_<source_.size()&&!Starts("?>"))Get();if(!Starts("?>"))Error("unterminated processing instruction");pos_+=2;continue;}
            if(Starts("<!--")){pos_+=4;while(pos_<source_.size()&&!Starts("-->"))Get();if(!Starts("-->"))Error("unterminated comment");pos_+=3;continue;}
            break;
        }
    }
    std::string Name(){
        std::size_t start=pos_;
        while(pos_<source_.size()){
            char c=Peek();
            if(std::isalnum(static_cast<unsigned char>(c))||c=='_'||c==':'||c=='.'||c=='-')++pos_;else break;
        }
        if(start==pos_)Error("expected name");
        return source_.substr(start,pos_-start);
    }
    std::string Quoted(){
        SkipSpace();char quote=Get();if(quote!='\"'&&quote!='\'')Error("expected quoted attribute value");std::string out;while(Peek()!=quote){if(Peek()=='\0')Error("unterminated attribute");out+=Get();}Get();return DecodeEntities(out);
    }
    std::unique_ptr<Node> ParseNode(){
        SkipTrivia(); if(Get()!='<')Error("expected '<'"); if(Peek()=='/')Error("unexpected closing tag");
        auto node=std::make_unique<Node>();node->line=line_;node->name=Name();
        for(;;){
            SkipSpace();
            if(Starts("/>")){pos_+=2;return node;}
            if(Peek()=='>'){Get();break;}
            Attribute a; a.name=Name(); SkipSpace(); if(Get()!='=')Error("expected '=' after attribute"); a.value=Quoted(); node->attributes.push_back(std::move(a));
        }
        std::string text;
        for(;;){
            if(pos_>=source_.size())Error("unterminated element " + node->name);
            if(Starts("<!--")){SkipTrivia();continue;}
            if(Starts("</")){
                pos_+=2;auto closing=Name();SkipSpace();if(Get()!='>')Error("expected '>'");if(closing!=node->name)Error("closing tag mismatch: expected " + node->name + ", got " + closing);break;
            }
            if(Peek()=='<') node->children.push_back(ParseNode());
            else text+=Get();
        }
        node->text=Trim(DecodeEntities(text));
        if(!node->text.empty()) node->attributes.push_back({"Content",node->text});
        return node;
    }
    std::string source_; std::size_t pos_{0}; int line_{1};
};

const std::unordered_set<std::string>& KnownTypes(){
    static const std::unordered_set<std::string> types={
        "Window","Grid","StackPanel","Canvas","Border","TextBlock","Button","AccentButton","HyperlinkButton",
        "ToggleSwitch","CheckBox","RadioButton","Slider","ProgressBar","ProgressRing","TextBox","SearchBox","ComboBox",
        "ListView","TabView","NavigationView","InfoBar","Badge","Card","SettingsCard","BreadcrumbBar","Expander","Separator",
        "ScrollViewer","Tooltip","ContextMenu","ContentDialog"
    };return types;
}

std::string LocalName(std::string_view raw){auto p=raw.rfind(':');return std::string(p==std::string_view::npos?raw:raw.substr(p+1));}

void Validate(const Node& n,int& nodes,int& attrs){
    ++nodes;attrs+=static_cast<int>(n.attributes.size());
    auto type=LocalName(n.name);
    if(!KnownTypes().contains(type)) throw std::runtime_error("XAML line " + std::to_string(n.line) + ": unknown KeshUI type '" + n.name + "'");
    for(auto& a:n.attributes){
        if(a.name=="x:Name"||a.name=="x:Class"||a.name.rfind("xmlns",0)==0||a.name.find('.')!=std::string::npos)continue;
        if(a.name.empty())throw std::runtime_error("XAML line " + std::to_string(n.line) + ": empty attribute name");
    }
    for(auto& c:n.children)Validate(*c,nodes,attrs);
}

std::string EscapeCpp(std::string_view s){std::string out;for(char c:s){switch(c){case '\\':out+="\\\\";break;case '\"':out+="\\\"";break;case '\n':out+="\\n";break;case '\r':break;case '\t':out+="\\t";break;default:out+=c;}}return out;}

void Dump(const Node& n,int depth=0){std::cout<<std::string(depth*2,' ')<<n.name;for(auto&a:n.attributes)std::cout<<" "<<a.name<<"=\""<<a.value<<"\"";std::cout<<"\n";for(auto&c:n.children)Dump(*c,depth+1);}

struct Gen { int next{0}; std::ostringstream out; };

std::string EmitNode(Gen& g,const Node& n,const std::string& parent={}){
    std::string id="n"+std::to_string(g.next++);
    g.out<<"    auto "<<id<<" = keshui::CreateXamlElement(\""<<EscapeCpp(n.name)<<"\");\n";
    g.out<<"    if (!"<<id<<") throw std::runtime_error(\"KXaml generated unknown element: "<<EscapeCpp(n.name)<<"\");\n";
    for(auto&a:n.attributes){
        g.out<<"    keshui::ApplyXamlAttribute("<<id<<", \""<<EscapeCpp(a.name)<<"\", \""<<EscapeCpp(a.value)<<"\", &binder);\n";
    }
    if(!parent.empty())g.out<<"    "<<parent<<"->AddChild("<<id<<");\n";
    for(auto&c:n.children)EmitNode(g,*c,id);
    return id;
}

std::string Generate(const Node& root,const std::string& input){
    Gen g;
    g.out<<"// Generated by KXamlC from "<<EscapeCpp(input)<<". DO NOT EDIT.\n";
    g.out<<"#pragma once\n#include <keshui/keshui.hpp>\n#include <stdexcept>\n\nnamespace keshui_generated {\n";
    g.out<<"inline std::shared_ptr<keshui::Window> BuildMainWindow(keshui::XamlEventBinder& binder) {\n";
    auto rootId=EmitNode(g,root);
    g.out<<"    auto window = std::dynamic_pointer_cast<keshui::Window>("<<rootId<<");\n";
    g.out<<"    if (!window) throw std::runtime_error(\"Root element must be Window\");\n";
    g.out<<"    return window;\n}\n}\n";
    return g.out.str();
}

void Usage(){std::cerr<<"KXamlC 0.2 - KeshUI XAML compiler\nUsage:\n  kxc input.xaml output.g.hpp\n  kxc --check input.xaml\n  kxc --dump-ir input.xaml\n";}

} // namespace

int main(int argc,char**argv){
    try{
        if(argc<3){Usage();return 2;}
        bool check=std::string_view(argv[1])=="--check";
        bool dump=std::string_view(argv[1])=="--dump-ir";
        std::string input=(check||dump)?argv[2]:argv[1];
        auto root=Parser(ReadFile(input)).Parse();
        int nodes=0,attrs=0;Validate(*root,nodes,attrs);
        if(LocalName(root->name)!="Window")throw std::runtime_error("root XAML element must be Window");
        if(check){std::cout<<"KXamlC: OK - "<<nodes<<" elements, "<<attrs<<" attributes\n";return 0;}
        if(dump){Dump(*root);return 0;}
        if(argc<3){Usage();return 2;}
        std::ofstream out(argv[2],std::ios::binary);if(!out)throw std::runtime_error("cannot open output");out<<Generate(*root,input);out.close();
        std::cout<<"KXamlC: generated "<<argv[2]<<" ("<<nodes<<" elements, "<<attrs<<" attributes)\n";
        return 0;
    }catch(const std::exception&e){std::cerr<<"KXamlC error: "<<e.what()<<"\n";return 1;}
}
