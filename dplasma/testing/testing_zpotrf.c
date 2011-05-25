/*
 * Copyright (c) 2009-2010 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */

#include "common.h"
#include "data_dist/matrix/sym_two_dim_rectangle_cyclic/sym_two_dim_rectangle_cyclic.h"
#include "data_dist/matrix/two_dim_rectangle_cyclic/two_dim_rectangle_cyclic.h"
#if defined(HAVE_CUDA) && defined(PRECISION_s)
#include "cuda_sgemm.h"
#endif


#define FMULS_POTRF(N) ((N) * (1.0 / 6.0 * (N) + 0.5) * (N))
#define FADDS_POTRF(N) ((N) * (1.0 / 6.0 * (N)      ) * (N))

#define FMULS_POTRS(N, NRHS) ( (NRHS) * ( (N) * ((N) + 1.) ) )
#define FADDS_POTRS(N, NRHS) ( (NRHS) * ( (N) * ((N) - 1.) ) )

static int check_solution( dague_context_t *dague, PLASMA_enum uplo, 
                           tiled_matrix_desc_t *ddescA, tiled_matrix_desc_t *ddescB, tiled_matrix_desc_t *ddescX );

int main(int argc, char ** argv)
{
    dague_context_t* dague;
    int iparam[IPARAM_SIZEOF];

    /* Set defaults for non argv iparams */
    iparam_default_facto(iparam);
    iparam_default_ibnbmb(iparam, 0, 180, 180);
#if defined(HAVE_CUDA) && defined(PRECISION_s)
    iparam[IPARAM_NGPUS] = 0;
#endif

    /* Initialize DAGuE */
    dague = setup_dague(argc, argv, iparam);
    PASTE_CODE_IPARAM_LOCALS(iparam);

    /* initializing matrix structure */
    PLASMA_enum uplo = PlasmaLower;
    int info = 0;
    LDA = max( LDA, N );
    LDB = max( LDB, N );
    SMB = 1; SNB = 1;


    PASTE_CODE_ALLOCATE_MATRIX(ddescA, 1, 
        sym_two_dim_block_cyclic, (&ddescA, matrix_ComplexDouble, 
                                   nodes, cores, rank, MB, NB, LDA, N, 0, 0, 
                                   N, N, P, uplo));

    /* load the GPU kernel */
#if defined(HAVE_CUDA) && defined(PRECISION_s)
    if(iparam[IPARAM_NGPUS] > 0)
    {
        if(loud) printf("+++ Load GPU kernel ... ");
        if(0 != zpotrf_cuda_init(dague, (tiled_matrix_desc_t *)&ddescA))
        {
            fprintf(stderr, "XXX Unable to load GPU kernel.\n");
            exit(3);
        }
        if(loud) printf("Done\n");
    }
#endif

    if(!check) 
    {
        PASTE_CODE_FLOPS_COUNT(FADDS_POTRF, FMULS_POTRF, ((DagDouble_t)N));

        /* matrix generation */
        if(loud > 2) printf("+++ Generate matrices ... ");
        generate_tiled_random_sym_pos_mat((tiled_matrix_desc_t*) &ddescA, 100);
        if(loud > 2) printf("Done\n");

#if defined(LLT_LL)
        PASTE_CODE_ENQUEUE_KERNEL(dague, zpotrf_ll, 
                                  (uplo, (tiled_matrix_desc_t*)&ddescA, &info));
        PASTE_CODE_PROGRESS_KERNEL(dague, zpotrf_ll);
#else
        PASTE_CODE_ENQUEUE_KERNEL(dague, zpotrf, 
                                  (uplo, (tiled_matrix_desc_t*)&ddescA, &info));
        PASTE_CODE_PROGRESS_KERNEL(dague, zpotrf);

        dplasma_zpotrf_Destruct( DAGUE_zpotrf );
#endif
    }
#if 0 /* THIS IS ALL WRONG, you cannot use a two_dim_block_cyclic and fill it with sym_pos_mat (what is the value of uplo if you do that ?) */
    else 
    {
        int u, t1, t2;
        int info_solution;

       PASTE_CODE_ALLOCATE_MATRIX(ddescA0, 1, 
          two_dim_block_cyclic, (&ddescA0, matrix_ComplexDouble, 
                                 nodes, cores, rank, MB, NB, LDA, N, 0, 0, 
                                 N, N, SMB, SNB, P));
       PASTE_CODE_ALLOCATE_MATRIX(ddescB, 1, 
            two_dim_block_cyclic, (&ddescB, matrix_ComplexDouble, 
                                   nodes, cores, rank, MB, NB, LDB, NRHS, 0, 0, 
                                   N, NRHS, SMB, SNB, P));
       PASTE_CODE_ALLOCATE_MATRIX(ddescX, 1, 
            two_dim_block_cyclic, (&ddescX, matrix_ComplexDouble, 
                                   nodes, cores, rank, MB, NB, LDB, NRHS, 0, 0, 
                                   N, NRHS, SMB, SNB, P));
        
        for ( u=0; u<2; u++) {
            if ( uplo[u] == PlasmaUpper ) {
                t1 = PlasmaConjTrans; t2 = PlasmaNoTrans;
            } else {
                t1 = PlasmaNoTrans; t2 = PlasmaconjTrans;
            }   

#if 0
            /*********************************************************************
             *               First Check
             */
            if ( rank == 0 ) {
                printf("***************************************************\n");
            }

            /* matrix generation */
            printf("Generate matrices ... ");
            generate_tiled_random_sym_pos_mat((tiled_matrix_desc_t *) &ddescA,  400);
            generate_tiled_random_sym_pos_mat((tiled_matrix_desc_t *) &ddescA0, 400);
            generate_tiled_random_mat((tiled_matrix_desc_t *) &ddescB, 200);
            generate_tiled_random_mat((tiled_matrix_desc_t *) &ddescX, 200);
            printf("Done\n");


            /* Compute */
            printf("Compute ... ... ");
            info = dplasma_zposv(dague, uplo[u], (tiled_matrix_desc_t *)&ddescA, (tiled_matrix_desc_t *)&ddescB );
            printf("Done\n");
            printf("Info = %d\n", info);

            /* Check the solution */
            info_solution = check_solution( dague, uplo[u], (tiled_matrix_desc_t *)&ddescA0, (tiled_matrix_desc_t *)&ddescB, (tiled_matrix_desc_t *)&ddescX);

            if ( rank == 0 ) {
                if (info_solution == 0) {
                    printf(" ----- TESTING ZPOSV (%s) ....... PASSED !\n", uplostr[u]);
                }
                else {
                    printf(" ----- TESTING ZPOSV (%s) ... FAILED !\n", uplostr[u]);
                }
                printf("***************************************************\n");
            }
#endif
            /*********************************************************************
             *               Second Check
             */
            if ( rank == 0 ) {
                printf("***************************************************\n");
            }

            /* matrix generation */
            printf("Generate matrices ... ");
            generate_tiled_random_sym_pos_mat((tiled_matrix_desc_t *) &ddescA,  400);
            generate_tiled_random_sym_pos_mat((tiled_matrix_desc_t *) &ddescA0, 400);
            generate_tiled_random_mat((tiled_matrix_desc_t *) &ddescB, 200);
            generate_tiled_random_mat((tiled_matrix_desc_t *) &ddescX, 200);
            printf("Done\n");


            /* Compute */
            printf("Compute ... ... ");
            info = dplasma_zpotrf(dague, uplo[u], (tiled_matrix_desc_t *)&ddescA );
            if ( info == 0 ) {
                dplasma_zpotrs(dague, uplo[u], (tiled_matrix_desc_t *)&ddescA, (tiled_matrix_desc_t *)&ddescX );
            }
            printf("Done\n");
            printf("Info = %d\n", info);

            /* Check the solution */
            info_solution = check_solution( dague, uplo[u], (tiled_matrix_desc_t *)&ddescA0, (tiled_matrix_desc_t *)&ddescB, (tiled_matrix_desc_t *)&ddescX);

            if ( rank == 0 ) {
                if (info_solution == 0) {
                    printf(" ----- TESTING ZPOTRF + ZPOTRS (%s) ....... PASSED !\n", uplostr[u]);
                }
                else {
                    printf(" ----- TESTING ZPOTRF + ZPOTRS (%s) ... FAILED !\n", uplostr[u]);
                }
                printf("***************************************************\n");
            }

            /*********************************************************************
             *               Third Check
             */
            if ( rank == 0 ) {
                printf("***************************************************\n");
            }

            /* matrix generation */
            printf("Generate matrices ... ");
            generate_tiled_random_sym_pos_mat((tiled_matrix_desc_t *) &ddescA,  400);
            generate_tiled_random_sym_pos_mat((tiled_matrix_desc_t *) &ddescA0, 400);
            generate_tiled_random_mat((tiled_matrix_desc_t *) &ddescB, 200);
            generate_tiled_random_mat((tiled_matrix_desc_t *) &ddescX, 200);
            printf("Done\n");


            /* Compute */
            printf("Compute ... ... ");
            info = dplasma_zpotrf(dague, uplo[u], (tiled_matrix_desc_t *)&ddescA );
            if ( info == 0 ) {
                dplasma_ztrsm(dague, PlasmaLeft, uplo[u], t1, PlasmaNonUnit, 1.0, (tiled_matrix_desc_t *)&ddescA, (tiled_matrix_desc_t *)&ddescX);
                dplasma_ztrsm(dague, PlasmaLeft, uplo[u], t2, PlasmaNonUnit, 1.0, (tiled_matrix_desc_t *)&ddescA, (tiled_matrix_desc_t *)&ddescX);
            }
            printf("Done\n");
            printf("Info = %d\n", info);

            /* Check the solution */
            info_solution = check_solution( dague, uplo[u], (tiled_matrix_desc_t *)&ddescA0, (tiled_matrix_desc_t *)&ddescB, (tiled_matrix_desc_t *)&ddescX);

            if ( rank == 0 ) {
                if (info_solution == 0) {
                    printf(" ----- TESTING ZPOTRF + ZTRSM + ZTRSM (%s) ....... PASSED !\n", uplostr[u]);
                }
                else {
                    printf(" ----- TESTING ZPOTRF + ZTRSM + ZTRSM (%s) ... FAILED !\n", uplostr[u]);
                }
                printf("***************************************************\n");
            }

        }

        dague_data_free(ddescA.mat);
        dague_ddesc_destroy( (dague_ddesc_t*)&ddescA);
        dague_data_free(ddescA0.mat);
        dague_ddesc_destroy( (dague_ddesc_t*)&ddescA0);
        dague_data_free(ddescB.mat);
        dague_ddesc_destroy( (dague_ddesc_t*)&ddescB);
        dague_data_free(ddescX.mat);
        dague_ddesc_destroy( (dague_ddesc_t*)&ddescX);
    }
#endif

#if defined(HAVE_CUDA) && defined(PRECISION_s)
    if(iparam[IPARAM_NGPUS] > 0) 
    {
        zpotrf_cuda_fini(dague);
    }
#endif


    dague_data_free(ddescA.mat);
    dague_ddesc_destroy( (dague_ddesc_t*)&ddescA);
    cleanup_dague(dague);


    return EXIT_SUCCESS;
}

static int check_solution( dague_context_t *dague, PLASMA_enum uplo, 
                           tiled_matrix_desc_t *ddescA, tiled_matrix_desc_t *ddescB, tiled_matrix_desc_t *ddescX )
{
    int info_solution;
    double Rnorm, Anorm, Bnorm, Xnorm, result;
    int N = ddescB->m;
    int NRHS = ddescB->n;
    double *work = (double *)malloc(N*sizeof(double));
    double eps = LAPACKE_dlamch_work('e');
    Dague_Complex64_t *W;

    W = (Dague_Complex64_t *)malloc( N*max(N,NRHS)*sizeof(Dague_Complex64_t));

    twoDBC_ztolapack( (two_dim_block_cyclic_t *)ddescA, W, N );
    Anorm = LAPACKE_zlanhe_work( LAPACK_COL_MAJOR, 'i', lapack_const(uplo), N, W, N, work );

    twoDBC_ztolapack( (two_dim_block_cyclic_t *)ddescB, W, N );
    Bnorm = LAPACKE_zlange_work( LAPACK_COL_MAJOR, 'i', N, NRHS, W, N, work );

    twoDBC_ztolapack( (two_dim_block_cyclic_t *)ddescX, W, N );
    Xnorm = LAPACKE_zlange_work( LAPACK_COL_MAJOR, 'i', N, NRHS, W, N, work );

    dplasma_zgemm( dague, PlasmaNoTrans, PlasmaNoTrans, -1.0, ddescA, ddescX, 1.0, ddescB);

    twoDBC_ztolapack( (two_dim_block_cyclic_t *)ddescB, W, N );
    Rnorm = LAPACKE_zlange_work( LAPACK_COL_MAJOR, 'i', N, NRHS, W, N, work );

    if (getenv("DPLASMA_TESTING_VERBOSE"))
        printf( "||A||_oo = %e, ||X||_oo = %e, ||B||_oo= %e, ||A X - B||_oo = %e\n", 
                Anorm, Xnorm, Bnorm, Rnorm );

    result = Rnorm / ( ( Anorm * Xnorm + Bnorm ) * N * eps ) ;
    printf("============\n");
    printf("Checking the Residual of the solution \n");
    printf("-- ||Ax-B||_oo/((||A||_oo||x||_oo+||B||_oo).N.eps) = %e \n", result);

    if (  isnan(Xnorm) || isinf(Xnorm) || isnan(result) || isinf(result) || (result > 60.0) ) {
        info_solution = 1;
     }
    else{
        info_solution = 0;
    }

    free(work); free(W);
    return info_solution;
}
