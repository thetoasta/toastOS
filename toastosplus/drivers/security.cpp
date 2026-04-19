//
// Created by toastman on 3/20/2026.
//

#include "security.hpp"

#include "editor.hpp"
#include "kio.hpp"
#include "string.hpp"

void showFilePrompt(char* filename, char* reason) {
    kprint("[toastSecurity] the file you're trying to edit should be left as is.");
    kprint_newline();
    kprint("[toastSecurity] it's protected because ");
    kprint(reason);
    kprint_newline();
    kprint("[toastSecurity] if your sure you want to edit, type OK > ");
    const char* response = rec_input();
    if (strcmp(response, "OK") == 0 ) {
        editor_open(filename);
    } else {
        kprint("cancelled.");
    }
}


void denyFilePrompt(char* reason) {
    kprint("[toastSecurity] hey, the file you're trying to edit is protected.");
    kprint_newline();
    kprint("[toastSecurity] it's protected because ");
    kprint(reason);
    kprint_newline();
    kprint("[toastSecurity] you cannot edit this file at this time.");
}
