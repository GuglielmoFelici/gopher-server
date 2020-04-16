#include "../headers/protocol.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../headers/datatypes.h"
#include "../headers/log.h"
#include "../headers/logger.h"
#include "../headers/platform.h"

#define MMAP_CHUNK 800000000 // 800MB
#define DEFAULT_SEM_TIMEOUT 15000

/** Path to the windows gopher helper executable */
string_t winHelperPath = NULL;

typedef struct {
    file_mapping_t* map;
    socket_t dest;
    char name[MAX_NAME];
    const logger_t* pLogger;
    semaphore_t* producer;
    semaphore_t* consumer;
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

static void sendFileTaskLog(const logger_t* pLogger, socket_t sock, string_t path, file_size_t bytesSent) {
    if (!pLogger) {
        return;
    }
    struct sockaddr_in clientAddr;
    int clientLen;
    char address[16];
    clientLen = sizeof(clientAddr);
    if (SOCKET_ERROR == getpeername(sock, (struct sockaddr*)&clientAddr, &clientLen)) {
        memset(&clientAddr, 0, clientLen);
    }
    inetNtoa(&clientAddr.sin_addr, address, sizeof(address));
    string_t logFormat = "File: %s | Size: %lldb | Sent to: %s:%i\n";
        size_t logSize;
        string_t log;
        string_t name;
        if (NULL == (name = strrchr(path, '/'))) {  // Isolates file name (after last "/")
            name = path;
        } else {
            name++;
        }
        logSize = snprintf(NULL, 0, logFormat, name, bytesSent, address, clientAddr.sin_port) + 1;
        if (NULL == (log = malloc(logSize))) {
            return;
        }
        if (snprintf(log, logSize, logFormat, name, bytesSent, address, clientAddr.sin_port) > 0) {
            logTransfer(pLogger, log);
        }
        free(log);
}

/** Function executed by a thread to send a file to the client.
 *  If the transfer succeeds, writes to the log. 
 * @param threadArgs a send_args_t compatible void pointer used as argument.
*/
static void* sendFileTask(void* threadArgs) {
    send_args_t args;
    file_size_t sent = 0;
    if (!threadArgs) {
        goto ON_ERROR;
    }
    args = *(send_args_t*)threadArgs;
    free(threadArgs);
    while (sent < args.map->totalSize) {
        printf("THREAD - Sending from MB %lld to %lld\n", sent, sent + args.map->size);
        if (PLATFORM_FAILURE == sendAll(args.dest, args.map->view, args.map->size)) {
            goto ON_ERROR;
        }
        printf("page sent\n");
        sent += args.map->size;
        if (sent < args.map->totalSize) {
            sigSemaphore(args.consumer);
            waitSemaphore(args.producer, DEFAULT_SEM_TIMEOUT);
        }
    }
    printf("THREAD - all pages sent\n");
    if (PLATFORM_FAILURE == sendAll(args.dest, CRLF ".", 3)) {
            goto ON_ERROR;
    }
    if (args.map) {
        unmapMem(args.map);
        CloseHandle(args.map->memMap);
        free(args.map);
        args.map = NULL;
    } 
    sendFileTaskLog(args.pLogger, args.dest, args.name, sent);
    closeSocket(args.dest);
    return NULL;
ON_ERROR:
    debugMessage(SEND_ERR, DBG_ERR);
    closeSocket(args.dest);
    if (args.map) {
        unmapMem(args.map);
        CloseHandle(args.map->memMap);
        free(args.map);
    } 
    return NULL;
}

/** Starts the file transfer thread, executing sendFileTask().
 * @param name The name of the file (used for logging). Can be NULL iff pLogger is NULL. 
 * @param map A pointer to a file_mapping_t representing the memory mapping of the file.
 * @param sock The client socket.
 * @param pLogger A pointer to the logger_t representing the logging process (can be NULL).
 * @return GOPHER_SUCCESS or GOPHER_FAILURE.
 */
static int sendFile(cstring_t name, int sock, const logger_t* pLogger) {
    send_args_t* args = NULL;
    file_mapping_t* map = NULL;
    semaphore_t producer, consumer;
    if (NULL == (args = malloc(sizeof(send_args_t)))) {
        goto ON_ERROR;
    }
    if (NULL == (map = malloc(sizeof(file_mapping_t)))) {
        goto ON_ERROR;
    }
    map->memMap = NULL;
    map->size = 0;
    map->view = NULL;
    file_size_t offset = 0;
    map->totalSize = getFileSize(name);
    if (map->totalSize < 0) {
        goto ON_ERROR;
    } else if (map->totalSize > 0){
        if (PLATFORM_SUCCESS != initSemaphore(&producer, 0, 1)) {
            goto ON_ERROR;
        }
        if (PLATFORM_SUCCESS != initSemaphore(&consumer, 0, 1)) {
            goto ON_ERROR;
        }
        if (PLATFORM_SUCCESS != getFileMap(name, map, offset, MMAP_CHUNK)) {
            debugMessage("map failed", DBG_DEBUG);
            goto ON_ERROR;
        }
        offset += map->size;
    }
    args->map = map;
    args->dest = sock;
    args->pLogger = pLogger;
    args->producer = &producer;
    args->consumer = &consumer;
    strncpy(args->name, name, sizeof(args->name));
    thread_t tid;
    if (PLATFORM_SUCCESS != startThread(&tid, (LPTHREAD_START_ROUTINE)sendFileTask, args)) {
        goto ON_ERROR;
    }
    detachThread(tid);
    while (offset < map->totalSize) {
        if (PLATFORM_SUCCESS != waitSemaphore(&consumer, DEFAULT_SEM_TIMEOUT)) {
            goto ON_ERROR;
        }
        if (PLATFORM_SUCCESS != unmapMem(map)) {
            goto ON_ERROR;
        }
        if (PLATFORM_SUCCESS != getFileMap(name, map, offset, MMAP_CHUNK)) {
            debugMessage(FILE_MAP_ERR, DBG_ERR);
            goto ON_ERROR;
        }
        if (PLATFORM_SUCCESS != sigSemaphore(&producer)) {
            goto ON_ERROR;
        }
        offset += map->size;
    }
    destroySemaphore(&producer); 
    destroySemaphore(&consumer);
    return GOPHER_SUCCESS;
ON_ERROR:
    destroySemaphore(&producer); 
    destroySemaphore(&consumer);
    sendErrorResponse(sock, SYS_ERR_MSG);
    if (map) {
        unmapMem(map);
        CloseHandle(map->memMap);
        free(map);
    }
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
