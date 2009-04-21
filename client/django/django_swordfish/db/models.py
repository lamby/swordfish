# -*- coding: UTF8 -*-
#
# Swordfish database
# Copyright (C) 2009 UUMC, Ltd. <chris@playfire.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import httplib

from copy import deepcopy
from urllib import quote, urlencode

from django.conf import settings
from django.utils import simplejson
from django.core.exceptions import ImproperlyConfigured

from django_swordfish.utils import SwordfishError

REPR_OUTPUT_SIZE = 20

__all__ = ('Database', 'Tree', 'TreeIntersection')

def make_call(path, method='GET', data=None):
    try:
        conn = httplib.HTTPConnection(settings.SWORDFISH_SERVER)
    except AttributeError:
        raise ImproperlyConfigured(
            'You must set SWORDFISH_SERVER in settings.py'
        )

    try:
        path = "/database/%s%s" % (settings.SWORDFISH_DATABASE, path)
    except AttributeError:
        raise ImproperlyConfigured(
            'You must set SWORDFISH_DATABASE in settings.py'
        )

    if method == 'POST' and data is None:
        data = ''

    headers = {}
    if data is not None:
        headers['Content-Length'] = len(data)

    conn.request(method, path, data, headers)
    response = conn.getresponse()
    assert response.status == 200, "Server status %d != 200" % response.status

    val = response.read()

    try:
        return simplejson.loads(val)
    except ValueError:
        raise SwordfishError('Error decoding JSON')

class SwordfishQuerySet(object):
    def __init__(self):
        self.low_mark = 0
        self.high_mark = None

        self._values = None
        self.model = None
        self.strict = None

        self.count_cache = None
        self.result_cache = None

    def __repr__(self):
        data = list(self[:REPR_OUTPUT_SIZE + 1])
        if len(data) > REPR_OUTPUT_SIZE:
            data[-1] = "...(remaining elements truncated)..."
        return repr(data)

    def __iter__(self):
        args = {}
        if self._values is not None:
            args['values'] = self._values

        if self.low_mark:
            args['skip'] = self.low_mark
        if self.high_mark:
            args['limit'] = self.high_mark - self.low_mark
            if not args['limit']:
                return iter([])

        uri = '%s%s' % (
            self.get_uri(),
            args and '?%s' % urlencode(args) or '',
        )

        if self.result_cache is None:
            self.result_cache = make_call(uri)['items']
        items = self.result_cache

        if self.model is None:
            return iter(items)

        data = self.model.objects.all().in_bulk(items)

        if self.strict:
            return iter(data[int(x)] for x in items)

        def non_strict():
            for x in items:
                try:
                    yield data[int(x)]
                except (KeyError, ValueError):
                    pass
        return non_strict()

    def __getitem__(self, k):
        if not isinstance(k, (slice, int, long)):
            raise TypeError
        assert ((not isinstance(k, slice) and (k >= 0))
                or (isinstance(k, slice) and (k.start is None or k.start >= 0)
                    and (k.stop is None or k.stop >= 0))), \
                "Negative indexing is not supported."

        if not isinstance(k, slice):
            return list(self[k:k+1])[0]

        sfqs = deepcopy(self)
        if k.start is not None:
            start = int(k.start)
        else:
            start = None
        if k.stop is not None:
            stop = int(k.stop)
        else:
            stop = None
        sfqs.set_limits(start, stop)

        return k.step and list(sfqs)[::k.step] or sfqs

    def __len__(self):
        return self.count()

    def count(self):
        if self.count_cache is None:
            if self.result_cache is None:
                self.count_cache = make_call(
                    self.get_count_uri()
                )['count']
            else:
                self.count_cache = len(self.result_cache)

        return self.count_cache

    def keys(self):
        assert self._values is None, \
            "Cannot call .keys() or .values() more than once."

        sfqs = deepcopy(self)
        sfqs._values = 'keys'
        return sfqs

    def values(self):
        assert self._values is None, \
            "Cannot call .keys() or .values() more than once."

        sfqs = deepcopy(self)
        sfqs._values = 'values'
        return sfqs

    def as_model(self, model, strict=True):
        assert self.model is None, "Cannot call .as_model(..) more than once"
        assert self._values is not None, \
            "One of .keys() or .values() must be called before .as_model(..)"

        sfqs = deepcopy(self)
        sfqs.model = model
        sfqs.strict = strict
        return sfqs

    def set_limits(self, low, high):
        if high is not None:
            if self.high_mark is not None:
                self.high_mark = min(self.high_mark, self.low_mark + high)
            else:
                self.high_mark = self.low_mark + high
        if low is not None:
            if self.high_mark is not None:
                self.low_mark = min(self.high_mark, self.low_mark + low)
            else:
                self.low_mark = self.low_mark + low

    def invalidate_cache(self):
        self.count_cache = None
        self.result_cache = None

class Tree(SwordfishQuerySet):
    def __init__(self, tree):
        super(Tree, self).__init__()
        self.tree = tree

    def get_uri(self):
        return '/trees/%s/' % quote(self.tree)

    def get_count_uri(self):
        return '/trees/%s/count/' % quote(self.tree)

    def set(self, key, value):
        assert self.low_mark == 0 and self.high_mark is None and \
            self._values is None and self.model is None, \
                "Must call .set() or .delete() on base Tree()"

        make_call(
            '/trees/%s/item/%s/' % (quote(self.tree), quote(str(key))),
            'POST',
            str(value),
        )
        self.invalidate_cache()

    def delete(self, key=None):
        # Just delete specified key
        if key is not None:
            return self.set(key, '')

        make_call(
            '/trees/%s/delete/' % quote(self.tree),
            'POST',
            '',
        )
        self.invalidate_cache()

    def map(self, template, key, value):
        """
        >>> my_tree.keys().map(template, key, value)

        is equivalent to:

        >>> for val in my_tree.keys():
                Tree(template.replace('%', val, 1)).set(key, value)
        """
        assert self._values is not None, \
            "map() must be called after one of keys() or values()"

        make_call(
            '/trees/%s/map/%s/%s?values=%s' % (
                quote(self.tree),
                template,
                quote(key),
                self._values,
            ),
            'POST',
            str(value),
        )

class Database(object):
    def __init__(self, database):
        self.database = database

    def __repr__(self):
        return "<Swordfish database %r>" % self.database

    def stats(self):
        return make_call('/stats')

    def delete(self):
        return make_call('/delete', method='POST')

class TreeCommand(SwordfishQuerySet):
    def __init__(self, left_tree, right_tree):
        super(TreeCommand, self).__init__()
        self.left_tree = left_tree
        self.right_tree = right_tree

    def get_uri(self):
        return '/trees/%s/%s/%s/' % \
            (quote(self.left_tree), self.COMMAND, quote(self.right_tree))

    def get_count_uri(self):
        return '/trees/%s/%s/%s/count/' % \
            (quote(self.left_tree), self.COMMAND, quote(self.right_tree))

class TreeIntersection(TreeCommand):
    COMMAND = 'intersection'

class TreeDifference(TreeCommand):
    COMMAND = 'difference'
