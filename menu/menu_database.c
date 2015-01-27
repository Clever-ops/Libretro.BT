/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "menu.h"
#include "menu_database.h"
#include "menu_list.h"
#include "menu_entries.h"
#include <string.h>

#ifdef HAVE_LIBRETRODB
static int menu_database_open_cursor(libretrodb_t *db,
   libretrodb_cursor_t *cur, const char *query)
{
   const char *error     = NULL;
   libretrodb_query_t *q = NULL;

   if (query) 
      q = (libretrodb_query_t*)libretrodb_query_compile(db, query,
      strlen(query), &error);
    
   if (error)
      return -1;
   if ((libretrodb_cursor_open(db, cur, q)) != 0)
      return -1;

   return 0;
}

#endif

int menu_database_populate_query(file_list_t *list, const char *path,
    const char *query)
{
#ifdef HAVE_LIBRETRODB
   libretrodb_t db;
   libretrodb_cursor_t cur;
    
   if ((libretrodb_open(path, &db)) != 0)
      return -1;
   if ((menu_database_open_cursor(&db, &cur, query) != 0))
      return -1;
   if ((menu_entries_push_query(&db, &cur, list)) != 0)
      return -1;
    
   libretrodb_cursor_close(&cur);
   libretrodb_close(&db);
#endif

   return 0;
}

int menu_database_get_cursor(const char *path,
    const char *query, libretrodb_cursor_t *cur)
{
#ifdef HAVE_LIBRETRODB
   libretrodb_t db;
   if ((libretrodb_open(path, &db)) != 0)
      return -1;
   if ((menu_database_open_cursor(&db, cur, query) != 0))
      return -1;

   libretrodb_close(&db);

   return 0;
#else
   return -1;
#endif
}
