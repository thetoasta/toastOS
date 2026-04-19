/*
 * toastOS++ File
 * Converted to C++ from toastOS
 */

#ifndef FILE_HPP
#define FILE_HPP

#ifdef __cplusplus
extern "C" {
#endif

void local_fs(const char* filename, const char* content);
void read_local_fs(const char* file_id);
void list_files(void);
void delete_file(const char* file_id);
void fs_test_auto(void);

#ifdef __cplusplus
}
#endif

#endif /* FILE_HPP */
