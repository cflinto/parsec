#ifndef _jdf2c_utils_h
#define _jdf2c_utils_h

#include "string_arena.h"

typedef char *(*dumper_function_t)(void **elt, void *arg);


/**
 * FLOW_IS_PARAMETRIZED:
 *    Tells whether a flow is parametrized or not.
 *  @param [IN] flow:         the flow to test.
 *
 *  @return a boolean value.
 */
#define FLOW_IS_PARAMETRIZED(flow) \
    ((flow)->local_variables != NULL)

/**
 * GET_PARAMETRIZED_FLOW_ITERATOR_NAME
 * @param flow: the flow from which to get the iterator
 * 
 * Returns a string, the name of the iterator
 */
#define GET_PARAMETRIZED_FLOW_ITERATOR_NAME(flow) \
    get_parametrized_flow_iterator_name(flow)

static inline char *get_parametrized_flow_iterator_name(jdf_dataflow_t const *flow)
{
    assert(FLOW_IS_PARAMETRIZED(flow));
    assert(flow->local_variables->next == NULL); // only one iterator for parametrized flows

    return flow->local_variables->alias;
}

/**
 * INDENTATION_IF_PARAMETRIZED:
 *    Returns an indentation string if the flow is parametrized.
 *  @param [IN] flow:         said flow.
 *
 *  @return a string containing an indentation if the flow is parametrized, an empty string otherwise.
 */
#define INDENTATION_IF_PARAMETRIZED(flow) \
    ((FLOW_IS_PARAMETRIZED(flow)) ? "  " : "")

/**
 * DUMP_ARRAY_OFFSET_IF_PARAMETRIZED:
 *    Tells whether a flow is parametrized or not.
 *  @param [IN] sa:           the string arena to use.
 *  @param [IN] flow:         the flow to test.
 *
 *  @return an empty string if not parametrized, [var] else (var being the name of the iterator variable).
 */
#define DUMP_ARRAY_OFFSET_IF_PARAMETRIZED(sa, flow) \
    util_dump_array_offset_if_parametrized(sa, flow)

/**
 * util_dump_array_offset_if_parametrized:
 *   function used by the DUMP_ARRAY_OFFSET_IF_PARAMETRIZED* macros. Do not use directly.
 */
static inline char*
util_dump_array_offset_if_parametrized(string_arena_t *sa, const jdf_dataflow_t *flow)
{
    // reinit sa
    string_arena_init(sa);

    if (FLOW_IS_PARAMETRIZED(flow)) {
        string_arena_add_string(sa, "[%s]", get_parametrized_flow_iterator_name(flow));
    }

    return string_arena_get_string(sa);
}

/**
 * @brief Dumps the proper access to the data, wether the flow is parametrized or not
 * 
 */
#define DUMP_DATA_FIELD_NAME_IN_TASK(sa, flow)\
    util_dump_data_field_name_in_task(sa, flow)

static inline char *util_dump_data_field_name_in_task(string_arena_t *sa, const jdf_dataflow_t *flow)
{
    string_arena_init(sa);

    if( FLOW_IS_PARAMETRIZED(flow) ) {
        string_arena_add_string(sa, "parametrized__f_%s(%s)", flow->varname, get_parametrized_flow_iterator_name(flow));
    } else {
        string_arena_add_string(sa, "_f_%s", flow->varname);
    }

    return string_arena_get_string(sa);
}

/**
 * @brief Dumps the flow_id's variable (when the task class has a parametrized flow or a referrer)
 * 
 */
#define DUMP_FLOW_ID_VARIABLE(sa, jdf_basename, function, flow)\
    util_dump_flow_id_variable(sa, jdf_basename, function, flow)

static inline char *util_dump_flow_id_variable(string_arena_t *sa, const char *jdf_basename, const jdf_function_entry_t *function, const jdf_dataflow_t *flow)
{
    string_arena_init(sa);

    // // if(flow->flow_flags & JDF_FLOW_TYPE_WRITE)
    // // {
    //     string_arena_add_string(sa, "(spec_%s.flow_id_of_flow_of_%s_%s_for_%s + %s)",
    //                 JDF_OBJECT_ONAME(function), jdf_basename, function->fname, flow->varname, get_parametrized_flow_iterator_name(flow));
    // // }
    // // else
    // // {
    // //     string_arena_add_string(sa, "DUNNO_HOW_TO_GET_ACTION_MASK_FOR_THIS_FLOW");
    // // }

    if( FLOW_IS_PARAMETRIZED(flow) ) {
        string_arena_add_string(sa, "(spec_%s.flow_id_of_flow_of_%s_%s_for_%s + %s)",
                    JDF_OBJECT_ONAME(function), jdf_basename, function->fname, flow->varname, get_parametrized_flow_iterator_name(flow));
    }
    else {
        string_arena_add_string(sa, "spec_%s.flow_id_of_flow_of_%s_%s_for_%s",
                    JDF_OBJECT_ONAME(function), jdf_basename, function->fname, flow->varname);
    }

    return string_arena_get_string(sa);
}

/**
 * @brief Dumps an expression that gives the number of flows in a task class, including each specialization of a parametrized flow
 * 
 */
#define DUMP_NUMBER_OF_FLOWS_IN_TASK_CLASS(sa, jdf_basename, function)\
    util_dump_number_of_flows_in_task_class(sa, jdf_basename, function)

static inline char *util_dump_number_of_flows_in_task_class(string_arena_t *sa, const char *jdf_basename, const jdf_function_entry_t *function)
{
    string_arena_init(sa);

    string_arena_add_string(sa, "(");

    // For each flow
    for(jdf_dataflow_t *flow = function->dataflow; flow != NULL; flow = flow->next) {
        if( FLOW_IS_PARAMETRIZED(flow) ) {
            string_arena_add_string(sa, " + spec_%s.nb_specializations_of_parametrized_flow_of_%s_%s_for_%s",
                        JDF_OBJECT_ONAME(function), jdf_basename, function->fname, flow->varname);
        } else {
            string_arena_add_string(sa, " + 1");
        }
    }

    string_arena_add_string(sa, ")");

    return string_arena_get_string(sa);
}

/** 
 * VARIABLE_IS_FLOW_LEVEL
 *   Tells whether a variable is a flow level variable or not.
 * @param [IN] var:           the variable to test.
 * @param [IN] flow:          the flow to test.
 * 
 * @return a boolean value.
 */
#define VARIABLE_IS_FLOW_LEVEL(flow, var) \
    variable_is_flow_level_util(flow, var)

static inline int variable_is_flow_level_util(const jdf_dataflow_t *flow, const jdf_expr_t *var)
{
    for(jdf_expr_t *flow_variable=flow->local_variables; flow_variable!=NULL; flow_variable=flow_variable->next) {
        if (strcmp(flow_variable->alias, var->alias) == 0) {
            return 1;
        }
    }

    return 0;
}

/**
 * JDF_ANY_FLOW_IS_PARAMETRIZED:
 *   Tells whether any flow is parametrized or not.
 *   Used to avoid code overloading if no paramtrized flow is present.
 * @param [IN] jdf:           the jdf to test.
 * 
 * @return a boolean value.
 */
#define JDF_ANY_FLOW_IS_PARAMETRIZED(jdf) \
    jdf_any_flow_is_parametrized_util(jdf)

static inline int jdf_any_flow_is_parametrized_util(const jdf_t *jdf)
{
    for( jdf_function_entry_t* f = jdf->functions; NULL != f; f = f->next ) {
        for( jdf_dataflow_t* df = f->dataflow; NULL != df; df = df->next ) {
            if( FLOW_IS_PARAMETRIZED(df) ) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * CALL_IS_PARAMETRIZED:
 * 
 * Tells whether a call is parametrized or not.
 * 
 * @param [IN] call:          the call to test.
 * 
 * @return a boolean value.
 */
#define CALL_IS_PARAMETRIZED(call) \
    call_is_parametrized_util(call)

static inline int call_is_parametrized_util(const jdf_call_t *call)
{
    return NULL != call->parametrized_offset;
}

/**
 * FLOW_ANY_DEP_IS_REFERRER
 * 
 * Tells whether any dependency of a flow is a referrer.
 * 
 * @param [IN] flow:          the flow to test.
 * 
 * @return a boolean value.
 */
#define FLOW_ANY_DEP_IS_REFERRER(flow) \
    flow_any_dep_is_referrer_util(flow)

static inline int flow_any_dep_is_referrer_util(const jdf_dataflow_t *flow)
{
    for( jdf_dep_t *dep = flow->deps; NULL != dep; dep = dep->next ) {
        for( int target_call=0; target_call<2; ++target_call ) {
            assert(dep->guard->guard_type==JDF_GUARD_UNCONDITIONAL || dep->guard->guard_type==JDF_GUARD_BINARY || dep->guard->guard_type==JDF_GUARD_TERNARY);
            if(dep->guard->guard_type!=JDF_GUARD_TERNARY && target_call==1)
            { // callfalse is only relevant for JDF_GUARD_UNCONDITIONAL and JDF_GUARD_BINARY
                continue;
            }
            jdf_call_t *call = target_call?dep->guard->callfalse:dep->guard->calltrue;
            assert(call);

            if( CALL_IS_PARAMETRIZED(call) )
            {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * FLOW_IS_PARAMETRIZED_OR_ANY_DEP_IS_REFERRER
 * 
 * Tells whether a flow is parametrized or not, or if any of its dependencies is a referrer.
 * 
 * @param [IN] flow:          the flow to test.
 * 
 * @return a boolean value.
 */
#define FLOW_IS_PARAMETRIZED_OR_ANY_DEP_IS_REFERRER(flow) \
    flow_is_parametrized_or_any_dep_is_referrer_util(flow)

static inline int flow_is_parametrized_or_any_dep_is_referrer_util(const jdf_dataflow_t *flow)
{
    return FLOW_IS_PARAMETRIZED(flow) || FLOW_ANY_DEP_IS_REFERRER(flow);
}

/**
 * TASK_CLASS_ANY_FLOW_IS_PARAMETRIZED_OR_REFERRER:
 *  Tells whether any flow is parametrized or if one of the deps is a referrer.
 * 
 * @param [IN] tc:            the task class to test.
 * 
 * @return a boolean value.
 */
#define TASK_CLASS_ANY_FLOW_IS_PARAMETRIZED_OR_REFERRER(tc) \
    task_class_any_flow_is_parametrized_or_referrer_util(tc)

static inline int task_class_any_flow_is_parametrized_or_referrer_util(const jdf_function_entry_t *tc)
{
    for( jdf_dataflow_t* df = tc->dataflow; NULL != df; df = df->next ) {
        if( FLOW_IS_PARAMETRIZED_OR_ANY_DEP_IS_REFERRER(df) ) {
            return 1;
        }
    }

    return 0;
}

/**
 * TASK_CLASS_ANY_FLOW_IS_PARAMETRIZED
 *  Tells whether any flow is parametrized
 * 
 * @param [IN] tc:            the task class to test.
 * 
 * @return a boolean value.
 */
#define TASK_CLASS_ANY_FLOW_IS_PARAMETRIZED(tc) \
    task_class_any_flow_is_parametrized_util(tc)

static inline int task_class_any_flow_is_parametrized_util(const jdf_function_entry_t *tc)
{
    for( jdf_dataflow_t* df = tc->dataflow; NULL != df; df = df->next ) {
        if( FLOW_IS_PARAMETRIZED(df) ) {
            return 1;
        }
    }

    return 0;
}


/**
 * STRING_IS_IN:
 * Tells whether a string is in a list of strings.
 * 
 */
#define STRING_IS_IN(string, arr, arr_size) \
    string_is_in_util(string, arr, arr_size)

static inline int string_is_in_util(const char *string, const char **arr, int arr_size)
{
    for( int i=0; i<arr_size; ++i ) {
        if( 0 == strcmp(string, arr[i]) ) {
            return 1;
        }
    }
    return 0;
}


/**
 * UTIL_DUMP_LIST_FIELD:
 *    Iterate over the elements of a list, transforming each field element in a string using a parameter function,
 *    and concatenate all strings.
 *    The function has the prototype  field_t **e, void *a -> char *strelt
 *    The final string has the format
 *       before (prefix strelt separator)* (prefix strelt) after
 *  @param [IN] arena:         string arena to use to add strings elements to the final string
 *  @param [IN] structure_ptr: pointer to a structure that implement any list
 *  @param [IN] nextfield:     the name of a field pointing to the next structure pointer
 *  @param [IN] eltfield:      the name of a field pointing to an element to print
 *  @param [IN] fct:           a function that transforms a pointer to an element to a string of characters
 *  @param [IN] fctarg:        fixed argument of the function
 *  @param [IN] before:        string (of characters) representing what must appear before the list
 *  @param [IN] prefix:        string (of characters) representing what must appear before each element
 *  @param [IN] separator:     string (of characters) that will be put between each element, but not at the end
 *                             or before the first
 *  @param [IN] after:         string (of characters) that will be put at the end of the list, after the last
 *                             element
 *
 *  @return a string (of characters) written in arena with the list formed so.
 *
 *  If the function fct return NULL, the element is ignored
 *
 *  Example: to create the list of expressions that is a parameter call, use
 *    UTIL_DUMP_LIST_FIELD(sa, jdf->functions->predicates, next, expr, dump_expr, NULL, "(", "", ", ", ")")
 *  Example: to create the list of declarations of globals, use
 *    UTIL_DUMP_LIST_FIELD(sa, jdf->globals, next, name, dumpstring, NULL, "", "  int ", ";\n", ";\n");
 */
#define UTIL_DUMP_LIST_FIELD(arena, structure_ptr, nextfield, eltfield, fct, fctarg, before, prefix, separator, after) \
    util_dump_list_fct( arena, structure_ptr,                           \
                        (char *)&(structure_ptr->nextfield)-(char *)structure_ptr, \
                        (char *)&(structure_ptr->eltfield)-(char *)structure_ptr, \
                            fct, fctarg, before, prefix, separator, after)

/**
 * UTIL_DUMP_LIST:
 *    Iterate over the elements of a list, transforming each element in a string using a parameter function,
 *    and concatenate all strings.
 *    The function has the prototype  list_elt_t **e, void *a -> char *strelt
 *    The final string has the format
 *       before (prefix strelt separator)* (prefix strelt) after
 *  @param [IN] arena:         string arena to use to add strings elements to the final string
 *  @param [IN] structure_ptr: pointer to a structure that implement any list
 *  @param [IN] nextfield:     the name of a field pointing to the next structure pointer
 *  @param [IN] fct:           a function that transforms a pointer to a list element to a string of characters
 *  @param [IN] fctarg:        fixed argument of the function
 *  @param [IN] before:        string (of characters) representing what must appear before the list
 *  @param [IN] prefix:        string (of characters) representing what must appear before each element
 *  @param [IN] separator:     string (of characters) that will be put between each element, but not at the end
 *                             or before the first
 *  @param [IN] after:         string (of characters) that will be put at the end of the list, after the last
 *                             element
 *
 *  If the function fct return NULL, the element is ignored
 *
 *  @return a string (of characters) written in arena with the list formed so.
 *
 *  Example: to create the list of expressions that is #define list of macros, transforming each element
 *            using both the name of the element and the number of parameters, use
 *          UTIL_DUMP_LIST(sa1, jdf->data, next, dump_data, sa2, "", "#define ", "\n", "\n"));
 */
#define UTIL_DUMP_LIST(arena, structure_ptr, nextfield, fct, fctarg, before, prefix, separator, after) \
    util_dump_list_fct( arena, structure_ptr,                           \
                        (char *)&(structure_ptr->nextfield)-(char *)structure_ptr, \
                        0, \
                        fct, fctarg, before, prefix, separator, after)

/**
 * util_dump_list_fct:
 *   function used by the UTIL_DUMP_LIST* macros. Do not use directly.
 */
static inline char*
util_dump_list_fct(string_arena_t *sa,
                   const void *firstelt, unsigned int next_offset, unsigned int elt_offset,
                   dumper_function_t fct, void *fctarg,
                   const char *before, const char *prefix, const char *separator, const char *after)
{
    char *eltstr;
    const char *prevstr = "";
    void *elt;

    string_arena_init(sa);

    string_arena_add_string(sa, "%s", before);

    while(firstelt != NULL) {
        elt = ((void **)((char*)(firstelt) + elt_offset));
        eltstr = fct(elt, fctarg);

        firstelt = *((void **)((char *)(firstelt) + next_offset));
        if( eltstr != NULL ) {
            string_arena_add_string(sa, "%s%s%s", prevstr, prefix, eltstr);
            prevstr = separator;
        }
    }

    string_arena_add_string(sa, "%s", after);

    return string_arena_get_string(sa);
}

jdf_def_list_t* jdf_create_properties_list( const char* name,
                                            int default_int,
                                            const char* default_char,
                                            jdf_def_list_t* next );

/**
 * Utilities to dump expressions and other parts of the internal
 * storage structure.
 */
typedef struct expr_info {
    struct string_arena* sa;
    const char* prefix;
    char*       assignments;
    int         nb_bound_locals;
    char**      bound_locals;
    const char* suffix;
} expr_info_t;

#define EMPTY_EXPR_INFO { .sa = NULL, .prefix = NULL, .assignments = NULL, .nb_bound_locals = 0, .bound_locals = NULL, .suffix = NULL }

/* The elem should be a jdf_expr_t while the arg should be an expr_info_t */
char * dump_expr(void **elem, void *arg);


#endif
