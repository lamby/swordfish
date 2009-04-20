/*
 *  Swordfish database
 *  Copyright (C) 2009 UUMC, Ltd. <chris@playfire.com>
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

#include <config.h>

#include <sys/queue.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#include <event.h>
#include <evhttp.h>

#include <tchdb.h>
#include <tcutil.h>

#include "swordfish.h"

struct stats stats;
struct config config;

extern char *optarg;

TCHDB *db = NULL;
char *db_name = NULL;

void
send_reply(struct evhttp_request *request, struct evbuffer *databuf, int errorcode, const char *reason)
{

	evhttp_add_header(request->output_headers,
		"Content-Type", "text/plain"); // "application/json");
	evhttp_add_header(request->output_headers,
		"Server", PACKAGE_NAME "/" PACKAGE_VERSION);
	evhttp_send_reply(request, errorcode, reason, databuf);

#ifdef DEBUG
	const char *method;

	switch (request->type) {
	case EVHTTP_REQ_GET:
		method = "GET";
		break;
	case EVHTTP_REQ_POST:
		method = "POST";
		break;
	case EVHTTP_REQ_HEAD:
		method = "HEAD";
		break;
	}
	
	fprintf(stderr, "[%s] %d %s %s\n",
		request->remote_host, errorcode, method, request->uri);
#endif
}

void
append_json_value(struct evbuffer *databuf, const char *value)
{
	int pos = 0;
	unsigned char c;

	evbuffer_add_printf(databuf, "\"");

	do {
		c = value[pos];

		switch (c) {
		case '\0':
			break;
		case '\b':
			evbuffer_add_printf(databuf, "\\b");
			break;
		case '\n':
			evbuffer_add_printf(databuf, "\\n");
			break;
		case '\r':
			evbuffer_add_printf(databuf, "\\r");
			break;
		case '\t':
			evbuffer_add_printf(databuf, "\\t");
			break;
		case '"':
			evbuffer_add_printf(databuf, "\\\"");
			break;
		case '\\':
			evbuffer_add_printf(databuf, "\\\\");
			break;
		case '/': evbuffer_add_printf(databuf, "\\/");
			break;
		default:
			if (c < ' ') {
				evbuffer_add_printf(databuf, "\\u00%c%c",
					"0123456789abcdef"[c >> 4],
					"0123456789abcdef"[c & 0xf]
				);
			} else {
				evbuffer_add_printf(databuf, "%c", c);
			}
		}

		++pos;
	} while (c);

	evbuffer_add_printf(databuf, "\"");
}

void
handler_sync(struct evhttp_request *request)
{
	struct evbuffer *databuf = evbuffer_new();

	switch (request->type) {
	case EVHTTP_REQ_POST:
		tchdbsync(db);
		evbuffer_add_printf(databuf, "true\n");
		REPLY_OK(request, databuf);
		break;
	default:
		evbuffer_add_printf(databuf,
			"{\"err\": \"/sync should be POST\"}\n");
		REPLY_BADMETHOD(request, databuf);
	}

	evbuffer_free(databuf);
}

void
handler_stats(struct evhttp_request *request)
{
	struct evbuffer *databuf = evbuffer_new();

	evbuffer_add_printf(databuf, "{");
	evbuffer_add_printf(databuf,   "\"started\": %lu", stats.started);
	evbuffer_add_printf(databuf, ", \"total_cmds\": %lu", stats.total_cmds);
	evbuffer_add_printf(databuf, ", \"version\": \"%s\"", PACKAGE_VERSION);
	evbuffer_add_printf(databuf, ", \"tokyocabinet_version\": \"%s\"", tcversion);
	evbuffer_add_printf(databuf, "}\n");

	REPLY_OK(request, databuf);
	evbuffer_free(databuf);
}

void
handler_stats_database(struct evhttp_request *request)
{
	struct stat file_status;
	struct evbuffer *databuf = evbuffer_new();

	char *db_realpath = tcrealpath(tchdbpath(db));
	if (stat(db_realpath, &file_status)) {
		perror("stat");
		goto fail;
	}

	evbuffer_add_printf(databuf, "{");
	evbuffer_add_printf(databuf, "  \"database\": \"%s\"", db_realpath);
	evbuffer_add_printf(databuf, ", \"num_items\": %zu", tchdbrnum(db));
	evbuffer_add_printf(databuf, ", \"database_bytes\": %jd", (intmax_t)file_status.st_size);
	evbuffer_add_printf(databuf, "}\n");

	REPLY_OK(request, databuf);

	goto end;
fail:	
	REPLY_INTERR(request, databuf);
end:
	free(db_realpath);
	evbuffer_free(databuf);
}

void
handler_tree_intersection(struct evhttp_request *request, const char *left_key, const char *right_key, int result, int skip, int limit)
{
	int size;
	int cmp_val;
	int result_count = 0;

	char* value = NULL;
	const char* left_val = NULL;
	const char* right_val = NULL;

	TCTREE *left = NULL;
	TCTREE *right = NULL;

	struct evbuffer *databuf = evbuffer_new();

	if (result == RESULT_COUNT) {
		evbuffer_add_printf(databuf, "{\"count\": ");
	} else {
		evbuffer_add_printf(databuf, "{\"items\": [");
	}

	value = tchdbget(db, left_key, strlen(left_key), &size);
	if (!value) {
		goto end;
	}
	left = tctreeload(value, size, SWORDFISH_KEY_CMP, NULL);
	free(value);

	value = tchdbget(db, right_key, strlen(right_key), &size);
	if (!value) {
		goto end;
	}
	right = tctreeload(value, size, SWORDFISH_KEY_CMP, NULL);
	free(value);

	tctreeiterinit(left);
	tctreeiterinit(right);

	left_val = tctreeiternext2(left);
	right_val = tctreeiternext2(right);

	if (limit == 0) {
		limit = -1;
	}

	while (left_val && right_val)
	{
		if (result_count == limit)
			break;

		cmp_val = SWORDFISH_KEY_CMP(left_val, strlen(left_val),
			right_val, strlen(right_val), NULL);

		switch ((cmp_val > 0) - (cmp_val < 0))
		{
		case 0:
			/* left == right; Element intersects */
			if (skip == 0) {
				switch (result) {
				case RESULT_KEYS:
					if (result_count)
						evbuffer_add_printf(databuf, ",");
					append_json_value(databuf, left_val);
					break;
				case RESULT_VALUES:
					if (result_count)
						evbuffer_add_printf(databuf, ",");
					append_json_value(databuf, tctreeget2(left, left_val));
					break;
				case RESULT_ALL:
					evbuffer_add_printf(databuf,
						(result_count == 0) ? "[" : ",[");
					append_json_value(databuf, left_val);
					evbuffer_add_printf(databuf, ",");
					append_json_value(databuf, tctreeget2(left, left_val));
					evbuffer_add_printf(databuf, "]");
				}

				++result_count;
			} else {
				/* Skip this element */
				skip--;
			}

			left_val = tctreeiternext2(left);
			right_val = tctreeiternext2(right);
			break;

		case -1:
			/* left < right */
			left_val = tctreeiternext2(left);
			break;

		case 1:
			/* left > right */
			right_val = tctreeiternext2(right);
			break;
		}

	}

end:
	if (result == RESULT_COUNT) {
		evbuffer_add_printf(databuf, "%d}", result_count);
	} else {
		evbuffer_add_printf(databuf, "]}");
	}

	REPLY_OK(request, databuf);

	if (left) tctreedel(left);
	if (right) tctreedel(right);

	evbuffer_free(databuf);
	++stats.total_cmds;
}

void
handler_tree_difference(struct evhttp_request *request, const char *left_key, const char *right_key, int result, int skip, int limit)
{
	int size;
	int cmp_val;
	int result_count = 0;

	char* value = NULL;
	const char* left_val = NULL;
	const char* right_val = NULL;

	TCTREE *left = NULL;
	TCTREE *right = NULL;

	struct evbuffer *databuf = evbuffer_new();

	if (limit == 0) {
		limit = -1;
	}

	if (result == RESULT_COUNT) {
		evbuffer_add_printf(databuf, "{\"count\": ");
	} else {
		evbuffer_add_printf(databuf, "{\"items\": [");
	}

	value = tchdbget(db, left_key, strlen(left_key), &size);
	if (value) {
		left = tctreeload(value, size, SWORDFISH_KEY_CMP, NULL);
		free(value);
	}

	value = tchdbget(db, right_key, strlen(right_key), &size);
	if (value) {
		right = tctreeload(value, size, SWORDFISH_KEY_CMP, NULL);
		free(value);
	}

	if (left == NULL) {
		/* No left tree; don't emit anything */
		goto end;
	}

	if (right == NULL) {
		/* No right tree; emit everything on left tree */

		if (result == RESULT_COUNT) {
			result_count = tctreernum(left);
			goto end;
		}
		
		tctreeiterinit(left);
		left_val = tctreeiternext2(left);

		while (left_val)
		{
			if (result_count == limit)
				break;

			if (skip == 0) {
				switch (result) {
				case RESULT_KEYS:
					if (result_count)
						evbuffer_add_printf(databuf, ",");
					append_json_value(databuf, left_val);
					break;
				case RESULT_VALUES:
					if (result_count)
						evbuffer_add_printf(databuf, ",");
					append_json_value(databuf, tctreeget2(left, left_val));
					break;
				case RESULT_ALL:
					evbuffer_add_printf(databuf,
						(result_count == 0) ? "[" : ",[");
					append_json_value(databuf, left_val);
					evbuffer_add_printf(databuf, ",");
					append_json_value(databuf, tctreeget2(left, left_val));
					evbuffer_add_printf(databuf, "]");
				}

				++result_count;
			} else {
				/* Skip this element */
				skip--;
			}

			left_val = tctreeiternext2(left);
		}

		goto end;
	}

	tctreeiterinit(left);
	tctreeiterinit(right);

	left_val = tctreeiternext2(left);
	right_val = tctreeiternext2(right);

	while (left_val && right_val)
	{
		if (result_count == limit)
			break;

		cmp_val = SWORDFISH_KEY_CMP(left_val, strlen(left_val),
			right_val, strlen(right_val), NULL);

		switch ((cmp_val > 0) - (cmp_val < 0))
		{
		case 0:
			/* left == right; Element intersects */
			left_val = tctreeiternext2(left);
			right_val = tctreeiternext2(right);
			break;

		case -1:
			/* left < right */
			if (skip == 0) {
				switch (result) {
				case RESULT_KEYS:
					if (result_count)
						evbuffer_add_printf(databuf, ",");
					append_json_value(databuf, left_val);
					break;
				case RESULT_VALUES:
					if (result_count)
						evbuffer_add_printf(databuf, ",");
					append_json_value(databuf, tctreeget2(left, left_val));
					break;
				case RESULT_ALL:
					evbuffer_add_printf(databuf,
						(result_count == 0) ? "[" : ",[");
					append_json_value(databuf, left_val);
					evbuffer_add_printf(databuf, ",");
					append_json_value(databuf, tctreeget2(left, left_val));
					evbuffer_add_printf(databuf, "]");
				}

				++result_count;
			} else {
				/* Skip this element */
				skip--;
			}
			
			left_val = tctreeiternext2(left);
			break;

		case 1:
			/* left > right */
			right_val = tctreeiternext2(right);
			break;
		}

	}

	while (left_val && (result_count != limit))
	{
		if (skip == 0) {
			switch (result) {
			case RESULT_KEYS:
				if (result_count)
					evbuffer_add_printf(databuf, ",");
				append_json_value(databuf, left_val);
				break;
			case RESULT_VALUES:
				if (result_count)
					evbuffer_add_printf(databuf, ",");
				append_json_value(databuf, tctreeget2(left, left_val));
				break;
			case RESULT_ALL:
				evbuffer_add_printf(databuf,
					(result_count == 0) ? "[" : ",[");
				append_json_value(databuf, left_val);
				evbuffer_add_printf(databuf, ",");
				append_json_value(databuf, tctreeget2(left, left_val));
				evbuffer_add_printf(databuf, "]");
			}

			++result_count;
		} else {
			/* Skip this element */
			skip--;
		}

		left_val = tctreeiternext2(left);
	}

end:
	if (result == RESULT_COUNT) {
		evbuffer_add_printf(databuf, "%d}", result_count);
	} else {
		evbuffer_add_printf(databuf, "]}");
	}

	REPLY_OK(request, databuf);

	if (left) tctreedel(left);
	if (right) tctreedel(right);

	evbuffer_free(databuf);
	++stats.total_cmds;
}

void
handler_tree_set_item(struct evhttp_request *request, const char *tree_key, const char *value_key)
{
	int size;
	int ecode;

	int rawtree_size;
	void *rawtree;

	struct evbuffer *databuf = evbuffer_new();

	TCTREE *tree = NULL;
	rawtree = tchdbget(db, tree_key, strlen(tree_key), &rawtree_size);

	if (rawtree) {
		tree = tctreeload(rawtree, rawtree_size, SWORDFISH_KEY_CMP, NULL);
		free(rawtree);
	} else {
		ecode = tchdbecode(db);

		if (ecode != TCENOREC) {
			evbuffer_add_printf(databuf,
				"{\"msg\": \"%s\"}", tchdberrmsg(ecode));
			REPLY_INTERR(request, databuf);
			goto end;
		}

		tree = tctreenew2(SWORDFISH_KEY_CMP, NULL);
	}

	if (request->ntoread) {
		evbuffer_add_printf(databuf,
			"{\"err\": \"Not enough POST data\"}");
		REPLY_BADMETHOD(request, databuf);
		goto end;
	}

	size = EVBUFFER_LENGTH(request->input_buffer);

	if (size) {
		tctreeput(tree, value_key, strlen(value_key),
			EVBUFFER_DATA(request->input_buffer), size);
	} else {
		/* If current size of tree is 1, delete entire tree instead of
		 * leaving tree with size 0. */
		if (tctreernum(tree) == 1) {
			tchdbout2(db, tree_key);

			evbuffer_add_printf(databuf, "true");
			REPLY_OK(request, databuf);
			goto end;
		}

		tctreeout2(tree, value_key);
	}

	rawtree = tctreedump(tree, &size);
	if (!tchdbput(db, tree_key, strlen(tree_key), rawtree, size)) {
		ecode = tchdbecode(db);
		free(rawtree);

		evbuffer_add_printf(databuf,
			"{\"msg\": \"%s\"}", tchdberrmsg(ecode));
		REPLY_INTERR(request, databuf);
		goto end;
	}

	free(rawtree);

	evbuffer_add_printf(databuf, "true");
	REPLY_OK(request, databuf);

end:
	if (tree)
		tctreedel(tree);

	evbuffer_free(databuf);
	++stats.total_cmds;
}

void
handler_counter_get(struct evhttp_request *request, const char *counter_key)
{
	int ecode;

	char *rawcount;
	int rawcount_size;

	struct evbuffer *databuf = evbuffer_new();

	rawcount = tchdbget(db, counter_key, strlen(counter_key), &rawcount_size);

	if (rawcount) {
		evbuffer_add_printf(databuf, "%s", rawcount);
	} else {
		ecode = tchdbecode(db);

		if (ecode != TCENOREC) {
			evbuffer_add_printf(databuf,
				"{\"msg\": \"%s\"}", tchdberrmsg(ecode));
			REPLY_INTERR(request, databuf);
			goto end;
		}

		evbuffer_add_printf(databuf, "0");
	}

	REPLY_OK(request, databuf);

end:
	if (rawcount)
		free(rawcount);

	evbuffer_free(databuf);
	++stats.total_cmds;
}

void
handler_counter_set(struct evhttp_request *request, const char *counter_key)
{
	int size;
	long unsigned int val;

	char rawcount[MAX_COUNTER_SIZE];

	char *str;
	char *endptr;

	struct evbuffer *databuf = evbuffer_new();

	if (request->ntoread) {
		evbuffer_add_printf(databuf,
			"{\"err\": \"Not enough POST data\"}");
		REPLY_BADMETHOD(request, databuf);
		goto end;
	}

	size = EVBUFFER_LENGTH(request->input_buffer);
	str = (char *)malloc(size + 1);
	memcpy(str, EVBUFFER_DATA(request->input_buffer), size);
	str[size] = '\0';

	val = strtoll(str, &endptr, 10);
	free(str);

	if (val == LONG_MAX || endptr == str) {
		evbuffer_add_printf(databuf,
			"{\"err\": \"Invalid counter value\"}");
		REPLY_BADMETHOD(request, databuf);
		goto end;
	}

	snprintf(rawcount, MAX_COUNTER_SIZE, "%lu", val);

	if (!tchdbput(db, counter_key, strlen(counter_key), rawcount, strlen(rawcount))) {
		int ecode = tchdbecode(db);

		evbuffer_add_printf(databuf,
			"{\"msg\": \"%s\"}", tchdberrmsg(ecode));
		REPLY_INTERR(request, databuf);
		goto end;
	}

	evbuffer_add_printf(databuf, "true");
	REPLY_OK(request, databuf);

end:
	evbuffer_free(databuf);
	++stats.total_cmds;
}

void
handler_tree_get_item(struct evhttp_request *request, const char *tree_key, const char *value_key)
{
	int ecode;

	int rawtree_size;
	char *rawtree;

	const char *value;

	struct evbuffer *databuf = evbuffer_new();

	TCTREE *tree = NULL;
	rawtree = tchdbget(db, tree_key, strlen(tree_key), &rawtree_size);

	if (!rawtree) {
		ecode = tchdbecode(db);

		if (ecode != TCENOREC) {
			evbuffer_add_printf(databuf,
				"{\"msg\": \"%s\"}", tchdberrmsg(ecode));
			REPLY_INTERR(request, databuf);
			goto end;
		}

		evbuffer_add_printf(databuf, "false");
		REPLY_NOTFOUND(request, databuf);
		goto end;
	}

	tree = tctreeload(rawtree, rawtree_size, SWORDFISH_KEY_CMP, NULL);
	free(rawtree);

	value = tctreeget2(tree, value_key);

	if (!value) {
		evbuffer_add_printf(databuf, "false");
		REPLY_NOTFOUND(request, databuf);
		goto end;
	}

	evbuffer_add_printf(databuf, "{\"item\": ");
	append_json_value(databuf, value);
	evbuffer_add_printf(databuf, "}");
	REPLY_OK(request, databuf);

end:
	if (tree)
		tctreedel(tree);

	evbuffer_free(databuf);
	++stats.total_cmds;
}

void
handler_tree_get(struct evhttp_request *request, const char *key, int result, int skip, int limit)
{
	int ecode;
	int result_count = 0;

	int rawtree_size;
	char *rawtree;

	const char *keyval;

	struct evbuffer *databuf = evbuffer_new();

	TCTREE *tree = NULL;
	rawtree = tchdbget(db, key, strlen(key), &rawtree_size);

	if (!rawtree) {
		ecode = tchdbecode(db);

		if (ecode != TCENOREC) {
			evbuffer_add_printf(databuf,
				"{\"msg\": \"%s\"}", tchdberrmsg(ecode));
			REPLY_INTERR(request, databuf);
			goto end;
		}

		if (result == RESULT_COUNT) {
			evbuffer_add_printf(databuf, "{\"count\": 0}");
		} else {
			evbuffer_add_printf(databuf, "{\"items\": []}");
		}

		REPLY_OK(request, databuf);
		
		goto end;
	}

	tree = tctreeload(rawtree, rawtree_size, SWORDFISH_KEY_CMP, NULL);
	free(rawtree);

	if (result == RESULT_COUNT) {
		evbuffer_add_printf(databuf,
			"{\"count\": %llu}", (long long)tctreernum(tree));
		REPLY_OK(request, databuf);
		goto end;
	}

	evbuffer_add_printf(databuf, "{\"items\": [");

	tctreeiterinit(tree);
	while ((keyval = tctreeiternext2(tree)) != NULL) {
		if (skip-- > 0)
			continue;

		switch (result) {
		case RESULT_KEYS:
			if (result_count)
				evbuffer_add_printf(databuf, ",");
			append_json_value(databuf, keyval);
			break;
		case RESULT_VALUES:
			if (result_count)
				evbuffer_add_printf(databuf, ",");
			append_json_value(databuf, tctreeget2(tree, keyval));
			break;
		case RESULT_ALL:
			evbuffer_add_printf(databuf,
				(result_count == 0) ? "[" : ",[");
			append_json_value(databuf, keyval);
			evbuffer_add_printf(databuf, ",");
			append_json_value(databuf, tctreeget2(tree, keyval));
			evbuffer_add_printf(databuf, "]");
		}

		if (++result_count == limit)
			break;
	}

	evbuffer_add_printf(databuf, "]}");

	REPLY_OK(request, databuf);

end:
	if (tree)
		tctreedel(tree);

	evbuffer_free(databuf);
	++stats.total_cmds;
}

void
handler_tree_delete(struct evhttp_request *request, const char *key)
{
	struct evbuffer *databuf = evbuffer_new();

	switch (request->type) {
	case EVHTTP_REQ_POST:
		evbuffer_add_printf(databuf,
			(tchdbout2(db, key)) ? "true" : "false");
		REPLY_OK(request, databuf);
		break;

	default:
		evbuffer_add_printf(databuf,
			"{\"err\": \"delete should be POST\"}");
		REPLY_BADMETHOD(request, databuf);
	}


	evbuffer_free(databuf);
	++stats.total_cmds;
}

void
handler_tree_map(struct evhttp_request *request, const char *src_key, const char *template, const char *value_key, int map_from)
{
	int size;
	int ecode;
	int post_size;
	int base_key_size;

	char *rawtree;
	int rawtree_size;

	const char *elem;
	const char *elem_value;

	char *dst_key;
	char *template_prefix;
	char *template_decoded;

	struct evbuffer *databuf = evbuffer_new();

	TCTREE *src_tree = NULL;
	TCTREE *dst_tree = NULL;

	template_decoded = evhttp_decode_uri(template);
	template_prefix = strsep(&template_decoded, "%");
	base_key_size = strlen(template_prefix) + strlen(template_decoded) + 2;

	if (request->type != EVHTTP_REQ_POST) {
		evbuffer_add_printf(databuf,
			"{\"err\": \"map should be POST\"}");
		REPLY_BADMETHOD(request, databuf);
		goto end;
	}

	if (request->ntoread) {
		evbuffer_add_printf(databuf,
			"{\"err\": \"Not enough POST data\"}");
		REPLY_BADMETHOD(request, databuf);
		goto end;
	}

	post_size = EVBUFFER_LENGTH(request->input_buffer);
	if (!post_size) {
		evbuffer_add_printf(databuf,
			"{\"err\": \"Size of POST data must be > 0\"}");
		REPLY_BADMETHOD(request, databuf);
		goto end;
	}

	rawtree = tchdbget(db, src_key, strlen(src_key), &rawtree_size);

	if (!rawtree) {
		ecode = tchdbecode(db);

		if (ecode != TCENOREC) {
			evbuffer_add_printf(databuf,
				"{\"msg\": \"%s\"}", tchdberrmsg(ecode));
			REPLY_INTERR(request, databuf);
			goto end;
		}

		evbuffer_add_printf(databuf, "true");
		REPLY_OK(request, databuf);
		
		goto end;
	}
	
	src_tree = tctreeload(rawtree, rawtree_size, SWORDFISH_KEY_CMP, NULL);
	free(rawtree);

	tctreeiterinit(src_tree);

	while ((elem = tctreeiternext2(src_tree)))
	{
		/* Determine value to be appended to the prefix to determine
		   the target tree. */
		elem_value = (map_from == RESULT_VALUES) ?
			tctreeget2(src_tree, elem) : elem;

		dst_key = (char *)malloc(base_key_size + strlen(elem_value));
		sprintf(dst_key, "%c%s%s%s", TYPE_TREE, template_prefix, elem_value,
			template_decoded);

		rawtree = tchdbget(db, dst_key, strlen(dst_key), &rawtree_size);

		if (rawtree) {
			dst_tree = tctreeload(rawtree, rawtree_size, SWORDFISH_KEY_CMP, NULL);
			free(rawtree);
		} else {
			ecode = tchdbecode(db);

			if (ecode != TCENOREC) {
				free(dst_key);
				tctreedel(src_tree);
				tctreedel(dst_tree);

				evbuffer_add_printf(databuf,
					"{\"msg\": \"%s\"}", tchdberrmsg(ecode));
				REPLY_INTERR(request, databuf);
				goto end;
			}

			dst_tree = tctreenew2(SWORDFISH_KEY_CMP, NULL);
		}

		tctreeput(dst_tree, value_key, strlen(value_key),
			EVBUFFER_DATA(request->input_buffer), post_size);

		rawtree = tctreedump(dst_tree, &size);

		if (!tchdbput(db, dst_key, strlen(dst_key), rawtree, size)) {
			ecode = tchdbecode(db);

			free(rawtree);
			free(dst_key);
			tctreedel(src_tree);
			tctreedel(dst_tree);

			evbuffer_add_printf(databuf,
				"{\"msg\": \"%s\"}", tchdberrmsg(ecode));
			REPLY_INTERR(request, databuf);
			goto end;
		}

		free(dst_key);
		free(rawtree);
		tctreedel(dst_tree);
	}

	tctreedel(src_tree);

	evbuffer_add_printf(databuf, "true");
	REPLY_OK(request, databuf);

end:
	free(template_decoded);
	evbuffer_free(databuf);
	++stats.total_cmds;
}

int
get_values_value(struct evkeyvalq *querystr)
{
	const char *c = evhttp_find_header(querystr, "values");

	if (!c)
		return RESULT_ALL;

	if (strcmp(c, "values") == 0)
		return RESULT_VALUES;

	if (strcmp(c, "keys") == 0)
		return RESULT_KEYS;

	return RESULT_ALL;
}

int
lookup_resource(const char *resource)
{
	int i;
	int num_cmds = sizeof(resource_lookup_table) / sizeof(*resource_lookup_table);

	if (!resource)
		return RESOURCE_NONE;

	for (i = 0; i != num_cmds; ++i) {
		if (strcmp(resource, resource_lookup_table[i]) == 0)
			return i;
	}

	return RESOURCE_UNKNOWN;
}

int
get_int_header(struct evkeyvalq *querystr, const char *header, int def)
{
	const char *c;
	
	c = evhttp_find_header(querystr, header);

	return (c && *c) ? atoi(c) : def;
}

char *
get_typed_key(char resource_type, const char *uri)
{
	int i;
	char c;

	int j = 0;
	char *result = (char *)malloc(strlen(uri) + 2);

	result[j++] = resource_type;

	for (i = 0; uri[i] != '\0'; i++) {
		c = uri[i];
		if (c == '%' && isxdigit((unsigned char)uri[i+1]) &&
		    isxdigit((unsigned char)uri[i+2]))
		{
			char tmp[] = { uri[i+1], uri[i+2], '\0' };
			c = (char)strtol(tmp, NULL, 16);
			i += 2;
		}
		result[j++] = c;
	}

	result[j] = '\0';

	return result;
}

void
request_handler(struct evhttp_request *request, void *arg)
{
	char *uri;
	char *saveptr = NULL;

	char *database = NULL;
	char *arg_1 = NULL;
	char *arg_2 = NULL;
	char *arg_3 = NULL;

	struct evkeyvalq querystr;
	struct evbuffer *databuf = evbuffer_new();

	TAILQ_INIT(&querystr);

	uri = strdup(request->uri);

	/* parse query string and then strip it off */
	evhttp_parse_query(uri, &querystr);
	strtok(uri, "?");

	switch (lookup_resource(strtok_r(uri, "/", &saveptr))) {

	case RESOURCE_SYNC:
		handler_sync(request);
		break;

	case RESOURCE_NONE:
	case RESOURCE_STATS:
		handler_stats(request);
		break;

	case RESOURCE_DATABASE:
		database = strtok_r(NULL, "/", &saveptr);
		if (!database) {
			/* no database specified */
			goto notfound;
		}

		if ((db_name == NULL) || strcmp(database, db_name) != 0) {
#ifdef DEBUG
			printf("Switching database from \"%s\" => \"%s\"\n",
				db_name ? db_name : "(none)", database);
#endif

			if (db != NULL)
				tchdbdel(db);

			db = tchdbnew();

			if (!tchdbopen(db, database, HDBOWRITER | HDBOCREAT)) {
				int ecode = tchdbecode(db);

#ifdef DEBUG
				printf("Could not open database \"%s\": %s\n",
					database, tchdberrmsg(ecode));
#endif

				evbuffer_add_printf(databuf,
					"{\"msg\": \"%s\"}", tchdberrmsg(ecode));
				REPLY_INTERR(request, databuf);

				goto end;
			}

			if (db_name != NULL)
				free(db_name);

			db_name = (char*)malloc(strlen(database) + 1);
			strcpy(db_name, database);
		}

		switch (lookup_resource(strtok_r(NULL, "/", &saveptr))) {
		case RESOURCE_STATS:
			handler_stats_database(request);
			break;

		case RESOURCE_COUNTERS:
			arg_1 = strtok_r(NULL, "/", &saveptr);
			if (!arg_1) {
				/* no count specified */
				goto notfound;
			}

			arg_1 = get_typed_key(TYPE_COUNT, arg_1);

			switch (request->type) {
			case EVHTTP_REQ_POST:
				handler_counter_set(request, arg_1);
				break;
			default:
				handler_counter_get(request, arg_1);
			}
			break;

		case RESOURCE_TREES:
			arg_1 = strtok_r(NULL, "/", &saveptr);
			if (!arg_1) {
				/* no tree specified */
				goto notfound;
			}

			arg_1 = get_typed_key(TYPE_TREE, arg_1);

			switch (lookup_resource(strtok_r(NULL, "/", &saveptr))) {

			case RESOURCE_NONE:
				handler_tree_get(request, arg_1,
					get_values_value(&querystr),
					get_int_header(&querystr, "skip", 0),
					get_int_header(&querystr, "limit", 0));
				break;

			case RESOURCE_ITEM:
				if ((arg_2 = strtok_r(NULL, "/", &saveptr)) == NULL)
					goto notfound;

				switch (request->type) {
				case EVHTTP_REQ_POST:
					handler_tree_set_item(request, arg_1, arg_2);
					break;
				default:
					handler_tree_get_item(request, arg_1, arg_2);
				}
				break;

			case RESOURCE_COUNT:
				handler_tree_get(request, arg_1, RESULT_COUNT,
					get_int_header(&querystr, "skip", 0),
					get_int_header(&querystr, "limit", 0));
				break;

			case RESOURCE_DELETE:
				/* delete tree `tree` */
				handler_tree_delete(request, arg_1);
				break;

			case RESOURCE_INTERSECTION:
				/* return intersection of `tree` and `arg_2` */
				if ((arg_2 = strtok_r(NULL, "/", &saveptr)) == NULL)
					goto notfound;

				arg_2 = get_typed_key(TYPE_TREE, arg_2);

				switch (lookup_resource(strtok_r(NULL, "/", &saveptr))) {
				case RESOURCE_NONE:
					handler_tree_intersection(request, arg_1, arg_2,
						get_values_value(&querystr),
						get_int_header(&querystr, "skip", 0),
						get_int_header(&querystr, "limit", 0));
					break;

				case RESOURCE_COUNT:
					handler_tree_intersection(request, arg_1, arg_2,
						RESULT_COUNT, 0, -1);
					break;
					
				default:
					/* unknown subcommand */
					free(arg_2);
					goto notfound;
				}
				free(arg_2);
				break;

			case RESOURCE_DIFFERENCE:
				/* return difference between `tree` and `arg_2` */
				if ((arg_2 = strtok_r(NULL, "/", &saveptr)) == NULL)
					goto notfound;

				arg_2 = get_typed_key(TYPE_TREE, arg_2);

				switch (lookup_resource(strtok_r(NULL, "/", &saveptr))) {
				case RESOURCE_NONE:
					handler_tree_difference(request, arg_1, arg_2,
						get_values_value(&querystr),
						get_int_header(&querystr, "skip", 0),
						get_int_header(&querystr, "limit", 0));
					break;

				case RESOURCE_COUNT:
					handler_tree_difference(request, arg_1, arg_2,
						RESULT_COUNT, 0, -1);
					break;
					
				default:
					/* unknown subcommand */
					free(arg_2);
					goto notfound;
				}
				free(arg_2);
				break;

			case RESOURCE_MAP:
				/* map contents of `tree` using template `arg_2` */
				if ((arg_2 = strtok_r(NULL, "/", &saveptr)) == NULL)
					goto notfound;

				if ((arg_3 = strtok_r(NULL, "/", &saveptr)) == NULL)
					goto notfound;

				handler_tree_map(request, arg_1, arg_2, arg_3, get_values_value(&querystr));
				
				break;

			default:
				/* unknown subcommand */
				goto notfound;
			}

			break;

		default:
			/* unknown sub-database entity */
			goto notfound;
		}

		break;

	default:
		/* unknown command */
		goto notfound;
	}

	goto end;

notfound:
	REPLY_NOTFOUND(request, databuf);

end:
	free(uri);
	if (arg_1)
		free(arg_1);
	evbuffer_free(databuf);
	evhttp_clear_headers(&querystr);
}

void
usage(const char *progname)
{
	fprintf(stderr,
		"%s [..options..]\n"
		"\t -d datadir     data directory\n"
		"\t -i interface   interface to run server on\n"
		"\t -P port        port number to run server on\n"
		"\t -p pidfile     daemonise and write pid to <pidfile>\n"
		"\t -v             enable verbose mode\n",
		progname);
}

void
stats_init(void)
{
	stats.started = time(NULL);
	stats.total_cmds = 0;
}

void
exit_handler(void)
{
	int ecode;

	if ((db != NULL)) {
		
		if (!tchdbclose(db)) {
			ecode = tchdbecode(db);
			fprintf(stderr, "tchdbdel: %s\n", tchdberrmsg(ecode));
			exit(EXIT_FAILURE);
		}

		tchdbdel(db);
	}

	if (config.pidfile)
		unlink(config.pidfile);

	_exit(EXIT_SUCCESS);
}

static void
sig_handler(const int sig)
{
	if (sig != SIGTERM && sig != SIGQUIT && sig != SIGINT)
		return;

	fprintf(stderr, "Caught signal %d, exiting...\n", sig);

	exit_handler();

	exit(EXIT_SUCCESS);
}

int
main(int argc, char** argv)
{
	int ch;
	int ecode;
	struct evhttp *http_server = NULL;

	/* set defaults */
	config.host = "127.0.0.1";
	config.port = 2929;
	config.datadir = ".";
	config.pidfile = NULL;

	while ((ch = getopt(argc, argv, "p:P:d:")) != -1)
		switch(ch) {
		case 'p':
			config.pidfile = optarg;
			break;
		case 'P':
			config.port = atoi(optarg);
			if (!config.port) {
				usage(argv[0]);
				return EXIT_FAILURE;
			}
			break;
		case 'h':
			config.host = optarg;
			break;
		case 'd':
			config.datadir = optarg;
			break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}

	event_init();
	stats_init();

	if (chdir(config.datadir) == -1) {
		perror("Could not chdir to datadir");
		return EXIT_FAILURE;
	}

	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if ((sigemptyset(&sa.sa_mask) == -1) || (sigaction(SIGPIPE, &sa, 0) == -1)) {
		perror("failed to ignore SIGPIPE; sigaction");
		return EXIT_FAILURE;
	}

	if (signal(SIGTERM, sig_handler) == SIG_ERR)
		fprintf(stderr, "Can not catch SIGTERM\n");
	if (signal(SIGQUIT, sig_handler) == SIG_ERR)
		fprintf(stderr, "Can not catch SIGQUIT\n");
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		fprintf(stderr, "Can not catch SIGINT\n");

	if (atexit(exit_handler)) {
		fprintf(stderr, "Could not register atexit(..)\n");
		return EXIT_FAILURE;
	}

	if ((http_server = evhttp_start(config.host, config.port)) == NULL) {
		fprintf(stderr,
			"Cannot listen on http://%s:%d/; exiting..\n",
			config.host, config.port);
		return EXIT_FAILURE;
	}

	evhttp_set_gencb(http_server, request_handler, NULL);

	if (config.pidfile) {
		FILE *fp;
		int fd;

		switch (fork()) {
		case -1:
			perror("fork");
			return EXIT_FAILURE;
		case 0:
			break;
		default:
			return EXIT_SUCCESS;
		}	

		if (setsid() == -1) {
			perror("setsid");
			return EXIT_FAILURE;
		}

		fd = open("/dev/null", O_RDWR, 0);
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		close(fd);

		fp = fopen(config.pidfile, "w");
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}

	fprintf(stderr, "Listening on http://%s:%d/ ...\n",
		config.host, config.port);

	event_dispatch();

	return EXIT_SUCCESS;
}
