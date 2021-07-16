#define HTCW_LITTLE_ENDIAN
#include <stdio.h>
#include <stdlib.h>
#include "zip.hpp"
using namespace io;
using namespace zip;
int main(int argc, char** argv) {
    const char* path = "D:\\Users\\gazto\\Documents\\nozip\\frankenstein_images.epub";
    file_stream fs(path);
    archive arch;
    zip_result r = archive::open(&fs,&arch);
    if(zip_result::success!=r) {
        printf("error opening zip: %d\r\n",(int)r);
        return -1;
    }
    char name[1024];
    for(int i = 0;i<arch.entries_size();++i) {
        archive_entry entry;
        r=arch.entry(i,&entry);
        if(zip_result::success!=r) {
            printf("error opening zip entry %d: %d\r\n",(int)i,(int)r);
            return -1;
        }
        entry.copy_path(name,1024);
        printf("%d) %s\r\n",(int)i,name);
    }
    return 0;
}