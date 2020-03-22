/** \file server.h 
 *  Functions to manage the main server.
 *  @file server.c
 */

#ifndef SERVER_H
#define SERVER_H

#if defined(_WIN32)
#else
#include <netinet/in.h>
#endif

#include <stdbool.h>

#include "datatypes.h"
#include "globals.h"
#include "logger.h"

#define READ_PORT 0x0001
#define READ_MULTIPROCESS 0x0002
#define SERVER_UPDATE 0x0001
#define SERVER_INIT 0x0002
// Error codes
#define SERVER_SUCCESS 0
#define SERVER_FAILURE -1

/** A struct representing an instance of a gopher server */
typedef struct {
    /** The socket of the server */
    socket_t sock;
    /** The socket address */
    struct sockaddr_in sockAddr;
    /** The port where the server is listening */
    unsigned short port;
    /** A flag stating whether the server should spawn a process or a thread per request */
    bool multiProcess;
} server_t;

/** Initializes a server socket and sets it to listening.
 *  Places the socket in pServer->sock. If flags is SERVER_UPDATE, the original socket in pServer->sock is closed.
 * @param pServer The server_t struct where the socket information will be stored.
 * @param flags Can be SERVER_UPDATE if a change of port was requested, or SERVER_INIT if the socket must be initialized from scratch.
 * @return SERVER_SUCCESS or SERVER_FAILURE.  
 */
int prepareSocket(server_t* pServer, int flags);

/** Installs the default signal handlers for the server process.
 * On Windows, CTRL_C interrupts the server and CTRL_BREAK requests a configuration update.
 * On Linux, SIGINT interrupts the server and SIGHUP requests a configuration update.
 * @return SERVER_SUCCESS or SERVER_FAILURE. 
*/
int installDefaultSigHandlers();

/** [Windows only] Initializes the Winsock DLL and the critical section for console signals.
 *  @return SERVER_SUCCESS or SERVER_FAILURE. 
 */
int initWsa();

/** Sets the default options. 
 * @param pServer A pointer to the server_t struct where the options will be stored.
 * @param which A flag stating the option to default. Can be READ_PORT, READ_MULTIPROCESS or both or-ed.
*/
void defaultConfig(server_t* pServer, int which);

/** Reads the options from a configuration file. 
 * @param pServer The server_t struct where the options will be stored.
 * @param which A flag stating the option to default. Can be READ_PORT, READ_MULTIPROCESS or both or-ed.
 * @return SERVER_SUCCESS or SERVER_FAILURE.  
*/
int readConfig(server_t* pServer, int which);

/** Starts a gopher server.
 * @param pServer A fully populated server_t struct representing the server to run.
 * @param pLogger A logger_t struct representing the transfer logging process (can be NULL).
 * @return SERVER_SUCCESS if the server is terminated by the user, SERVER_FAILURE otherwise.  
 */
int runServer(server_t* pServer, logger_t* pLogger);

#endif