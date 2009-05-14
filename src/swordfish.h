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

#ifdef __GNUC__
#define CHECK_FMT(a,b) __attribute__((format(printf, a, b)))
#else
#define CHECK_FMT(a,b)
#endif

void _swordfish_log(const char *level, const char *format, ...) CHECK_FMT(2, 3);

#define swordfish_info(args...) _swordfish_log("info", ## args);
#define swordfish_warn(args...) _swordfish_log("warn", ## args);
#define swordfish_fatal(args...) _swordfish_log("fatal", ## args);

#ifdef DEBUG
#define swordfish_debug(args...) _swordfish_log("debug", ## args);
#else
#define swordfish_debug(args...) do {;} while (0)
#endif

struct stats {
	uint64_t total_cmds;
	time_t started;
};

struct config {
	const char* host;
	const char* datadir;
	short port;
	const char* pidfile;
	const char* logfile;
};

#define REPLY_OK(req, buf) send_reply(req, buf, HTTP_OK, "OK")
#define REPLY_BADREQUEST(req, buf) send_reply(req, buf, HTTP_BADREQUEST, "Bad Request")
#define REPLY_NOTFOUND(req, buf, message) send_reply(req, buf, HTTP_NOTFOUND, message)
#define REPLY_BADMETHOD(req, buf) send_reply(req, buf, 405, "Method Not Allowed")
#define REPLY_INTERR(req, buf) send_reply(req, buf, 500, "Internal Server Error")

#define SWORDFISH_KEY_CMP tccmplexical
#define SWORDFISH_CONTENT_TYPE "text/plain"

#define TYPE_TREE 'T'
#define TYPE_COUNT 'C'

#define MAX_COUNTER_SIZE 30

enum {
	RESULT_COUNT,
	RESULT_ALL,
	RESULT_KEYS,
	RESULT_VALUES,
};

const char *resource_lookup_table[] = {
	"databases",
	"trees",
	"counters",
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
	RESOURCE_DATABASE,
	RESOURCE_TREES,
	RESOURCE_COUNTERS,
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
