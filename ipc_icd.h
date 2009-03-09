#ifndef IPC_ICD_H_
#define IPC_ICD_H_

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

void ipc_icd_quit(void);
int  ipc_icd_init(void (*status_cb)(int));

#ifdef __cplusplus
};
#endif

#endif /* IPC_ICD_H_ */
