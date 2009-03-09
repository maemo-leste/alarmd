#ifndef IPC_EXEC_H_
#define IPC_EXEC_H_

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

int ipc_exec_run_command(const char *cmd);
int ipc_exec_init       (void);
int ipc_exec_quit       (void);

#ifdef __cplusplus
};
#endif

#endif /* IPC_EXEC_H_ */
