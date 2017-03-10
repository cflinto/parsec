#include "parsec_config.h"

/* system and io */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* parsec things */
#include "parsec.h"
#include "parsec/profiling.h"
#ifdef PARSEC_VTRACE
#include "parsec/vt_user.h"
#endif

#include "common_data.h"
#include "common_timing.h"
#include "parsec/interfaces/superscalar/insert_function_internal.h"
#include "dplasma/testing/common_timing.h"

#if defined(PARSEC_HAVE_STRING_H)
#include <string.h>
#endif  /* defined(PARSEC_HAVE_STRING_H) */

#if defined(PARSEC_HAVE_MPI)
#include <mpi.h>
#endif  /* defined(PARSEC_HAVE_MPI) */

double time_elapsed;
double sync_time_elapsed;

int
task_to_check_generation(parsec_execution_unit_t *context, parsec_execution_context_t *this_task)
{
    (void)context; (void)this_task;

    return PARSEC_HOOK_RETURN_DONE;
}

int main(int argc, char ** argv)
{
    parsec_context_t* parsec;
    int rank, world, cores;
    int nb, nt, parsec_argc;
    tiled_matrix_desc_t *ddescA;
    char** parsec_argv;

#if defined(PARSEC_HAVE_MPI)
    {
        int provided;
        MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &world);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#else
    world = 1;
    rank = 0;
#endif

    cores = -1;  /* everything on the node */
    parsec_argv = &argv[1];
    parsec_argc = argc - 1;
    if(argv[1] != NULL) {
        cores = atoi(argv[1]);
        parsec_argv++;
        parsec_argc--;
    }

    /* Creating parsec context and initializing dtd environment */
    parsec = parsec_init( cores, &parsec_argc, &parsec_argv );
    if( NULL == parsec ) {
        exit(-1);
    }

    /****** Checking task generation ******/
    parsec_handle_t *parsec_dtd_handle = parsec_dtd_handle_new(  );

    int i, j, total_tasks = 100000;

    if( 0 == rank ) {
        parsec_output( 0, "\nChecking task generation using dtd interface. "
                      "We insert 10000 tasks and atomically increase a global counter to see if %d task executed\n\n", total_tasks );
    }

    /* Registering the dtd_handle with PARSEC context */
    parsec_enqueue( parsec, parsec_dtd_handle );

    parsec_context_start(parsec);

    for( i = 0; i < 6; i++ ) {
        SYNC_TIME_START();
        for( j = 0; j < total_tasks; j++ ) {
            /* This task does not have any data associated with it, so it will be inserted in all mpi processes */
            parsec_insert_task( parsec_dtd_handle, task_to_check_generation,    0,  "sample_task",
                                0 );
        }

        parsec_dtd_handle_wait( parsec, parsec_dtd_handle );
        SYNC_TIME_PRINT(rank, ("\n"));
    }

    parsec_handle_free( parsec_dtd_handle );

    parsec_context_wait(parsec);

    parsec_fini(&parsec);

#ifdef PARSEC_HAVE_MPI
    MPI_Finalize();
#endif

    return 0;
}