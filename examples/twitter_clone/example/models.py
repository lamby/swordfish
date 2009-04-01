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

    def save(self, *args, **kwargs):
        self.username = self.user.username

        super(Message, self).save(*args, **kwargs)

        key = '%0.10d-%d' % (
            2**31 - time.mktime(self.date.timetuple()),
            self.pk,
        )

        Tree('messages-for-%s' % self.user).set(key, self.pk)
        for username in Tree('follower-of-%s' % self.user).keys():
            Tree('messages-for-%s' % username).set(key, self.pk)
