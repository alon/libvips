#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include <glib.h>

#include <vips/debug.h>
#include <vips/vips.h>

enum {
    LABEL = 0,
    MULTILINE = 1,
};

gchar *reversed_list_join(GList *list, gchar seperator)
{
    GList *item;
    gchar **arr;
    gchar *ret;
    int i = 0;

    arr = g_new(gchar *, g_list_length(list));
    for (item = g_list_last(list); item; item = g_list_previous(item), ++i) {
        arr[i] = item->data;
    }
    ret = g_strjoinv(" ", arr);
    g_free(arr);
    return ret;
}

GHashTable *parse_label(FILE *f)
{
    GHashTable *d;
    int state;
    gchar line[1024];
    gchar *k;
    gchar *v;
    GList *v_list;
    gchar **equal_split;

    d = g_hash_table_new(g_str_hash, g_str_equal);
    state = LABEL;
    while (1) {
        fgets(line, sizeof(line), f);
        if (feof(f)) {
            // error out
            printf("error - reached end of file prematurely\n");
            return NULL;
        }
        if (g_str_has_prefix(line, "END")) {
            break;
        }
        if (state == LABEL) {
            if (g_strstr_len(line, -1, "=")) {
                equal_split = g_strsplit(line, "=", 2);
                k = g_strdup(equal_split[0]);
                v = g_strdup(equal_split[1]);
                k = g_strstrip(k);
                v = g_strstrip(v);
                g_strfreev(equal_split);
            } else {
                continue;
            }
            if (v[0] == '"' && v[strlen(v) - 1] != '"') {
                state = MULTILINE;
                v_list = g_list_alloc();
                g_list_append(v_list, v);
            } else {
                g_hash_table_replace(d, k, v);
            }
        } else {
            v_list = g_list_prepend(v_list, g_strdup(line));
            g_strchomp(line);
            if (line[strlen(line) - 1] == '"') {
                v = reversed_list_join(v_list, ' ');
                g_hash_table_replace(d, k, v);
                state = LABEL;
            }
        }
    }
    return d;
}

typedef struct {
    char *filename;
    FILE *f;
    VipsImage *image;
    GHashTable *label;
    int image_offset;
    int line_size;
} VipsPds;


#if 0
    /* write something to out */
    fseek(f, image_offset, SEEK_SET);
    line_size = LINE_SAMPLES * SAMPLE_BITS / 8;
    line = g_malloc(line_size);
    if (handle_line) {
        for (i = 0 ; i < LINES; ++i) {
            fread(line, line_size, 1, f);
            handle_line(line, line_size);
        }
    }
    g_free(line);
    fclose(f);
#endif


int vips_pds_get_header( VipsPds *pds , VipsImage *out )
{
    /*
    Parse a PDS3 image filename. All Lunar Reconnaissance Orbiter (LRO) Calibrated Data Record (CDR)
    are provided as PDS3 encoded images. This means they start with a header in ASCII in a key=value
    format up to a single line containing 'END\n', with filler '\n' until a size of a single record,
    followed by binary records as described by the header:

    File is divided into RECORDs, each a multiple of RECORD_BYTEs size:
        Label            - First record
        Image            - pointed to from Label via a "^Image" key
    Label is comprised of:
        key = value
        'END\n'

    From now on an all caps underscore connected word denotes the value of same name label key, i.e.
    RECORD_BYTES = 5064
    iff there is a line in label of the form:
        RECORD_BYTES = 5064
    Filler is comprised of:
        '\n' * (RECORD_BYTES - size(label))
    Image is comprised of:
        LINES times
         SAMPLE_BITS / 8 * LINE_SAMPLES

    Some consistency checks:
        FILE_RECORDS * RECORD_BYTES = file size
        LINES * SAMPLE_BITS / 8 * LINE_SAMPLES = file_size - RECORD_BYTES
        size(label) <= RECORD_BYTES
    */
    int RECORD_BYTES;
    int SAMPLE_BITS;
    int LINES;
    int LINE_SAMPLES;
    int FILE_RECORDS;
    int image_offset;
    struct stat stat_pds;
    int line_size;
    gchar *line;
    int file_size;
    GHashTableIter iter;
    gchar *key, *value;
    int width, height;

    RECORD_BYTES = g_ascii_strtoll(g_hash_table_lookup(pds->label, "RECORD_BYTES"),
                                   NULL, 10);
    pds->image_offset = (g_ascii_strtoll(g_hash_table_lookup(pds->label, "^IMAGE"),
                                    NULL, 10) - 1) * RECORD_BYTES;
    SAMPLE_BITS = g_ascii_strtoll(g_hash_table_lookup(pds->label, "SAMPLE_BITS"),
                                  NULL, 10);
    LINES = g_ascii_strtoll(g_hash_table_lookup(pds->label, "LINES"),
                            NULL, 10);
    LINE_SAMPLES = g_ascii_strtoll(g_hash_table_lookup(pds->label, "LINE_SAMPLES"),
                                   NULL, 10);
    FILE_RECORDS = g_ascii_strtoll(g_hash_table_lookup(pds->label, "FILE_RECORDS"),
                                   NULL, 10);
    if (stat(pds->filename, &stat_pds) != 0) {
        fprintf(stderr, "error: stat failed on %s\n", pds->filename);
        return( -1 );
    }
    pds->line_size = LINE_SAMPLES * SAMPLE_BITS / 8;
    file_size = stat_pds.st_size;
    assert(file_size == FILE_RECORDS * RECORD_BYTES);
    assert(file_size - pds->image_offset == LINES * SAMPLE_BITS / 8 * LINE_SAMPLES);
    assert(ftell(pds->f) <= pds->image_offset);
    width = LINE_SAMPLES;
    height = LINES;
    vips_image_init_fields( out,
            width, height, 1 /* bands */, VIPS_FORMAT_USHORT,
            VIPS_CODING_NONE, VIPS_INTERPRETATION_GREY16,
            1.0, 1.0);
    g_hash_table_iter_init(&iter, pds->label);
    while (g_hash_table_iter_next(&iter, (gpointer *)&key, (gpointer *)&value)) {
        VIPS_DEBUG_MSG( "iter %s = %s\n", key, value);
        vips_image_set_string( out, key, value );
    }
    return( 0 );
}

/* create a new vips reader */
const char *vips__pds_suffs[] = {".IMG", NULL};

/* Shut down. Can be called many times.
 */
static void
vips_pds_close( VipsPds *pds )
{
    VIPS_FREE( pds->filename );
    if ( pds->f ) {
        fclose( pds->f );
        pds->f = NULL;
    }
}

static void
vips_pds_close_cb( VipsImage *image, VipsPds *pds )
{
    vips_pds_close( pds );
}

int vips__pds_ispds( const char *filename )
{
    char buf[100];
    const char *field = "PDS_VERSION_ID";

    if (!im__get_bytes(filename, buf, strlen(field))) {
        return 0;
    }
    buf[strlen(field)] = '\0';
    if (g_strcmp0(buf, field)) {
        return 0;
    }
    return 1;
}

static VipsPds *
vips_pds_new_read( const char *filename, VipsImage *out)
{
    VipsPds *pds;

    if( !(pds = VIPS_NEW( out, VipsPds )) ) {
        return( NULL );
    }

    pds->filename = vips_strdup( NULL, filename );
    pds->f = fopen(filename, "rb");
    g_signal_connect( out, "close", 
            G_CALLBACK( vips_pds_close_cb ), pds );

    pds->image = out;
    pds->label = parse_label(pds->f);
}

int
vips__pds_read_header( const char *filename, VipsImage *out )
{
    VipsPds *pds;

    VIPS_DEBUG_MSG( "pds2vips_header: reading \"%s\"\n", filename );

    if( !(pds = vips_pds_new_read( filename, out )) || 
        vips_pds_get_header( pds, out ) ) 
        return( -1 );

    return( 0 );
}

static int
pds2vips_generate( VipsRegion *out, 
    void *seq, void *a, void *b, gboolean *stop )
{
    VipsPds *pds = (VipsPds *) a;
    Rect *r = &out->valid;
    int read_length;
    int skip_length;
    VipsPel *q;
    int i;

    VIPS_DEBUG_MSG( "pds2vips_generate: "
        "generating left = %d, top = %d, width = %d, height = %d\n", 
        r->left, r->top, r->width, r->height );

    assert(VIPS_IMAGE_SIZEOF_PEL((out->im)) == 2);
    fseek(pds->f, pds->image_offset + r->left * 2 + r->top * pds->line_size, SEEK_SET);
    read_length = r->width * 2;
    skip_length = pds->line_size - read_length + r->width * 2;
    q = VIPS_REGION_ADDR( out, r->left, r->top );

    if (skip_length) {
        for (i = 0 ; i < r->height; ++i) {
            fread(q, read_length, 1, pds->f);
            fseek(pds->f, skip_length, SEEK_CUR);
        }
    } else {
        fread(q, read_length * r->height, 1, pds->f);
    }

    return( 0 );
pds_read_subset_error:
    vips_error( "pds", "pds_read_subset" );
    return( -1 );
}


static int
pds2vips( const char *filename, VipsImage *out )
{
    VipsPds *pds;

    if( !(pds = vips_pds_new_read( filename, out )) )
        return( -1 );
    if( vips_pds_get_header( pds, out ) ||
        vips_image_generate( out, 
            NULL, pds2vips_generate, NULL, pds, NULL ) ) {
        vips_pds_close( pds );
        return( -1 );
    }

    /* Don't vips_pds_close(), we need it to stick around for the
     * generate.
     */

    return( 0 );
}

int
vips__pds_read( const char *filename, VipsImage *out )
{
    VipsImage *t;
    int n_bands;

    VIPS_DEBUG_MSG( "pds2vips: reading \"%s\"\n", filename );

    /* pds is naturally a band-separated format. For single-band images
     * we can just read out. For many bands we read each band out
     * separately then join them.
     */

    t = vips_image_new();
    if( vips__pds_read_header( filename, t ) ) {
        g_object_unref( t );
        return( -1 );
    }
    g_object_unref( t );

    return pds2vips( filename, out );
}
