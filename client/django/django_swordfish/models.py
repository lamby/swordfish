from django.core.signals import request_started

import django_swordfish

def reset_queries(**kwargs):
    django_swordfish.queries = []

request_started.connect(reset_queries)
