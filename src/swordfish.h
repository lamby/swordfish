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
	VALUES_ALL,
	VALUES_KEYS,
	VALUES_VALUES,
};

const char *lookup_table[] = {
	"trees",
	"intersection",
	"count",
	"item",
	"map",
	"stats",
	"sync",
	"keys",
	"values"
};

enum {
	RESOURCE_TREES,
	RESOURCE_INTERSECTION,
	RESOURCE_COUNT,
	RESOURCE_ITEM,
	RESOURCE_MAP,
	RESOURCE_STATS,
	RESOURCE_SYNC,
	RESOURCE_KEYS,
	RESOURCE_VALUES,

	RESOURCE_NONE,
	RESOURCE_UNKNOWN,
	RESOURCE_ALL
};
