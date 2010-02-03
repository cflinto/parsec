/*
 * Copyright (c) 2009      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

/* /!\  THIS FILE IS NOT INTENDED TO BE COMPILED ON ITS OWN
 *      It should be included from remote_dep.c if USE_MPI is defined
 */

#include <mpi.h>

#define USE_MPI_THREAD_PROGRESS

static int __remote_dep_mpi_init(dplasma_context_t* context);
static int __remote_dep_mpi_fini(dplasma_context_t* context);
static int __remote_dep_send(dplasma_execution_context_t* task, int rank, void **data);
static int __remote_dep_progress(dplasma_execution_unit_t* eu_context);

#ifdef USE_MPI_THREAD_PROGRESS
    static int remote_dep_thread_init(dplasma_context_t* context);
    static int remote_dep_thread_fini(dplasma_context_t* fini);
    static int remote_dep_thread_send(const dplasma_execution_context_t* task, int rank, void **data);
    static int remote_dep_thread_progress(dplasma_execution_unit_t* eu_context);
#   define remote_dep_mpi_init(ctx) remote_dep_thread_init(ctx)
#   define remote_dep_mpi_fini(ctx) remote_dep_thread_fini(ctx)
#   define remote_dep_send(task, rank, data) remote_dep_thread_send(task, rank, data)
#   define remote_dep_progress(ctx) remote_dep_thread_progress(ctx)
#else
#   define remote_dep_mpi_init(ctx) __remote_dep_mpi_init(ctx)
#   define remote_dep_mpi_fini(ctx) __remote_dep_mpi_fini(ctx)
#   define remote_dep_send(task, rank, data) __remote_dep_send(task, rank, data)
#   define remote_dep_progress(ctx) __remote_dep_progress(ctx)
#endif 


int __remote_dep_init(dplasma_context_t* context)
{
    return remote_dep_mpi_init(context);
}

int __remote_dep_fini(dplasma_context_t* context)
{
    return remote_dep_mpi_fini(context);
}


int dplasma_remote_dep_activate_rank(dplasma_execution_unit_t* eu_context, 
                                     const dplasma_execution_context_t* origin,
                                     const param_t* origin_param,
                                     int rank, void** data)
{
#ifdef _DEBUG
    char tmp[128];
#endif
    
    assert(rank >= 0);
    assert(rank < eu_context->master_context->nb_nodes);
    if(dplasma_remote_dep_is_forwarded(eu_context, rank))
    {    
        return 0;
    }
    dplasma_remote_dep_mark_forwarded(eu_context, rank);
    DEBUG(("Realease %s deps\ttrigger REMOTE process rank %d\n", dplasma_service_to_string(origin, tmp, 128), rank ));
    
    /* make sure we don't leave before serving all data deps */
    dplasma_atomic_inc_32b( &(eu_context->master_context->taskstodo) );
    return remote_dep_send(origin, rank, data);
}

int dplasma_remote_dep_activate(dplasma_execution_unit_t* eu_context,
                                const dplasma_execution_context_t* origin,
                                const param_t* origin_param,
                                const dplasma_execution_context_t* exec_context,
                                const param_t* dest_param )
{
    int rank; 
#ifdef _DEBUG
    char tmp[128];
    char tmp2[128];
#endif
    
    rank = dplasma_remote_dep_compute_grid_rank(eu_context, origin, exec_context);
    assert(rank >= 0);
    assert(rank < eu_context->master_context->nb_nodes);
    if(dplasma_remote_dep_is_forwarded(eu_context, rank))
    {    
        return 0;
    }
    dplasma_remote_dep_mark_forwarded(eu_context, rank);
    DEBUG(("%s -> %s\ttrigger REMOTE process rank %d\n", dplasma_service_to_string(origin, tmp2, 128), dplasma_service_to_string(exec_context, tmp, 128), rank ));
    return remote_dep_send(origin, rank, NULL);
}


int dplasma_remote_dep_progress(dplasma_execution_unit_t* eu_context)
{
    return remote_dep_progress(eu_context);
}




/* TODO: smart use of dplasma context instead of ugly globals */
#define DEP_NB_CONCURENT 16
static MPI_Comm dep_comm;
static MPI_Request dep_req[4 * DEP_NB_CONCURENT];
static MPI_Request* dep_activate_req = &dep_req[0];
static MPI_Request* dep_get_req = &dep_req[DEP_NB_CONCURENT];
static MPI_Request* dep_put_rcv_req = &dep_req[2 * DEP_NB_CONCURENT];
static MPI_Request* dep_put_snd_req = &dep_req[3 * DEP_NB_CONCURENT];
/* TODO: fix heterogeneous restriction by using proper mpi datatypes */
#define dep_dtt MPI_BYTE
#define dep_count sizeof(dplasma_execution_context_t)
static dplasma_execution_context_t dep_activate_buff[DEP_NB_CONCURENT];
#define data_dtt MPI_LONG_LONG
static void* dep_get_buff[DEP_NB_CONCURENT];

static int __remote_dep_mpi_init(dplasma_context_t* context)
{
    int i, np;
    MPI_Comm_dup(MPI_COMM_WORLD, &dep_comm);
    MPI_Comm_size(dep_comm, &np);

    for(i = 0; i < DEP_NB_CONCURENT; i++)
    {        
        MPI_Recv_init(&dep_activate_buff[i], dep_count, dep_dtt, MPI_ANY_SOURCE, REMOTE_DEP_ACTIVATE_TAG, dep_comm, &dep_activate_req[i]);
        MPI_Start(&dep_activate_req[i]);
        MPI_Recv_init(&dep_get_buff[i], 1, data_dtt, MPI_ANY_SOURCE, REMOTE_DEP_GET_DATA_TAG, dep_comm, &dep_get_req[i]);
        MPI_Start(&dep_get_req[i]);
        dep_put_rcv_req[i] = MPI_REQUEST_NULL;
        dep_put_snd_req[i] = MPI_REQUEST_NULL;
    }
    return np;
}

static int __remote_dep_mpi_fini(dplasma_context_t* context)
{
    int i;
    
    for(i = 0; i < DEP_NB_CONCURENT; i++)
    {
        MPI_Request_free(&dep_activate_req[i]);
        MPI_Request_free(&dep_req[i]);        
    }
    
    MPI_Comm_free(&dep_comm);
    return 0;
}


static void remote_dep_put_data(void* data, int to, int i);
static void remote_dep_get_data(const dplasma_execution_context_t* task, int from, int i);

static int __remote_dep_progress(dplasma_execution_unit_t* eu_context)
{
#ifdef _DEBUG
    char tmp[128];
#endif
    MPI_Status status;
    int ret = 0;
    int i, flag;
    
    do {
        MPI_Testany(4 * DEP_NB_CONCURENT, dep_req, &i, &flag, &status);
        if(flag)
        {
            if(REMOTE_DEP_ACTIVATE_TAG == status.MPI_TAG)
            {
                DEBUG(("%s\tFROM REMOTE process rank %d (concurent %d)\n", dplasma_service_to_string(&dep_activate_buff[i], tmp, 128), status.MPI_SOURCE, i));
                remote_dep_get_data(&dep_activate_buff[i], status.MPI_SOURCE, i);
            } 
            else if(REMOTE_DEP_GET_DATA_TAG == status.MPI_TAG)
            {
                i -= DEP_NB_CONCURENT; /* shift i */
                remote_dep_put_data(dep_get_buff[i], status.MPI_SOURCE, i);
            }
            else 
            {
                assert(REMOTE_DEP_PUT_DATA_TAG == status.MPI_TAG);
                i -= DEP_NB_CONCURENT * 2;
                assert(i >= 0);
                if(i < DEP_NB_CONCURENT)
                {
                    dep_activate_buff[i].function->release_deps(eu_context, &dep_activate_buff[i], 0, &dep_activate_buff[i].list_item.cache_friendly_emptiness);
                    MPI_Start(&dep_activate_req[i]);
                    ret++;
                }
                else
                {
                    /* We finished sending the data, allow for more requests 
                     * to be processed */
                    i -= DEP_NB_CONCURENT;
                    MPI_Start(&dep_get_req[i]);
                    /* Allow for termination if needed */
                    dplasma_atomic_dec_32b( &(eu_context->master_context->taskstodo) );
                }
            }
        }
    } while(flag);
    return ret;
}

#define TILE_SIZE (120 * 120)

static void remote_dep_put_data(void* data, int to, int i)
{
    MPI_Isend(data, TILE_SIZE, MPI_DOUBLE, to, REMOTE_DEP_PUT_DATA_TAG, dep_comm, &dep_put_snd_req[i]);
}

static void remote_dep_get_data(const dplasma_execution_context_t* task, int from, int i)
{
    MPI_Send(&dep_activate_buff[i].list_item.cache_friendly_emptiness, 1, data_dtt, from, REMOTE_DEP_GET_DATA_TAG, dep_comm);
    dep_activate_buff[i].list_item.cache_friendly_emptiness = malloc(sizeof(double) * TILE_SIZE);
    MPI_Irecv(dep_activate_buff[i].list_item.cache_friendly_emptiness, TILE_SIZE, 
              MPI_DOUBLE, from, REMOTE_DEP_PUT_DATA_TAG, dep_comm, &dep_put_rcv_req[i]);
}


/* Send the activate tag */
static int __remote_dep_send(dplasma_execution_context_t* task, int rank, void **data)
{
    task->list_item.cache_friendly_emptiness = data[0];
    return MPI_Send((void*) task, dep_count, dep_dtt, rank, REMOTE_DEP_ACTIVATE_TAG, dep_comm);
}



#ifdef USE_MPI_THREAD_PROGRESS

#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

#define YIELD_TIME 500
static void init_ts(struct timespec* ts)
{
#if defined(__gnu_linux__)
    clock_gettime(CLOCK_REALTIME, ts);
#else
    gettimeofday((struct timeval*) ts, NULL);
    ts->tv_nsec *= 1000;
#endif
}

static inline void update_ts(struct timespec* ts, long nsec) 
{
    ts->tv_nsec += nsec;
    while(ts->tv_nsec > 1000000000)
    {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000;
    }
}
    
pthread_t dep_thread_id;
pthread_cond_t dep_msg_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t dep_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dep_seq_mutex = PTHREAD_MUTEX_INITIALIZER;
typedef enum {WANT_ZERO, WANT_SEND, WANT_RECV, WANT_FINI} dep_signal_reason_t;
volatile dep_signal_reason_t dep_signal_reason;
volatile int dep_ret;

volatile int enable_self_progress;
volatile int np;

dplasma_execution_context_t *dep_send_context;
void **dep_send_data;
int dep_send_rank;

dplasma_execution_unit_t *dep_recv_eu_context;


static void* remote_dep_thread_main(dplasma_context_t* context)
{
    int ret;
    int keep_probing = 1;
    struct timespec ts;
    
    np = __remote_dep_mpi_init(context);
    
    init_ts(&ts);
    
    pthread_mutex_lock(&dep_msg_mutex);
    do {
        switch(dep_signal_reason)
        {                
            case WANT_SEND:
                dep_ret = __remote_dep_send(dep_send_context, dep_send_rank, dep_send_data);
                break;
            case WANT_RECV:
                dep_ret = __remote_dep_progress(dep_recv_eu_context);
                break;
            case WANT_FINI:
                keep_probing = 0;
                break;
            case WANT_ZERO:
                if(enable_self_progress)
                {
                    __remote_dep_progress(&context->execution_units[0]);
                }
                update_ts(&ts, YIELD_TIME);
                ret = pthread_cond_timedwait(&dep_msg_cond, &dep_msg_mutex, &ts);
                assert((0 == ret) || (ETIMEDOUT == ret));
                continue;
        }
        dep_signal_reason = WANT_ZERO;
    } while(keep_probing);
    pthread_mutex_unlock(&dep_msg_mutex);

    __remote_dep_mpi_fini(context);    
    return context;
}

static int remote_dep_thread_init(dplasma_context_t* context)
{
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setscope(&thread_attr, PTHREAD_SCOPE_SYSTEM);
    
    enable_self_progress = 0;
    np = 0;
    dep_signal_reason = WANT_ZERO;

    pthread_create( &dep_thread_id,
                    &thread_attr,
                    (void* (*)(void*))remote_dep_thread_main,
                    (void*)context);

    while(0 == np); /* wait until the thread inits MPI */
    return np;
}

static int remote_dep_thread_fini(dplasma_context_t* context)
{
    dplasma_context_t *ret;
    
    pthread_mutex_lock(&dep_seq_mutex);
    pthread_mutex_lock(&dep_msg_mutex);
    
    dep_signal_reason = WANT_FINI;
    
    pthread_cond_signal(&dep_msg_cond);
    pthread_mutex_unlock(&dep_msg_mutex);
    
    pthread_join(dep_thread_id, (void**) &ret);
    assert(ret == context);
    
    pthread_mutex_unlock(&dep_seq_mutex);

    return 0;
}

static int remote_dep_thread_send(const dplasma_execution_context_t* task, int rank, void **data)
{
    int ret; 
    
    pthread_mutex_lock(&dep_seq_mutex);
    pthread_mutex_lock(&dep_msg_mutex);
    
    dep_ret = -1;
    dep_send_context = (dplasma_execution_context_t*) task;
    dep_send_data = data;
    dep_send_rank = rank;
    dep_signal_reason = WANT_SEND;
    
    pthread_cond_signal(&dep_msg_cond);
    pthread_mutex_unlock(&dep_msg_mutex);
    
    while(-1 == dep_ret);
    ret = dep_ret;
    
    pthread_mutex_unlock(&dep_seq_mutex);
    return ret;
}

static int remote_dep_thread_progress(dplasma_execution_unit_t* eu_context)
{
    int ret;

    pthread_mutex_lock(&dep_seq_mutex);
    pthread_mutex_lock(&dep_msg_mutex);
    
    enable_self_progress = 1;
    
    dep_ret = -1;
    dep_recv_eu_context = eu_context;
    dep_signal_reason = WANT_RECV;
    
    pthread_cond_signal(&dep_msg_cond);
    pthread_mutex_unlock(&dep_msg_mutex);
    
    while(-1 == dep_ret);
    ret = dep_ret;
    
    pthread_mutex_unlock(&dep_seq_mutex);
    return ret;
}

#endif 

