#include "csapp.h"
#include "pbx.h"
#include "server.h"
#include "debug.h"


void *pbx_client_service(void *arg){
    int connfd = *((int *)arg);
    free(arg);
    pthread_detach(pthread_self());
    TU *tu = pbx_register(pbx, connfd);
    debug("server tid - %ld", pthread_self());

    FILE *fd = fdopen(connfd, "r");

    char *pickup = Malloc(strlen(tu_command_names[TU_PICKUP_CMD])+strlen(EOL)+1);
    strcpy(pickup, tu_command_names[TU_PICKUP_CMD]);
    strcat(pickup, EOL);
    char *hangup = Malloc(strlen(tu_command_names[TU_HANGUP_CMD])+strlen(EOL)+ 1);
    strcpy(hangup, tu_command_names[TU_HANGUP_CMD]);
    strcat(hangup, EOL);
    char *dial = Malloc(strlen(tu_command_names[TU_DIAL_CMD])+strlen("\0")+ 1);
    strcpy(dial, tu_command_names[TU_DIAL_CMD]);
    strcat(dial, "\0");
    char *chat = Malloc(strlen(tu_command_names[TU_CHAT_CMD])+strlen("\0")+ 1);
    strcpy(chat, tu_command_names[TU_CHAT_CMD]);
    strcat(chat, "\0");


    int current_sz = 64;
    int malloc_by = 64;
    char *cmd = malloc(malloc_by);
    //current_sz = current_max;
    int i = 0;
    int c;

    while ((c = fgetc(fd)) != EOF){
        if (feof(fd)) break;

        cmd[i]=(char)c;
        i++;

        if(current_sz == i){
            current_sz = malloc_by + i;
            cmd = realloc(cmd, current_sz);
        }

        if (c == '\n'){
            cmd[i] = '\0';
            if (strcmp(cmd, pickup) == 0){
                tu_pickup(tu);
                i = 0;
            }
            else if (strcmp(cmd, hangup) == 0){
                tu_hangup(tu);
                i = 0;
            }
            else{
                char *ptr = cmd;
                char *cmd2 = malloc(5);
                int j;
                for (j = 0; j < 4; j++){
                    cmd2[j] = (char)*ptr;
                    ptr++;
                }
                cmd2[4] = '\0';

                ptr++;
                if (strcmp(cmd2, dial) == 0){
                    //debug("dialing");
                    int ext = atoi(ptr);
                    tu_dial(tu, ext);
                    i = 0;
                }
                else if (strcmp(cmd2, chat) == 0){
                    //debug("chatting");
                    tu_chat(tu, ptr);
                    free(cmd);
                    cmd = malloc(malloc_by);
                    current_sz = malloc_by;
                    i = 0;
                }
                free(cmd2);
            }
            i = 0;
        }
    }
    cmd[i] = '\0';
    free(cmd);
    cmd = NULL;

    free(pickup);
    free(hangup);
    free(dial);
    free(chat);

    shutdown(connfd, SHUT_RD);
    pbx_unregister(pbx, tu);

    fclose(fd);
    close(connfd);

    //V(pbx->shutdown);
    return NULL;
}
