/*
 * Copyright (c) 2010-2012 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "dague_internal.h"
#include <plasma.h>
#include "dplasma.h"
#include "dplasma/lib/dplasmaaux.h"
#include "dplasma/lib/dplasmatypes.h"

#include "map2.h"

dague_object_t *
dplasma_map2_New( PLASMA_enum uplo,
                  tiled_matrix_desc_t *A,
                  tiled_matrix_desc_t *B,
                  dague_operator_t operator,
                  void * op_args)
{
    dague_map2_object_t *dague_map2 = NULL;

    dague_map2 = dague_map2_new( uplo,
                                 *A, (dague_ddesc_t*)A,
                                 *B, (dague_ddesc_t*)B,
                                 operator, op_args);

    dplasma_add2arena_tile( dague_map2->arenas[DAGUE_map2_DEFAULT_ARENA],
                            1, DAGUE_ARENA_ALIGNMENT_SSE, MPI_INT, 1);

    return (dague_object_t*)dague_map2;
}

void
dplasma_map2_Destruct( dague_object_t *o )
{
    dague_map2_object_t *omap2 = (dague_map2_object_t *)o;

    dplasma_datatype_undefine_type( &(omap2->arenas[DAGUE_map2_DEFAULT_ARENA]->opaque_dtt) );

    DAGUE_INTERNAL_OBJECT_DESTRUCT(omap2);
}

void
dplasma_map2( dague_context_t *dague,
              PLASMA_enum uplo,
              tiled_matrix_desc_t *A,
              tiled_matrix_desc_t *B,
              dague_operator_t operator,
              void * op_args)
{
    dague_object_t *dague_map2 = NULL;

    dague_map2 = dplasma_map2_New( uplo, A, B, operator, op_args );

    if ( dague_map2 != NULL )
    {
        dague_enqueue( dague, dague_map2 );
        dplasma_progress( dague );
        dplasma_map2_Destruct( dague_map2 );
    }
}
