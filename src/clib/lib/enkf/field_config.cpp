#include <cstdlib>
#include <cstring>

#include <ert/util/string_util.h>
#include <ert/util/util.h>

#include <ert/ecl/ecl_endian_flip.h>
#include <ert/ecl/ecl_grid.h>

#include <ert/enkf/config_keys.hpp>
#include <ert/enkf/enkf_defaults.hpp>
#include <ert/enkf/enkf_types.hpp>
#include <ert/enkf/field_config.hpp>
#include <ert/enkf/field_trans.hpp>

/**
   About transformations and truncations
   -------------------------------------

   The values of the fields data can be automagically manipulated through two methods:

   * You can specify a min and a max value which will serve as truncation.

   * You can specify transformation functions which are applied to the field as follows:

     init_transform: This function is applied to the field when the
        field is loaded the first time, i.e. initialized. It is *NOT*
        applied under subsequent loads of dynamic fields during the
        execution.

     output_transform: This function is applied to the field before it
        is exported to eclipse.

     input_transform: This function is applied each time a field is
        loaded in from the forward model; i.e. this transformation
        applies to dynamic fields.



                                                            _______________________________         ___
                                                           /                               \        /|\
                                                           | Forward model (i.e. ECLIPSE)  |         |
                                                           | generates dynamic fields like |         |
                                                           | PRESSURE and SATURATIONS      |         |
                                                           \_______________________________/         |     This code is run
                                                                          |                          |     every time a field
                                                                          |                          |     is loaded FROM the
                                                                         \|/                         |     forward model into
                                                                          |                          |     EnKF.
                                                                  ________|_________                 |
                                                                 /                  \                |
                                                                 | Input transform  |                |
                                                                 \__________________/                |
                                                                          |                          |
                                                                          |                          |
                                                                         \|/                         |
                                                                          |                          |
                                                          ________________|__________________      _\|/_
_______________                       ___________        /                                   \
               \                     /           \       |  The internal representation      |
 Geo Modelling |                     | init-     |       |  of the field. This (should)      |
 creates a     |==>===============>==| transform |===>===|  be a normally distributed        |
 realization   |                     |           |       |  variable suitable for updates    |
_______________/                     \___________/       |  with EnKF.                       |
                                                         \___________________________________/   ___
|<----   This path is ONLY executed during INIT ------->|                  |                     /|\
         Observe that there is no truncation                              \|/                     |
         on load.                                                 _________|__________            |
                                                                 /                    \           |   This code is run
                                                                 |  Output transform  |           |   every time a field
                                                                 \____________________/           |   is exported from
                                                                           |                      |   enkf to the forward
                                                                          \|/                     |   model - i.e. ECLIPSE.
                                                                  _________|__________            |
                                                                 /                    \           |
                                                                 | Truncate min/max   |           |
                                                                 \____________________/           |
                                                                           |                      |
                                                                          \|/                     |
                                                                  _________|__________            |
                                                                 /                    \           |
                                                                 |    FORWARD MODEL   |           |
                                                                 \____________________/         _\|/_


Observe the following convention:

    global_index:  [0 , nx*ny*nz)
    active_index:  [0 , nactive)
*/
struct field_config_struct {
    /** Name/key ... */
    char *ecl_kw_name;
    /** The number of elements in the three directions. */
    int data_size, nx, ny, nz;
    /** Whether the data contains only active cells or active and inactive
     * cells */
    bool keep_inactive_cells;
    /** A shared reference to the grid this field is defined on. */
    char *grid;

    /** How the field should be trunacted before exporting for simulation, and
     * for the inital import. OR'd combination of truncation_type from enkf_types.h*/
    int truncation;
    /** The min value used in truncation. */
    double min_value;
    /** The maximum value used in truncation. */
    double max_value;

    field_file_format_type export_format;
    field_file_format_type import_format;
    char *output_field_name;

    field_type_enum type;
    /** Internalize a (pointer to) a table of the available transformation functions. */
    field_trans_table_type *trans_table;
    /** Function to apply to the data before they are exported - NULL: no transform. */
    field_func_type *output_transform;
    /** Function to apply on the data when they are loaded the first time -
     * i.e. initialized. NULL : no transform*/
    field_func_type *init_transform;
    /** Function to apply on the data when they are loaded from the forward
     * model - i.e. for dynamic data. */
    field_func_type *input_transform;

    char *output_transform_name;
    char *init_transform_name;
    char *input_transform_name;
};

field_file_format_type
field_config_default_export_format(const char *filename) {
    field_file_format_type export_format = FILE_FORMAT_NULL;
    if (filename != NULL) {
        export_format =
            ECL_KW_FILE_ALL_CELLS; /* Suitable for PERMX/PORO/... ; when this export format is
                                                used IMPORT must be used in the datafile instead of
                                                INCLUDE. This gives faster ECLIPSE startup time, but is
                                                (unfortunately) quite unstandard. */

        char *extension;
        util_alloc_file_components(filename, NULL, NULL, &extension);
        if (extension != NULL) {
            util_strupr(extension);
            if (strcmp(extension, "GRDECL") == 0)
                export_format = ECL_GRDECL_FILE;
            else if (strcmp(extension, "ROFF") == 0)
                export_format = RMS_ROFF_FILE;

            free(extension);
        }
    }
    return export_format;
}

field_file_format_type
field_config_get_export_format(const field_config_type *field_config) {
    return field_config->export_format;
}

const char *
field_config_get_output_file_name(const field_config_type *field_config) {
    return field_config->output_field_name;
}

/**
   Will return the name of the init_transform function, or NULL if no
   init_transform function has been registered.
*/
const char *
field_config_get_init_transform_name(const field_config_type *field_config) {
    return field_config->init_transform_name;
}

const char *
field_config_get_input_transform_name(const field_config_type *field_config) {
    return field_config->input_transform_name;
}

const char *
field_config_get_output_transform_name(const field_config_type *field_config) {
    return field_config->output_transform_name;
}

const char *field_config_get_grid_name(const field_config_type *config) {
    return config->grid;
}

/*
  The return value from this function is hardly usable.
*/
field_config_type *field_config_alloc_empty(const char *ecl_kw_name,
                                            const char *path_to_grid,
                                            bool keep_inactive_cells) {
    field_config_type *config =
        (field_config_type *)util_malloc(sizeof *config);

    config->keep_inactive_cells = keep_inactive_cells;
    config->ecl_kw_name = util_alloc_string_copy(ecl_kw_name);
    config->output_field_name = NULL;
    config->grid = util_alloc_string_copy(path_to_grid);
    config->type = UNKNOWN_FIELD_TYPE;

    config->output_transform = NULL;
    config->input_transform = NULL;
    config->init_transform = NULL;
    config->output_transform_name = NULL;
    config->input_transform_name = NULL;
    config->init_transform_name = NULL;

    config->truncation = TRUNCATE_NONE;
    config->trans_table = field_trans_table_alloc();

    return config;
}

static void field_config_set_init_transform(field_config_type *config,
                                            const char *__init_transform_name) {
    const char *init_transform_name = NULL;
    if (field_trans_table_has_key(config->trans_table, __init_transform_name))
        init_transform_name = __init_transform_name;
    else if (__init_transform_name != NULL) {
        fprintf(stderr,
                "Sorry: the field transformation function:%s is not recognized "
                "\n\n",
                __init_transform_name);
        field_trans_table_fprintf(config->trans_table, stderr);
        util_exit("Exiting ... \n");
    }

    config->init_transform_name = util_realloc_string_copy(
        config->init_transform_name, init_transform_name);
    if (init_transform_name != NULL)
        config->init_transform =
            field_trans_table_lookup(config->trans_table, init_transform_name);
    else
        config->init_transform = NULL;
}

static void
field_config_set_output_transform(field_config_type *config,
                                  const char *__output_transform_name) {
    const char *output_transform_name = NULL;
    if (field_trans_table_has_key(config->trans_table, __output_transform_name))
        output_transform_name = __output_transform_name;
    else if (__output_transform_name) {
        fprintf(stderr,
                "Sorry: the field transformation function:%s is not recognized "
                "\n\n",
                __output_transform_name);
        field_trans_table_fprintf(config->trans_table, stderr);
        util_exit("Exiting ... \n");
    }

    config->output_transform_name = util_realloc_string_copy(
        config->output_transform_name, output_transform_name);
    if (output_transform_name != NULL)
        config->output_transform = field_trans_table_lookup(
            config->trans_table, output_transform_name);
    else
        config->output_transform = NULL;
}

static void
field_config_set_input_transform(field_config_type *config,
                                 const char *__input_transform_name) {
    const char *input_transform_name = NULL;
    if (field_trans_table_has_key(config->trans_table, __input_transform_name))
        input_transform_name = __input_transform_name;
    else if (__input_transform_name != NULL) {
        fprintf(stderr,
                "Sorry: the field transformation function:%s is not recognized "
                "\n\n",
                __input_transform_name);
        field_trans_table_fprintf(config->trans_table, stderr);
        util_exit("Exiting ... \n");
    }

    config->input_transform_name = util_realloc_string_copy(
        config->input_transform_name, input_transform_name);
    if (input_transform_name != NULL)
        config->input_transform =
            field_trans_table_lookup(config->trans_table, input_transform_name);
    else
        config->input_transform = NULL;
}

void field_config_update_field(
    field_config_type *config, int truncation, double min_value,
    double max_value,
    field_file_format_type
        export_format, /* This can be guessed with the field_config_default_export_format( ecl_file ) function. */
    const char *init_transform, const char *input_transform,
    const char *output_transform, const char *output_field_name) {

    field_config_set_truncation(config, truncation, min_value, max_value);
    config->export_format = export_format;

    config->type = ECLIPSE_PARAMETER;
    field_config_set_input_transform(config, input_transform);
    field_config_set_init_transform(config, init_transform);
    field_config_set_output_transform(config, output_transform);
    config->output_field_name = util_alloc_string_copy(output_field_name);
}

/**
   Requirements:

   ECLIPSE_PARAMETER: export_format != UNDEFINED_FORMAT

   ECLIPSE_RESTART  : Validation can be finalized at the enkf_config_node level.

   GENERAL          : export_format != UNDEFINED_FORMAT
*/
bool field_config_is_valid(const field_config_type *field_config) {
    bool valid = true;

    switch (field_config->type) {
    case ECLIPSE_PARAMETER:
        if (field_config->export_format == UNDEFINED_FORMAT)
            valid = false;
        break;
    case ECLIPSE_RESTART:
        break;
    case GENERAL:
        if (field_config->export_format == UNDEFINED_FORMAT)
            valid = false;
        break;
    default:
        util_abort("%s: Internal inconsistency in field config \n", __func__);
    }
    return valid;
}

field_type_enum field_config_get_type(const field_config_type *config) {
    return config->type;
}

void field_config_set_truncation(field_config_type *config, int truncation,
                                 double min_value, double max_value) {
    config->truncation = truncation;
    config->min_value = min_value;
    config->max_value = max_value;
}

int field_config_get_truncation_mode(const field_config_type *config) {
    return config->truncation;
}

double field_config_get_truncation_min(const field_config_type *config) {
    return config->min_value;
}

double field_config_get_truncation_max(const field_config_type *config) {
    return config->max_value;
}

void field_config_free(field_config_type *config) {
    free(config->ecl_kw_name);
    free(config->input_transform_name);
    free(config->output_transform_name);
    free(config->init_transform_name);
    free(config->grid);
    field_trans_table_free(config->trans_table);
    free(config);
}

int field_config_get_nx(const field_config_type *config) { return config->nx; }

int field_config_get_ny(const field_config_type *config) { return config->ny; }

int field_config_get_nz(const field_config_type *config) { return config->nz; }


const char *field_config_get_key(const field_config_type *field_config) {
    return field_config->ecl_kw_name;
}

bool field_config_keep_inactive_cells(const field_config_type *config) {
    return config->keep_inactive_cells;
}

field_func_type *
field_config_get_output_transform(const field_config_type *config) {
    return config->output_transform;
}

field_func_type *
field_config_get_init_transform(const field_config_type *config) {
    return config->init_transform;
}

field_func_type *
field_config_get_input_transform(const field_config_type *config) {
    return config->input_transform;
}


void field_config_set_dims(field_config_type *config, int x, int y, int z) {
    config->nx = x;
    config->ny = y;
    config->nz = z;
}

CONFIG_GET_ECL_KW_NAME(field);
GET_DATA_SIZE(field)
VOID_GET_DATA_SIZE(field)
VOID_FREE(field_config)
