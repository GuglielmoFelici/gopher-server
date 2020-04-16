#include "../headers/protocol.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../headers/datatypes.h"
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"

#define MMAP_CHUNK 800000000 // 800MB

/** Path to the windows gopher helper executable */
string_t winHelperPath = NULL;

typedef struct {
    file_mapping_t* map;
    int totalPages;
    socket_t dest;
    char name[MAX_NAME];
    const logger_t* pLogger;
} send_args_t;

/** @return The character identifying the type of the file, X if the type is unknown or an error occurs. */
static char gopherType(const char* file);

/** Removes CRLF; if str is empty, writes "." to it; 
  * maps all the backslashes to forward slashes; removes trailing slashes.
  * @return GOPHER_SUCCESS or GOPHER_FAILURE if str is NULL
*/
static int normalizePath(string_t str) {
    if (!str) {
        return GOPHER_FAILURE;
    }
    if (strncmp(str, CRLF, sizeof(CRLF)) == 0) {
        strcpy(str, ".");
    } else {
        string_t strtokptr;
        strtok_r(str, CRLF, &strtokptr);
        int len = strlen(str);
        while (str[len - 1] == '/' || str[len - 1] == '\\') {
            str[len - 1] = '\0';
            len--;
        }
        for (int i = 0; i < len; i++) {
            if (str[i] == '\\') {
                str[i] = '/';
            }
        }
    }
    return GOPHER_SUCCESS;
}

// TODO
/** @return true if str represents a valid gopher selector */
static bool validateInput(cstring_t str) {
    if (!str) {
        return false;
    } else if (strncmp(str, CRLF, sizeof(CRLF)) == 0) {
        return true;
    } else {
        return isPathRelative(str) && !strstr(str, "..") && !strstr(str, ".\\") && !strstr(str, "./") && str[strlen(str) - strlen(CRLF) - 1] != '.';
    }
}

/** Sends the error message msg to the socket sock.
 * @return GOPHER_SUCCESS or GOPHER_FAILURE
*/
static int sendErrorResponse(socket_t sock, cstring_t msg) {
    if (PLATFORM_FAILURE == sendAll(sock, ERROR_MSG " - ", sizeof(ERROR_MSG) + 2)) {
        return GOPHER_FAILURE;
    }
    if (PLATFORM_FAILURE == sendAll(sock, msg, strlen(msg))) {
        return GOPHER_FAILURE;
    }
    if (PLATFORM_FAILURE == sendAll(sock, CRLF ".", 3)) {
        return GOPHER_FAILURE;
    }
    if (PLATFORM_FAILURE == closeSocket(sock)) {
        return GOPHER_FAILURE;
    }
    return GOPHER_SUCCESS;
}

/** Builds the file list and sends it to the client. 
 *  @return GOPHER_SUCCESS or GOPHER_FAILURE
*/
static int sendDir(cstring_t path, int sock, int port) {
    if (!path) {
        return GOPHER_FAILURE;
    }
    _dir dir = NULL;
    char filePath[MAX_NAME];
    char* fileName = NULL;
    char* line = NULL;
    size_t lineSize, filePathSize;
    int res;
    while (PLATFORM_SUCCESS == (res = iterateDir(path, &dir, filePath, sizeof(filePath)))) {
        normalizePath(filePath);
        if (NULL == (fileName = strrchr(filePath, '/'))) {  // Isolates file name (after last "/")
            fileName = filePath;
        } else {
            fileName++;
        }
        if (strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0) {
            continue;  // Ignores ./ and ../
        }
        char type = gopherType(filePath);
        lineSize = 1 + strlen(fileName) + strlen(filePath) + sizeof(GOPHER_DOMAIN) + sizeof(CRLF) + 6;
        if (lineSize > (line ? strlen(line) : 0)) {  // Realloc only if necessary
            if (NULL == (line = realloc(line, lineSize))) {
                sendErrorResponse(sock, SYS_ERR_MSG);
                goto ON_ERROR;
            }
        }
        if (snprintf(line, lineSize, "%c%s\t%s\t%s\t%hu" CRLF, type, fileName, filePath, GOPHER_DOMAIN, port) >= lineSize) {
            sendErrorResponse(sock, SYS_ERR_MSG);
            goto ON_ERROR;
        }
        if (PLATFORM_FAILURE == sendAll(sock, line, strlen(line))) {
            goto ON_ERROR;
        }
    }
    if (!(res & PLATFORM_END_OF_DIR)) {
        dir = NULL;
        sendErrorResponse(sock, (res & PLATFORM_NOT_FOUND) ? RESOURCE_NOT_FOUND_MSG : SYS_ERR_MSG);
        goto ON_ERROR;
    }
    if (send(sock, ".", 1, 0) < 1) {
        goto ON_ERROR;
    }
    closeSocket(sock);
    free(line);
    closeDir(dir);
    return GOPHER_SUCCESS;
ON_ERROR:
    if (line) free(line);
    if (dir) {
        closeDir(dir);
    }
    return GOPHER_FAILURE;
}

/** Function executed by a thread to send a file to the client.
 *  If the transfer succeeds, writes to the log. 
 * @param threadArgs a send_args_t compatible void pointer used as argument.
*/
static void* sendFileTask(void* threadArgs) {
    send_args_t args;
    struct sockaddr_in clientAddr;
    int clientLen;
    char address[16];
    file_size_t totalSize = 0;
    args = *(send_args_t*)threadArgs;
    free(threadArgs);
    printf("THREAD - acquired args: total: %d map: %p view: %p\n", args.totalPages, args.map, args.map->view);
    int count = 0;
    while (count < args.totalPages) {
        printf("THREAD - Sending page number %d of %d\n", count+1, args.totalPages);
        if (PLATFORM_FAILURE == sendAll(args.dest, args.map->view, args.map->size)) {
            debugMessage(SEND_ERR, DBG_ERR);
            closeSocket(args.dest);
            return NULL;
        }
        count++;
        totalSize += args.map->size;
    }
    printf("THREAD - all pages sent\n");
    if (PLATFORM_FAILURE == sendAll(args.dest, CRLF ".", 3)) {
            debugMessage(SEND_ERR, DBG_ERR);
            closeSocket(args.dest);
            return NULL;
    }
    if (args.map) unmapMem(args.map);
    clientLen = sizeof(clientAddr);
    if (SOCKET_ERROR == getpeername(args.dest, (struct sockaddr*)&clientAddr, &clientLen)) {
        memset(&clientAddr, 0, clientLen);
    }
    if (args.map) free(args.map);
    closeSocket(args.dest);
    inetNtoa(&clientAddr.sin_addr, address, sizeof(address));
    string_t logFormat = "File: %s | Size: %db | Sent to: %s:%i\n";
    if (args.pLogger) {
        size_t logSize;
        string_t log;
        logSize = snprintf(NULL, 0, logFormat, args.name, totalSize, address, clientAddr.sin_port) + 1;
        if (NULL == (log = malloc(logSize))) {
            return NULL;
        }
        if (snprintf(log, logSize, logFormat, args.name, totalSize, address, clientAddr.sin_port) > 0) {
            logTransfer(args.pLogger, log);
        }
        free(log);
    }
}

/** Starts the file transfer thread, executing sendFileTask().
 * @param name The name of the file (used for logging). Can be NULL iff pLogger is NULL. 
 * @param map A pointer to a file_mapping_t representing the memory mapping of the file.
 * @param sock The client socket.
 * @param pLogger A pointer to the logger_t representing the logging process (can be NULL).
 * @return GOPHER_SUCCESS or GOPHER_FAILURE.
 */
static int sendFile(cstring_t name, int sock, const logger_t* pLogger) {
    thread_t tid;
    send_args_t* args = NULL;
    semaphore_t mappingReady;
    file_mapping_t* map = NULL;

    if (NULL == (args = malloc(sizeof(send_args_t)))) {
        goto ON_ERROR;
    }
    if (NULL == (map = malloc(sizeof(file_mapping_t)))) {
        goto ON_ERROR;
    }
    map->memMap = NULL;
    map->size = 0;
    map->view = NULL;
    // TODO maxVal del semaforo
    if (PLATFORM_SUCCESS != initSemaphore(&mappingReady, 0, 10)) {
        goto ON_ERROR;
    }
    int offset = 0;
    int totalPages = 0;
    file_size_t size = getFileSize(name);
    printf("size: %lli\n", size);
    if (size < 0) {
        debugMessage("getFileSize failed", DBG_DEBUG);
        goto ON_ERROR;
    } else if (size == 0) {
        totalPages = 0;
        map->size = 0;
        map->view = "";
    } else {
        totalPages = (size / MMAP_CHUNK) + 1;
        printf("totalPages: %d\n", totalPages);
        printf("size: %lli\n", size);
        printf("chunk: %li\n", MMAP_CHUNK );
        if (PLATFORM_SUCCESS != getFileMap(name, map, offset, min(MMAP_CHUNK, size-offset))) {
            printf("fileMap err\n");
            goto ON_ERROR;
        }
        offset += map->size;
    }
    printf("view before thread: %p\n", map->view);
    args->map = map;
    args->totalPages = totalPages;
    args->dest = sock;
    args->pLogger = pLogger;
    strncpy(args->name, name, sizeof(args->name));
    printf("args ready\n");
    if (PLATFORM_SUCCESS != startThread(&tid, (LPTHREAD_START_ROUTINE)sendFileTask, args)) {
        free(args);
        return GOPHER_FAILURE;
    }
    printf("started thread\n");
    detachThread(tid);
    while (offset < size) {
        printf("sendFile entering cycle..\n");
        if (PLATFORM_SUCCESS != waitSemaphore(&mappingReady)) {
            printf("wait sem \n");
            goto ON_ERROR;
        }
        printf("producer ready to go\n");
        if (PLATFORM_SUCCESS != unmapMem(map)) {
            goto ON_ERROR;
        }
        if (PLATFORM_SUCCESS != getFileMap(name, map, offset, MMAP_CHUNK)) {
            debugMessage(FILE_MAP_ERR, DBG_ERR);
            goto ON_ERROR;
        }
        if (PLATFORM_SUCCESS != sigSemaphore(&mappingReady)) {
            printf("sig sem \n");
            goto ON_ERROR;
        }
        offset += map->size;
    }
    printf("returning from sendFile\n");
    // TODO destroy sem
    return GOPHER_SUCCESS;
ON_ERROR:
    // TODO destroy sem
    if (map) free(map);
    sendErrorResponse(sock, SYS_ERR_MSG);
    if (args) free(args);
    return GOPHER_FAILURE;

}

/** Executes the gopher protocol.
 *  Receives a selector and performs validateInput() and normalizePath(). If the selector points to a directory, calls sendDir().
 *  Else, a file_mapping_t is created using getFileMap() and is sent to the client using sendFile().
 *  @return GOPHER_SUCCESS or GOPHER_FAILURE.
 */
int gopher(socket_t sock, int port, const logger_t* pLogger) {
    string_t selector = NULL;
    size_t bytesRec = 0, selectorSize = 1;
    do {
        if (NULL == (selector = realloc(selector, selectorSize + BUFF_SIZE))) {
            debugMessage(ALLOC_ERR, DBG_ERR);
            goto ON_ERROR;
        }
        if ((bytesRec = recv(sock, selector + selectorSize - 1, BUFF_SIZE, 0)) <= 0) {
            debugMessage(bytesRec == 0 ? CONN_CLOS_ERR : RECV_ERR, bytesRec == 0 ? DBG_WARN : DBG_ERR);
            goto ON_ERROR;
        }
        selectorSize += bytesRec;
        selector[selectorSize - 1] = '\0';
    } while (bytesRec > 0 && !strstr(selector, CRLF));
    debugMessage(selector, DBG_DEBUG);
    if (!validateInput(selector)) {
        sendErrorResponse(sock, INVALID_SELECTOR);
        goto ON_ERROR;
    }
    normalizePath(selector);
    debugMessage(selector, DBG_INFO);
    int fileAttr = fileAttributes(selector);
    if (PLATFORM_FAILURE & fileAttr) {
        sendErrorResponse(sock, (PLATFORM_NOT_FOUND & fileAttr) ? RESOURCE_NOT_FOUND_MSG : SYS_ERR_MSG);
        goto ON_ERROR;
    } else if (PLATFORM_ISDIR & fileAttr) {  // Directory
        if (GOPHER_SUCCESS != sendDir(selector, sock, port)) {
            debugMessage(DIR_SEND_ERR, DBG_ERR);
            goto ON_ERROR;
        }
    } else {  // File
        if (GOPHER_SUCCESS != sendFile(selector, sock, pLogger)) {
            debugMessage(FILE_SEND_ERR, DBG_ERR);
            goto ON_ERROR;
        }
    }
    free(selector);
    return GOPHER_SUCCESS;
ON_ERROR:
    debugMessage(GOPHER_REQUEST_FAILED, DBG_WARN);
    closeSocket(sock);
    if (selector) free(selector);
    return GOPHER_FAILURE;
}

/*****************************************************************************************************************/
/*                                             WINDOWS FUNCTIONS                                                 */

/*****************************************************************************************************************/

#if defined(_WIN32)

#include <windows.h>

static char gopherType(LPCSTR filePath) {
    LPSTR ext;
    if (fileAttributes(filePath) & PLATFORM_ISDIR) {
        return GOPHER_DIR;
    }
    ext = strrchr(filePath, '.');
    if (!ext) {
        return GOPHER_UNKNOWN;
    }
    for (int i = 0; i < EXT_NO; i++) {
        if (strcmp(ext, extensions[i]) == 0) {
            if (CHECK_GRP(i, EXT_TXT)) {
                return GOPHER_TEXT;
            } else if (CHECK_GRP(i, EXT_HQX)) {
                return GOPHER_BINHEX;
            } else if (CHECK_GRP(i, EXT_DOS)) {
                return GOPHER_DOS;
            } else if (CHECK_GRP(i, EXT_BIN)) {
                return GOPHER_BINARY;
            } else if (CHECK_GRP(i, EXT_GIF)) {
                return GOPHER_GIF;
            } else if (CHECK_GRP(i, EXT_IMG)) {
                return GOPHER_IMAGE;
            }
        }
    }
    return GOPHER_UNKNOWN;
}

#else

/*****************************************************************************************************************/
/*                                             LINUX FUNCTIONS                                                    */

/*****************************************************************************************************************/

static char gopherType(const char* file) {
    if (fileAttributes(file) & PLATFORM_ISDIR) {
        return GOPHER_DIR;
    }
    char cmd[MAX_NAME];
    FILE* response;
    int bytesRead;
    if (snprintf(cmd, sizeof(cmd), "file \"%s\"", file) < 0) {
        return GOPHER_UNKNOWN;
    }
    response = popen(cmd, "r");
    bytesRead = fread(cmd, 1, sizeof(cmd), response);
    fclose(response);
    if (strstr(cmd, FILE_CMD_NOT_FOUND)) {
        return GOPHER_UNKNOWN;
    } else if (strstr(cmd, FILE_CMD_EXEC1) || strstr(cmd, FILE_CMD_EXEC2)) {
        return GOPHER_BINARY;
    } else if (strstr(cmd, FILE_CMD_IMG)) {
        return GOPHER_IMAGE;
    } else if (strstr(cmd, FILE_CMD_GIF)) {
        return GOPHER_GIF;
    } else if (strstr(cmd, FILE_CMD_TXT)) {
        return GOPHER_TEXT;
    }
    return GOPHER_UNKNOWN;
}

#endif
