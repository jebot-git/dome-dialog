// SPDX-License-Identifier: GPL-3.0-or-later
// Native FLTK dialogs for Zenity command-line clients used by this installation.
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Browser.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Secret_Input.H>
#include <FL/Fl_Text_Display.H>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
#include "appearance.h"
struct Options {
    std::string mode, title="DomeD", text, separator="|", filename, filter="*";
    int width=620,height=440,timeout=0,printColumn=0;
    bool multiple=false,checklist=false,radiolist=false,autoClose=false,hideText=false;
    std::vector<std::string> columns,rows,hidden;
    struct Field {std::string label,type,values;};
    std::vector<Field> fields;
};
static std::vector<std::string> split(const std::string &s,char delimiter) {
    std::vector<std::string> result;std::istringstream in(s);std::string part;
    while(std::getline(in,part,delimiter))result.push_back(part);
    return result;
}
static Options parse(int argc,char **argv) {
    Options o;
    const std::set<std::string> modes={"info","error","warning","question","entry","password","list","forms","progress","file-selection","text-info"};
    const std::set<std::string> values={"title","text","width","height","timeout","entry-text","filename","file-filter","separator","print-column","hide-column","column","add-entry","add-password","add-combo","combo-values","ok-label","cancel-label","window-icon","icon-name","name","class","display","attach","expand-column"};
    const std::set<std::string> flags={"no-wrap","ellipsize","no-markup","auto-close","auto-kill","pulsate","multiple","radiolist","checklist","editable","hide-text","directory","save","confirm-overwrite","modal","switch","default-cancel"};
    for(int i=1;i<argc;++i) {
        std::string argument=argv[i];
        if(argument=="--version"){std::cout<<"4.2.2\n";std::exit(0);}
        if(argument=="--help"||argument=="--help-all"){std::cout<<"DomeD FLTK dialogs: --info --error --warning --question --entry --password --list --forms --progress --file-selection --text-info\n";std::exit(0);}
        if(!argument.starts_with("--")){o.rows.push_back(argument);continue;}
        auto equal=argument.find('=');auto key=argument.substr(2,equal==std::string::npos?equal:equal-2);
        std::string value=equal==std::string::npos?"":argument.substr(equal+1);
        if(values.contains(key)&&equal==std::string::npos){if(++i>=argc)throw std::runtime_error("Missing value for --"+key);value=argv[i];}
        if(modes.contains(key)){if(!o.mode.empty()&&o.mode!=key)throw std::runtime_error("Choose one dialog type");o.mode=key;}
        else if(key=="title")o.title=value;
        else if(key=="text")o.text=value;
        else if(key=="width")o.width=std::clamp(std::stoi(value),280,2000);
        else if(key=="height")o.height=std::clamp(std::stoi(value),200,1600);
        else if(key=="timeout")o.timeout=std::max(0,std::stoi(value));
        else if(key=="entry-text")o.rows.push_back(value);
        else if(key=="filename")o.filename=value;
        else if(key=="file-filter"){auto at=value.find('|');o.filter=at==std::string::npos?value:value.substr(at+1);}
        else if(key=="separator")o.separator=value;
        else if(key=="print-column")o.printColumn=std::stoi(value);
        else if(key=="hide-column")o.hidden=split(value,',');
        else if(key=="column")o.columns.push_back(value);
        else if(key=="add-entry"||key=="add-password"||key=="add-combo")o.fields.push_back({value,key,{}});
        else if(key=="combo-values"){if(o.fields.empty())throw std::runtime_error("--combo-values requires --add-combo");o.fields.back().values=value;}
        else if(key=="multiple")o.multiple=true;
        else if(key=="checklist")o.checklist=true;
        else if(key=="radiolist")o.radiolist=true;
        else if(key=="auto-close")o.autoClose=true;
        else if(key=="hide-text")o.hideText=true;
        else if(!values.contains(key)&&!flags.contains(key))throw std::runtime_error("Unsupported dialog option: --"+key);
    }
    if(o.mode.empty())throw std::runtime_error("Choose a dialog type; use --help");
    return o;
}
struct State {Fl_Window *window;bool accepted=false;int code=1;};
static void accept(Fl_Widget *,void *data){auto&s=*static_cast<State*>(data);s.accepted=true;s.code=0;s.window->hide();}
static void cancel(Fl_Widget *,void *data){static_cast<State*>(data)->window->hide();}
static void timeout(void *data){auto&s=*static_cast<State*>(data);s.code=5;s.window->hide();}
struct Progress {State *state;Fl_Progress *bar;std::string input;bool autoClose;};
static void progressInput(int fd,void*data) {
    auto&p=*static_cast<Progress*>(data);char buf[4096];auto n=read(fd,buf,sizeof(buf));
    if(n==0){Fl::remove_fd(fd);if(p.autoClose)accept(nullptr,p.state);return;}
    if(n<0)return;
    p.input.append(buf,n);size_t at;
    while((at=p.input.find('\n'))!=std::string::npos){
        auto line=p.input.substr(0,at);p.input.erase(0,at+1);
        if(line.starts_with('#'))p.bar->copy_label(line.substr(1).c_str());
        else {char*end=nullptr;double value=strtod(line.c_str(),&end);if(end!=line.c_str())p.bar->value(std::clamp(value,0.,100.));}
        if(p.autoClose&&p.bar->value()>=100)accept(nullptr,p.state);
    }
}
int main(int argc,char **argv) try {
    auto o=parse(argc,argv);dx::init();
    if(o.mode=="file-selection") {
        Fl_File_Chooser chooser(o.filename.empty()?".":o.filename.c_str(),o.filter.c_str(),o.multiple?Fl_File_Chooser::MULTI:Fl_File_Chooser::SINGLE,o.title.c_str());
        chooser.show();while(chooser.shown())Fl::wait();
        if(!chooser.value())return 1;
        for(int i=1;i<=chooser.count();++i){if(i>1)std::cout<<o.separator;std::cout<<chooser.value(i);}std::cout<<'\n';return 0;
    }
    Fl_Double_Window window(o.width,o.height,o.title.c_str());State state{&window};
    Fl_Text_Buffer buffer;Fl_Text_Display text(12,12,o.width-24,80);text.buffer(buffer);buffer.text(o.text.c_str());
    Fl_Hold_Browser browser(12,100,o.width-24,o.height-154);
    Fl_Check_Browser checks(12,100,o.width-24,o.height-154);
    Fl_Input entry(12,110,o.width-24,32);
    Fl_Secret_Input password(12,110,o.width-24,32);
    Fl_Progress progress(12,110,o.width-24,32);progress.minimum(0);progress.maximum(100);
    browser.hide();checks.hide();entry.hide();password.hide();progress.hide();
    std::vector<std::vector<std::string>> rows;
    std::vector<std::unique_ptr<Fl_Widget>> fields;
    if(o.mode=="list") {
        const auto count=std::max(size_t(1),o.columns.size());
        if(o.rows.size()%count)throw std::runtime_error("List row data does not match the number of columns");
        for(size_t i=0;i<o.rows.size();i+=count) {
            std::vector<std::string> row(o.rows.begin()+i,o.rows.begin()+i+count);std::string label;
            for(size_t column=0;column<count;++column){if((o.checklist||o.radiolist)&&column==0)continue;if(std::find(o.hidden.begin(),o.hidden.end(),std::to_string(column+1))!=o.hidden.end())continue;if(!label.empty())label+="  ";label+=row[column];}
            rows.push_back(row);
            if(o.checklist)checks.add(label.c_str(),row[0]=="TRUE"||row[0]=="true");
            else {browser.add(label.c_str());if(o.radiolist&&(row[0]=="TRUE"||row[0]=="true"))browser.select(rows.size());}
        }
        if(o.checklist)checks.show();else browser.show();
        if(!o.checklist&&!browser.value()&&browser.size())browser.select(1);
    } else if(o.mode=="entry"||o.mode=="password") {
        auto&input=o.mode=="password"||o.hideText?static_cast<Fl_Input&>(password):entry;
        input.value(o.rows.empty()?"":o.rows.front().c_str());input.show();
    } else if(o.mode=="forms") {
        int y=110;
        for(const auto&field:o.fields) {
            if(field.type=="add-combo") {
                auto choice=std::make_unique<Fl_Choice>(150,y,o.width-162,30,field.label.c_str());
                for(const auto&value:split(field.values,'|'))choice->add(value.c_str());choice->value(0);fields.push_back(std::move(choice));
            } else {
                std::unique_ptr<Fl_Input> input;
                if(field.type=="add-password")input=std::make_unique<Fl_Secret_Input>(150,y,o.width-162,30,field.label.c_str());else input=std::make_unique<Fl_Input>(150,y,o.width-162,30,field.label.c_str());
                fields.push_back(std::move(input));
            }
            y+=42;
        }
    } else if(o.mode=="progress")progress.show();
    else if(o.mode=="text-info") {
        std::string content;
        if(!o.filename.empty()){std::ifstream file(o.filename);content.assign(std::istreambuf_iterator<char>(file),{});}else content.assign(std::istreambuf_iterator<char>(std::cin),{});
        buffer.text(content.c_str());text.resize(12,12,o.width-24,o.height-70);
    }
    Fl_Button dismiss(o.width-230,o.height-42,100,28,"Cancel");dismiss.callback(cancel,&state);
    Fl_Button ok(o.width-118,o.height-42,106,28,"OK");ok.callback(accept,&state);ok.shortcut(FL_Enter);
    if(o.mode=="info"||o.mode=="warning"||o.mode=="error")dismiss.hide();
    window.callback(cancel,&state);window.end();window.resizable(text);dx::style(&window);window.show();
    Progress progressState{&state,&progress,{},o.autoClose};
    if(o.mode=="progress"){fcntl(0,F_SETFL,fcntl(0,F_GETFL)|O_NONBLOCK);Fl::add_fd(0,FL_READ,progressInput,&progressState);}
    if(o.timeout)Fl::add_timeout(o.timeout,timeout,&state);
    while(window.shown())Fl::wait();
    if(o.mode=="progress")Fl::remove_fd(0);
    if(o.timeout)Fl::remove_timeout(timeout,&state);
    if(state.accepted) {
        if(o.mode=="entry"||o.mode=="password")std::cout<<(o.mode=="password"||o.hideText?password.value():entry.value())<<'\n';
        else if(o.mode=="forms") {
            for(size_t i=0;i<fields.size();++i){if(i)std::cout<<o.separator;if(auto choice=dynamic_cast<Fl_Choice*>(fields[i].get()))std::cout<<(choice->text()?choice->text():"");else std::cout<<static_cast<Fl_Input*>(fields[i].get())->value();}std::cout<<'\n';
        } else if(o.mode=="list") {
            bool first=true;int column=o.printColumn?o.printColumn:((o.checklist||o.radiolist)?2:1);
            for(size_t i=0;i<rows.size();++i){if(o.checklist?!checks.checked(i+1):browser.value()!=int(i+1))continue;if(column<1||column>int(rows[i].size()))throw std::runtime_error("Invalid output column");if(!first)std::cout<<o.separator;first=false;std::cout<<rows[i][column-1];}std::cout<<'\n';
        }
    }
    text.buffer(nullptr);
    // Child widgets with unique_ptr ownership must leave the FLTK group first.
    for(auto&field:fields)window.remove(field.get());
    return state.code;
} catch(const std::exception&error){std::cerr<<"dome-dialog: "<<error.what()<<'\n';return 2;}
