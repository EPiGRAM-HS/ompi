#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2009 High Performance Computing Center Stuttgart,
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2013      Los Alamos National Security, LLC. All rights
#                         reserved.
# Copyright (c) 2017      Research Organization for Information Science
#                         and Technology (RIST). All rights reserved.
# Copyright (c) 2017      IBM Corporation.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
	coll_libpnbc_osc.h \
	coll_libpnbc_osc_component.c \
	pnbc_osc.c \
	pnbc_osc_internal.h \
	pnbc_osc_iallgather.c \
	pnbc_osc_iallgatherv.c \
	pnbc_osc_iallreduce.c \
	pnbc_osc_ialltoall.c \
	pnbc_osc_ialltoallv.c \
	pnbc_osc_ialltoallw.c \
	pnbc_osc_ibarrier.c \
	pnbc_osc_ibcast.c \
	pnbc_osc_iexscan.c \
	pnbc_osc_igather.c \
	pnbc_osc_igatherv.c \
	pnbc_osc_ineighbor_allgather.c \
	pnbc_osc_ineighbor_allgatherv.c \
	pnbc_osc_ineighbor_alltoall.c \
	pnbc_osc_ineighbor_alltoallv.c \
	pnbc_osc_ineighbor_alltoallw.c \
	pnbc_osc_ireduce.c \
	pnbc_osc_ireduce_scatter.c \
	pnbc_osc_ireduce_scatter_block.c \
	pnbc_osc_iscan.c \
	pnbc_osc_iscatter.c \
	pnbc_osc_iscatterv.c \
	pnbc_osc_neighbor_helpers.c \
	coll_libpnbc_osc_win.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_ompi_coll_libpnbc_osc_DSO
component_noinst =
component_install = mca_coll_libpnbc_osc.la
else
component_noinst = libmca_coll_libpnbc_osc.la
component_install =
endif

mcacomponentdir = $(ompilibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_coll_libpnbc_osc_la_SOURCES = $(sources)
mca_coll_libpnbc_osc_la_LDFLAGS = -module -avoid-version
mca_coll_libpnbc_osc_la_LIBADD = $(top_builddir)/ompi/lib@OMPI_LIBMPI_NAME@.la

noinst_LTLIBRARIES = $(component_noinst)
libmca_coll_libpnbc_osc_la_SOURCES =$(sources)
libmca_coll_libpnbc_osc_la_LDFLAGS = -module -avoid-version
