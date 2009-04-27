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

import time
import datetime

from django.db import models
from django.contrib.auth.models import User

from django_swordfish.db import Tree

class Following(models.Model):
    src = models.ForeignKey(User, related_name='following')
    dst = models.ForeignKey(User, related_name='followers')

    def save(self, *args, **kwargs):
        super(Following, self).save(*args, **kwargs)
        Tree('followed-by-%s' % self.src).set(self.dst, self.dst_id)
        Tree('follower-of-%s' % self.dst).set(self.src, self.src_id)

class Message(models.Model):
    user = models.ForeignKey(User, related_name='messages')
    username = models.CharField(max_length=15)
    msg = models.CharField(max_length=140)
    date = models.DateTimeField(default=datetime.datetime.now)

    class Meta:
        ordering = ('-date',)

    def key(self):
        return '%0.10d-%d' % (
            2**31 - time.mktime(self.date.timetuple()),
            self.pk,
        )

    def save(self, *args, **kwargs):
        created = not self.id

        if created:
            self.username = self.user.username

        super(Message, self).save(*args, **kwargs)

        if created:
            key = self.key()

            # Add the message to the user's message tree.
            Tree('messages-for-%s' % self.user).set(key, self.pk)

            # Use the follower-of- tree for this user to add the message their
            # followers' trees.
            #
            # This step could be done asynchronously if required - the above call
            # could still be made sychronously to maintain the illusion.
            Tree('follower-of-%s' % self.user).keys().map('messages-for-%', key, self.pk)

    def delete(self, *args, **kwargs):
        key = self.key()

        # Remove the message to the user's message tree.
        Tree('messages-for-%s' % self.user).delete(key)

        # Use the follower-of- tree for this user to remove the message their
        # followers' trees.
        #
        # Like the call in save(), this step could be done asynchronously.
        Tree('follower-of-%s' % self.user).keys().map('messages-for-%', key)

        super(Message, self).delete(*args, **kwargs)
