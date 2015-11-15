/*
 * Copyright (c) 2014-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "vector.h"
#include <stdarg.h>
#include "dague/debug.h"
#include "dague/data_distribution.h"
#include "dague/data_internal.h"
#include "dague/data.h"

typedef struct {
    dague_ddesc_t super;
    int32_t start_rank;
    int32_t block_size;
    int32_t total_size;
    int32_t nb_blocks;
    struct dague_data_copy_s* data;
    int32_t* ptr;
} vector_datatype_t;

static uint32_t rank_of(dague_ddesc_t *desc, ...)
{
    vector_datatype_t *dat = (vector_datatype_t*)desc;
    va_list ap;
    int k;

    va_start(ap, desc);
    k = va_arg(ap, int);
    va_end(ap);

    return (k + dat->start_rank) % dat->super.nodes;
}

static int32_t vpid_of(dague_ddesc_t *desc, ...)
{
    int k;
    va_list ap;

    va_start(ap, desc);
    k = va_arg(ap, int);
    va_end(ap);

    (void)k;

    return 0;
}

static dague_data_t* data_of(dague_ddesc_t *desc, ...)
{
    vector_datatype_t *dat = (vector_datatype_t*)desc;
    va_list ap;
    int k;

    va_start(ap, desc);
    k = va_arg(ap, int);
    va_end(ap);

    (void)k;

    if(NULL == dat->data) {
        dat->data = dague_data_copy_new(NULL, 0);
        dat->data->device_private = dat->ptr;
    }
    return (void*)(dat->data);
}

#if defined(DAGUE_PROF_TRACE)
static uint32_t data_key(dague_ddesc_t *desc, ...)
{
    int k;
    va_list ap;

    va_start(ap, desc);
    k = va_arg(ap, int);
    va_end(ap);

    return (uint32_t)k;
}
#endif

dague_ddesc_t*
create_vector(int me, int world, int start_rank,
              int block_size, int total_size)
{
    vector_datatype_t *m = (vector_datatype_t*)calloc(1, sizeof(vector_datatype_t));
    dague_ddesc_t *d = &(m->super);

    d->myrank  = me;
    d->nodes   = world;

    d->rank_of = rank_of;
    d->data_of = data_of;
    d->vpid_of = vpid_of;
#if defined(DAGUE_PROF_TRACE)
    asprintf(&d->key_dim, "(%d)", (total_size+block_size-1)%total_size);
    d->key_base = NULL;
    d->data_key = data_key;
#endif

    m->start_rank = start_rank;
    m->block_size = block_size;
    m->total_size = total_size;
    m->nb_blocks  = (total_size + block_size - 1) / block_size;
    m->data = NULL;
    m->ptr = (int32_t*)malloc(m->nb_blocks * sizeof(int32_t));
    return d;
}

void release_vector(dague_ddesc_t *d)
{
    vector_datatype_t *m = (vector_datatype_t*)d;
    if(NULL != m->data) {
        DAGUE_DATA_COPY_RELEASE(m->data);
    }
    free(m->ptr);
    dague_ddesc_destroy(d);
    free(d);
}