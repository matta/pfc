/* acconfig.h -- autoheader configuration file
 */

#ifndef CONFIG_H
#define CONFIG_H

@TOP@

/* acconfig.h

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


/* Define if your machine can copy aligned words much faster than bytes.  */
#undef LZO_ALIGNED_OK_4

/* Define if your memcmp is broken.  */
#undef NO_MEMCMP

/* Define to the name of the distribution.  */
#undef PACKAGE

/* Define to `long' if <stddef.h> doesn't define.  */
#undef ptrdiff_t

/* The number of bytes in a ptrdiff_t.  */
#undef SIZEOF_PTRDIFF_T

/* The number of bytes in a size_t.  */
#undef SIZEOF_SIZE_T

/* Define to the version of the distribution.  */
#undef VERSION


/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */



@BOTTOM@

/* $BOTTOM$ */

#if (SIZEOF_PTRDIFF_T <= 0)
#  undef /**/ SIZEOF_PTRDIFF_T
#endif

#if (SIZEOF_SIZE_T <= 0)
#  undef /**/ SIZEOF_SIZE_T
#endif

#endif /* already included */






