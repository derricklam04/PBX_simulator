#include <stdio.h>

#include "pbx.h"
#include "csapp.h"
#include "debug.h"
//#include <pthread.h>

struct tu{
    pthread_t tid;
    int extension;
    int FD;
    int state;
    int callfrom;
    sem_t mutex; // protect access to state
};

struct pbx{
    TU *registry[PBX_MAX_EXTENSIONS];
    int extensions[PBX_MAX_EXTENSIONS];
    sem_t mutex;
    sem_t shutdown;

};
/*
 * Initialize a new PBX.
 *
 * @return the newly initialized PBX, or NULL if initialization fails.
 */
PBX *pbx_init(){
    pbx = Malloc(sizeof(struct pbx));
    for (int i =0; i<PBX_MAX_EXTENSIONS; i++){
        pbx->extensions[i]= -1;
    }
    //PBX *ptr = &pbx;
    Sem_init(&pbx->mutex, 0, 1);
    Sem_init(&pbx->shutdown, 0, 1);


    return pbx;
}

/*
 * Shut down a pbx, shutting down all network connections, waiting for all server
 * threads to terminate, and freeing all associated resources.
 * If there are any registered extensions, the associated network connections are
 * shut down, which will cause the server threads to terminate.
 * Once all the server threads have terminated, any remaining resources associated
 * with the PBX are freed.  The PBX object itself is freed, and should not be used again.
 *
 * @param pbx  The PBX to be shut down.
 */
void pbx_shutdown(PBX *pbx){
    //(&pbx->shutdown);

    for(int i =0; i<PBX_MAX_EXTENSIONS; i++){
        //V(&pbx->shutdown);
        if (pbx->extensions[i] != -1){

            debug("shutdown: tid - %ld", pbx->registry[i]->tid);
            P(&pbx->shutdown);
            int fd = pbx->registry[i]->FD;
            shutdown(fd, SHUT_RD);
            //V(&pbx->shutdown);
            //pthread_cancel(pbx->registry[i]->tid);

        }
        sleep(0.9);
    }
    //V(&pbx->shutdown);
    //P(&pbx->shutdown);
    //sleep(0.5);
    free(pbx);
    //V(&pbx->shutdown);
    return;
}

/*
 * Register a TU client with a PBX.
 * This amounts to "plugging a telephone unit into the PBX".
 * The TU is assigned an extension number and it is initialized to the TU_ON_HOOK state.
 * A notification of the assigned extension number is sent to the underlying network
 * client.
 *
 * @param pbx  The PBX.
 * @param fd  File descriptor providing access to the underlying network client.
 * @return A TU object representing the client TU, if registration succeeds, otherwise NULL.
 * The caller is responsible for eventually calling pbx_unregister to free the TU object
 * that was returned.
 */
TU *pbx_register(PBX *pbx, int fd){
    //V(&pbx->mutex);

    P(&pbx->mutex);
    TU *tu = Malloc(sizeof(struct tu));

    tu->tid = pthread_self();
    tu->FD = fd;
    tu->extension = fd;
    tu->state = TU_ON_HOOK;
    tu->callfrom = -1;
    Sem_init(&tu->mutex, 0, 1);

    pbx->registry[fd] = tu;
    pbx->extensions[fd] = tu->extension;
    dprintf(fd,"%s %d\n", tu_state_names[tu->state],fd);
    fsync(fd);
    V(&pbx->mutex);
    return tu;
}

/*
 * Unregister a TU from a PBX.
 * This amounts to "unplugging a telephone unit from the PBX".
 *
 * @param pbx  The PBX.
 * @param tu  The TU to be unregistered.
 * This object is freed as a result of the call and must not be used again.
 * @return 0 if unregistration succeeds, otherwise -1.
 */
int pbx_unregister(PBX *pbx, TU *tu){
    //P(&pbx->mutex);
    int fd = tu_fileno(tu);
    debug("unregistering tid - %ld", pbx->registry[fd]->tid);

    //V(&tu->mutex2);
    P(&tu->mutex);
    if (tu->state != TU_ON_HOOK){
        int callfromextension = tu->callfrom;
        if (callfromextension != -1){
            TU *callfromtu = pbx->registry[callfromextension];
            int callfromfd = callfromtu->FD;
            if (tu->state == TU_CONNECTED || tu->state == TU_RINGING){
                P(&callfromtu->mutex);
                callfromtu->state = TU_DIAL_TONE;
                callfromtu->callfrom = -1; // end connection
                dprintf(callfromfd,"%s\n", tu_state_names[callfromtu->state]);
                V(&callfromtu->mutex);
            }
            else if (tu->state == TU_RING_BACK){
                P(&callfromtu->mutex);
                callfromtu->state = TU_ON_HOOK;
                callfromtu->callfrom = -1;
                dprintf(callfromfd,"%s %d\n", tu_state_names[callfromtu->state], callfromfd);
                V(&callfromtu->mutex);
            }
            //fsync(callfromfd);
        }
        //P(&tu->mutex);
        tu->state = TU_ON_HOOK;
        tu->callfrom = -1; // reset callfrom
        //dprintf(fd,"%s %d\n", tu_state_names[tu->state], fd);
        //V(&tu->mutex);
    }
    V(&tu->mutex);

    P(&pbx->mutex);
    pbx->extensions[fd] = -1;

    free(tu);
    V(&pbx->mutex);

    V(&pbx->shutdown);

    return 0;
}

/*
 * Get the file descriptor for the network connection underlying a TU.
 * This file descriptor should only be used by a server to read input from
 * the connection.  Output to the connection must only be performed within
 * the PBX functions.
 *
 * @param tu
 * @return the underlying file descriptor, if any, otherwise -1.
 */
int tu_fileno(TU *tu){
    int filedes = tu->FD;
    return filedes;
}

/*
 * Get the extension number for a TU.
 * This extension number is assigned by the PBX when a TU is registered
 * and it is used to identify a particular TU in calls to tu_dial().
 * The value returned might be the same as the value returned by tu_fileno(),
 * but is not necessarily so.
 *
 * @param tu
 * @return the extension number, if any, otherwise -1.
 */
int tu_extension(TU *tu){
    int extension = tu->extension;
    return extension;
}

/*
 * Take a TU receiver off-hook (i.e. pick up the handset).
 *
 *   If the TU was in the TU_ON_HOOK state, it goes to the TU_DIAL_TONE state.
 *   If the TU was in the TU_RINGING state, it goes to the TU_CONNECTED state,
 *     reflecting an answered call.  In this case, the calling TU simultaneously
 *     also transitions to the TU_CONNECTED state.
 *   If the TU was in any other state, then it remains in that state.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU. In addition, if the new state is TU_CONNECTED, then the
 * calling TU is also notified of its new state.
 *
 * @param tu  The TU that is to be taken off-hook.
 * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 * an underlying I/O or other implementation error; a transition to the TU_ERROR
 * state (with no underlying implementation error) is considered a normal occurrence
 * and would result in 0 being returned.
 */
int tu_pickup(TU *tu){
    //sleep(0.1);
    //V(&tu->mutex);
    //V(&tu->mutex2);

    P(&tu->mutex);
    int fd = tu_fileno(tu);
    if (tu->state == TU_ON_HOOK){
        //P(&tu->mutex);
        tu->state = TU_DIAL_TONE;
        //V(&tu->mutex);
        dprintf(fd,"%s\n", tu_state_names[tu->state]);

    }
    else if (tu->state == TU_RINGING){
        int callfromextension = tu->callfrom;
        //debug("callfrom - %d", callfromextension);
        TU *callfromtu = pbx->registry[callfromextension];
        int callfromfd = callfromtu->extension;

        P(&callfromtu->mutex);
        callfromtu->state = TU_CONNECTED;
        callfromtu->callfrom = tu->extension; // callfrom connected
        //tu->callfrom = -1; // reset callfrom
        V(&callfromtu->mutex);

        //P(&tu->mutex);
        tu->state = TU_CONNECTED;
        //V(&tu->mutex);


        dprintf(fd,"%s %d\n", tu_state_names[tu->state], callfromtu->extension);
        dprintf(callfromfd,"%s %d\n", tu_state_names[callfromtu->state], tu->extension);
        //fsync(callfromfd);
    }
    else{
        if (tu->state == TU_ON_HOOK)
            dprintf(fd,"%s %d\n", tu_state_names[tu->state], fd);
        else if (tu->state == TU_CONNECTED)
            dprintf(fd,"%s %d\n", tu_state_names[tu->state], tu->callfrom);
        else dprintf(fd,"%s\n", tu_state_names[tu->state]);
    }
    V(&tu->mutex);
    //fsync(fd);

    return 0;
}


 // * Hang up a TU (i.e. replace the handset on the switchhook).
 // *
 // *   If the TU was in the TU_CONNECTED state, then it goes to the TU_ON_HOOK state.
 // *     In addition, in this case the peer TU (the one to which the call is currently
 // *     connected) simultaneously transitions to the TU_DIAL_TONE state.
 // *   If the TU was in the TU_RING_BACK state, then it goes to the TU_ON_HOOK state.
 // *     In addition, in this case the calling TU (which is in the TU_RINGING state)
 // *     simultaneously transitions to the TU_ON_HOOK state.
 // *   If the TU was in the TU_DIAL_TONE, TU_BUSY_SIGNAL, or TU_ERROR state,
 // *     then it goes to the TU_ON_HOOK state.
 // *   If the TU was in any other state, then there is no change of state.
 // *
 // * In all cases, a notification of the new state is sent to the network client
 // * underlying this TU.  In addition, if the previous state was TU_CONNECTED or
 // * TU_RING_BACK, then the peer or calling TU is also notified of its new state.
 // *
 // * @param tu  The tu that is to be hung up.
 // * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 // * an underlying I/O or other implementation error; a transition to the TU_ERROR
 // * state (with no underlying implementation error) is considered a normal occurrence
 // * and would result in 0 being returned.

int tu_hangup(TU *tu){
       // sleep(0.1);
    //V(&tu->mutex);
    //V(&tu->mutex2);
    P(&tu->mutex);
    int fd = tu_fileno(tu);
    if (tu->state != TU_RINGING && tu->state != TU_ON_HOOK){


        int callfromextension = tu->callfrom;
        if (callfromextension != -1){
            TU *callfromtu = pbx->registry[callfromextension];
            int callfromfd = callfromtu->FD;
            if (tu->state == TU_CONNECTED){
                P(&callfromtu->mutex);
                callfromtu->state = TU_DIAL_TONE;
                callfromtu->callfrom = -1; // end connection
                dprintf(callfromfd,"%s\n", tu_state_names[callfromtu->state]);
                V(&callfromtu->mutex);
            }
            else if (tu->state == TU_RING_BACK){
                P(&callfromtu->mutex);
                callfromtu->state = TU_ON_HOOK;
                callfromtu->callfrom = -1;
                dprintf(callfromfd,"%s %d\n", tu_state_names[callfromtu->state], callfromfd);
                V(&callfromtu->mutex);
            }
            //fsync(callfromfd);
        }
        //P(&tu->mutex);
        tu->state = TU_ON_HOOK;
        tu->callfrom = -1; // reset callfrom
        dprintf(fd,"%s %d\n", tu_state_names[tu->state], fd);
        //V(&tu->mutex);

    }
    else{
        if (tu->state == TU_ON_HOOK)
            dprintf(fd,"%s %d\n", tu_state_names[tu->state], fd);
        else dprintf(fd,"%s\n", tu_state_names[tu->state]);
    }
    V(&tu->mutex);
    //fsync(fd);
    return 0;
}

/*
 * Dial an extension on a TU.
 *
 *   If the specified extension number does not refer to any currently registered
 *     extension, then the TU transitions to the TU_ERROR state.
 *   Otherwise, if the TU was in the TU_DIAL_TONE state, then what happens depends
 *     on the current state of the dialed extension:
 *       If the dialed extension was in the TU_ON_HOOK state, then the calling TU
 *         transitions to the TU_RING_BACK state and the dialed TU simultaneously
 *         transitions to the TU_RINGING state.
 *       If the dialed extension was not in the TU_ON_HOOK state, then the calling
 *         TU transitions to the TU_BUSY_SIGNAL state and there is no change to the
 *         state of the dialed extension.
 *   If the TU was in any state other than TU_DIAL_TONE, then there is no state change.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU.  In addition, if the new state is TU_RING_BACK, then the
 * called extension is also notified of its new state (i.e. TU_RINGING).
 *
 * @param tu  The tu on which the dialing operation is to be performed.
 * @param ext  The extension to be dialed.
 * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 * an underlying I/O or other implementation error; a transition to the TU_ERROR
 * state (with no underlying implementation error) is considered a normal occurrence
 * and would result in 0 being returned.
 */
int tu_dial(TU *tu, int ext){
    P(&tu->mutex);
    int fd = tu_fileno(tu);
    if (tu->state != TU_DIAL_TONE){
        if (tu->state == TU_ON_HOOK)
            dprintf(fd,"%s %d\n", tu_state_names[tu->state], fd);
        else if (tu->state == TU_CONNECTED)
            dprintf(fd,"%s %d\n", tu_state_names[tu->state], tu->callfrom);
        else dprintf(fd,"%s\n", tu_state_names[tu->state]);
        //fsync(fd);
        V(&tu->mutex);
        return 0;
    }
    if (ext > PBX_MAX_EXTENSIONS || pbx->extensions[ext] != ext){
        //P(&tu->mutex);
        tu->state = TU_ERROR;
        dprintf(fd,"%s\n", tu_state_names[tu->state]);
        //V(&tu->mutex);
    }
    else{
        TU *dialedtu = pbx->registry[ext];

        int dialedfd = tu_fileno(dialedtu);
        //debug("dialing after chat: tu->state : %d", tu->state);
        if (tu->state == TU_DIAL_TONE){

            if (dialedtu->state == TU_ON_HOOK){
                P(&dialedtu->mutex);
                dialedtu->state = TU_RINGING;
                dialedtu->callfrom = tu->extension;
                dprintf(dialedfd,"%s\n", tu_state_names[dialedtu->state]);
                V(&dialedtu->mutex);

                //P(&tu->mutex);
                tu->state = TU_RING_BACK;
                tu->callfrom = dialedtu->extension;
                dprintf(fd,"%s\n", tu_state_names[tu->state]);
                //V(&tu->mutex);

            }
            else{
                //V(&dialedtu->mutex);
                //P(&tu->mutex);
                tu->state = TU_BUSY_SIGNAL;
                dprintf(fd,"%s\n", tu_state_names[tu->state]);
                //V(&tu->mutex);
            }
            //fsync(dialedfd);
        }
        else{
            if (tu->state == TU_ON_HOOK)
                dprintf(fd,"%s %d\n", tu_state_names[tu->state], fd);
            else dprintf(fd,"%s\n", tu_state_names[tu->state]);
        }
    }
    V(&tu->mutex);
    //fsync(fd);

    return 0;
}

/*
 * "Chat" over a connection.
 *
 * If the state of the TU is not TU_CONNECTED, then nothing is sent and -1 is returned.
 * Otherwise, the specified message is sent via the network connection to the peer TU.
 * In all cases, the states of the TUs are left unchanged.
 *
 * @param tu  The tu sending the chat.
 * @param msg  The message to be sent.
 * @return 0  If the chat was successfully sent, -1 if there is no call in progress
 * or some other error occurs.
 */
int tu_chat(TU *tu, char *msg){
    //V(&tu->mutex);
    //V(&tu->mutex2);
    //V(&tu->mutex2);

    P(&tu->mutex);
    int fd = tu_fileno(tu);
    if (tu->state != TU_CONNECTED){
        if (tu->state == TU_ON_HOOK)
            dprintf(fd,"%s %d\n", tu_state_names[tu->state], fd);
        else dprintf(fd,"%s\n", tu_state_names[tu->state]);
        V(&tu->mutex);
        //fsync(fd);
        return -1;
    }
    else{
        int connectedwithext = tu->callfrom;
        TU *connectedwithtu = pbx->registry[connectedwithext];
        int connectedwithfd = connectedwithtu->FD;

        char message[MAXLINE]="";
        int i = 0;
        while(*msg != '\n'){
            message[i] = *msg;
            msg++;
            i++;
        }
        //msg[i] = '\0';

        dprintf(fd, "CONNECTED %d\n", connectedwithfd);
        dprintf(connectedwithfd, "CHAT %s\n", message);
        //fsync(connectedwithfd);
        //fsync(fd);
    }
    V(&tu->mutex);
    return 0;
}