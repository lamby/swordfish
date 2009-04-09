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

import random
import datetime

from django.contrib.auth.models import User
from django.core.management.base import NoArgsCommand

from django_swordfish.db import Tree
from twitter_clone.example.models import Following, Message

USERNAMES = (
    'alice',
    'bob',
    'carol',
    'dave',
    'eve',
    'isaac',
    'ivan',
    'justin',
    'mallory',
    'matilda',
    'oscar',
    'pat',
    'plod',
    'steve',
    'trent',
    'trudy',
    'walter',
    'zoe',
)

MSGS = (
    'Oh hi, I upgraded your RAM.',
    'Monorail cat has left the station',
    'Happycat has run out of happy',
)

class Command(NoArgsCommand):
    def handle_noargs(self, **options):
        users = []

        User.objects.all().delete()
        Following.objects.all().delete()
        Message.objects.all().delete()

        users = [User.objects.create(username=x) for x in USERNAMES]

        for user in users:
            Tree('messages-for-%s' % user).delete()
            Tree('followed-by-%s' % user).delete()
            Tree('follower-of-%s' % user).delete()

        for user in users:
            for u in random.sample(users, 3):
                if u == user:
                    continue

                Following.objects.create(
                    src=user,
                    dst=u,
                )

        for user in users:
            for x in range(random.randrange(5, 15)):
                Message.objects.create(
                    user=user,
                    msg=random.choice(MSGS),
                    date=datetime.datetime.now() -
                        datetime.timedelta(minutes=random.randrange(1, 100))
                )
