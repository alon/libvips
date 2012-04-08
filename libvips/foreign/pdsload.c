/* load PDS from a file
 *
 * 31/3/12
 * 	- from fitsload.c
 */

/*

    This file is part of VIPS.
    
    VIPS is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

/*

    These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define DEBUG
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vips/vips.h>
#include <vips/buf.h>
#include <vips/internal.h>
#include "pds.h"

typedef struct _VipsForeignLoadPds {
	VipsForeignLoad parent_object;

	/* Filename for load.
	 */
	char *filename; 

} VipsForeignLoadPds;

typedef VipsForeignLoadClass VipsForeignLoadPdsClass;

G_DEFINE_TYPE( VipsForeignLoadPds, vips_foreign_load_pds, 
	VIPS_TYPE_FOREIGN_LOAD );

static int
vips_foreign_load_pds_header( VipsForeignLoad *load )
{
	VipsForeignLoadPds *pds = (VipsForeignLoadPds *) load;

	if( vips__pds_read_header( pds->filename, load->out ) ) 
		return( -1 );

	return( 0 );
}

static int
vips_foreign_load_pds_load( VipsForeignLoad *load )
{
	VipsForeignLoadPds *pds = (VipsForeignLoadPds *) load;

	if( vips__pds_read( pds->filename, load->real ) ) {
		return( -1 );
        }

	return( 0 );
}

static void
vips_foreign_load_pds_class_init( VipsForeignLoadPdsClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *object_class = (VipsObjectClass *) class;
	VipsForeignClass *foreign_class = (VipsForeignClass *) class;
	VipsForeignLoadClass *load_class = (VipsForeignLoadClass *) class;

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	object_class->nickname = "pdsload";
	object_class->description = _( "load a PDS image" );

	foreign_class->suffs = vips__pds_suffs;

	load_class->is_a = vips__pds_ispds;
	load_class->header = vips_foreign_load_pds_header;
	load_class->load = vips_foreign_load_pds_load;

	VIPS_ARG_STRING( class, "filename", 1, 
		_( "Filename" ),
		_( "Filename to load from" ),
		VIPS_ARGUMENT_REQUIRED_INPUT, 
		G_STRUCT_OFFSET( VipsForeignLoadPds, filename ),
		NULL );
}

static void
vips_foreign_load_pds_init( VipsForeignLoadPds *pds )
{
}
