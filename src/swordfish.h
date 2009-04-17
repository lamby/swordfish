/*
 *  Swordfish database
 *  Copyrigh (C) 2009 UUMC, Ltd. <chris@playfire.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

struct stats {
	uint64_t total_cmds;
	time_t started;
};

struct config {
	const char* host;
	const char* database;
	short port;
	const char* pidfile;
};

#define REPLY_OK(req, buf) send_reply(req, buf, HTTP_OK, "OK")
#define REPLY_BADREQUEST(req, buf) send_reply(req, buf, HTTP_BADREQUEST, "Bad Request")
#define REPLY_NOTFOUND(req, buf) send_reply(req, buf, HTTP_NOTFOUND, "Not Found")
#define REPLY_BADMETHOD(req, buf) send_reply(req, buf, 405, "Method Not Allowed")
#define REPLY_INTERR(req, buf) send_reply(req, buf, 500, "Internal Server Error")

#define SWORDFISH_KEY_CMP tccmplexical

enum {
	RESULT_COUNT,
	RESULT_ALL,
	RESULT_KEYS,
	RESULT_VALUES,
};

const char *resource_lookup_table[] = {
	"trees",
	"intersection",
	"difference",
	"count",
	"item",
	"map",
	"stats",
	"sync",
	"keys",
	"values",
	"delete"
};

enum {
	RESOURCE_TREES,
	RESOURCE_INTERSECTION,
	RESOURCE_DIFFERENCE,
	RESOURCE_COUNT,
	RESOURCE_ITEM,
	RESOURCE_MAP,
	RESOURCE_STATS,
	RESOURCE_SYNC,
	RESOURCE_KEYS,
	RESOURCE_VALUES,
	RESOURCE_DELETE,

	RESOURCE_NONE,
	RESOURCE_UNKNOWN,
	RESOURCE_ALL
};
