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
