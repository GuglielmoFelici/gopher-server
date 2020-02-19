/** \file log.h
 * Contains constant messages for logging.
 */

#ifndef LOG_H
#define LOG_H

#define _GNU_SOURCE  // TODO rimuovere

#define MAIN_CONFIG_ERR "Can't read configuration file, default settings will be used."
#define MAIN_MULTIPROCESS_CONFIG_ERR "Can't read configuration file, multiprocess will be disabled."
#define MAIN_PORT_CONFIG_ERR "Can't read configuration for port, default one will be used."
#define MAIN_CWD_ERR "Can't get current working directory"
#define MAIN_STARTUP_ERR "Error starting up."
#define MAIN_LOOP_ERR "Server error."
#define MAIN_SELECT_ERR "Select failed."
#define MAIN_SOCKET_ERR "Error initializing server socket."
#define MAIN_LISTEN_ERR "Socket - listen failed."
#define MAIN_START_LOG_ERR "Error starting logger."
#define MAIN_CLOSE_LOG_ERR "Error stopping logger."
#define MAIN_DAEMON_ERR "Can't daemonize process."
#define MAIN_THREAD_ERR "Error starting a new thread."
#define MAIN_SYS_ERR "System error."
#define MAIN_CTRL_ERR "Error setting up console ctrl events handlers."
#define MAIN_WSA_ERR "Error initializing the WSA."
#define MAIN_USAGE "Available options:\n-h\tshow usage help\n-s\tDon't log on console/syslog\n-p=PORT\tsets the port to PORT\n-d=DIR\tchanges cwd of the server to DIR\n-m\tactivates multiprocess mode"
#define MAIN_STARTING "Starting server..."
#define FORK_FAILED "Fork failed."
#define BIND_ERR "Can't bind socket."
#define CLOSE_SOCKET_ERR "Errore closing socket."
#define ALLOC_ERR "Memory allocation failed."
#define SERVE_CLIENT_ERR "Error serving client."
#define LOGFILE_NAME_ERR "Log file name too long."
#define LOGFILE_OPEN_ERR "Error opening log file."
#define LOGFILE_WRITE_ERR "Error writing to log file"
#define MUTEX_INIT_ERR "Error initializing mutex."
#define MUTEX_LOCK_ERR "Can't lock mutex."
#define MUTEX_UNLOCK_ERR "Can't unlock mutex."
#define FILE_LOCK_ERR "Error locking file."
#define FILE_UNLOCK_ERR "Error unlocking file."
#define COND_INIT_ERR "Error initializing the condition variable."
#define COND_WAIT_ERR "Error waiting on the condition variable."
#define COND_SIGNAL_ERR "Error signaling on the condition variable."
#define PIPE_READ_ERR "Error reading from pipe."
#define PIPE_CLOSE_ERR "Can't close pipe."
#define FILE_MAP_ERR "Error mapping file."
#define FILE_SEND_ERR "Error sending requested file."
#define DIR_SEND_ERR "Error sending requested directory."
#define ARGS_ERR "Wrong arguments in function call."
#define RECV_ERR "Error receiving the selector"
#define GOPHER_REQUEST_FAILED "Serving gopher request failed."
#define SHUTDOWN_REQUESTED "Shutdown requested."
#define UPDATE_REQUESTED "Requested configuration update."
#define INCOMING_CONNECTION "Incoming connection."
#define TERMINATE_WITH_ERRORS "The program terminated with errors."

#endif