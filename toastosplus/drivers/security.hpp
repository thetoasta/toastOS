/*
 * toastOS++ Security
 * Converted to C++ from toastOS
 */

#ifndef SECURITY_HPP
#define SECURITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

//
// Created by toastman on 3/20/2026.
//

void showFilePrompt(char* filename, char* reason);
void denyFilePrompt(char* reason);

#ifdef __cplusplus
}
#endif

#endif /* SECURITY_HPP */
