#include <fstream>    
#include <iostream>
#include <filesystem>
#define MAXFILENAME 64
#define FNAMERSLTN 32
namespace fs = std::filesystem;
using FILE = std::fstream;
using str = std::string;
//

class AriDB {
    
};
/*
-- MGR FILE --
|     Size    |           File Names           |
|   2 Bytes   | 32 chars | 32 bytes |   ....   |   
*/
struct AriDBMSMgr {
    unsigned short size;
    char** fnames;
    AriDB* dbs;
};

class AriDBMS {
    AriDBMSMgr* mgr;
    AriDBMS(char** inputs){
        mgr = new AriDBMSMgr();
        init();
        
    }
    
    short init() {
        // -- INITIALIZE VDBMS (Very Demeaning Ball Munching Slander) --  
        // Create/open a file
        fs::create_directories("vdbs");
        FILE file("mgr.dat", std::ios::binary);  
        if (!file.is_open()) {
            return ENOFILE;
        }
        file.close();
        
        // Load Manager File 
        file.read(reinterpret_cast<char*>(&mgr->size), sizeof(unsigned short));
        mgr->fnames = new char*[mgr->size + FNAMERSLTN - (mgr->size % FNAMERSLTN)];
        if(mgr->size == 0){
            return 0;
        }
        for(int i = 0; i < mgr->size; i++){
            mgr->fnames[i] = new char[MAXFILENAME];
            file.read(mgr->fnames[i], MAXFILENAME);
        }

        return 0;
    }

    void err(short num){
        
    }
};

int main() {
    
    return 0;
}