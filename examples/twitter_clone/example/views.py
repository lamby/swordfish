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

from django.db.models import Q
from django.template import RequestContext
from django.core.paginator import Paginator
from django.contrib.auth.models import User
from django.http import HttpResponseRedirect
from django.shortcuts import render_to_response, get_object_or_404

from django_swordfish.db import Tree, TreeIntersection, TreeDifference

from models import Message
from forms import MessageForm

def index(request):
    return render_to_response('index.html', {
        'users': User.objects.all(),
    }, context_instance=RequestContext(request))

def user_page(request, username):
    user = get_object_or_404(User, username=username)

    # Use MySQL to get the User objects out via JOINs
    following = User.objects.filter(following__dst=user)
    followers = User.objects.filter(followers__src=user)

    # Alternatively, use .as_model() to get the User objects out
    following = Tree('followed-by-%s' % user).values().as_model(User)
    followers = Tree('follower-of-%s' % user).values().as_model(User)

    # Or (better still) as our URLs are deterministic, just get the keys out
    following = Tree('followed-by-%s' % user).keys()
    followers = Tree('follower-of-%s' % user).keys()

    msgs = user.messages.all()

    return render_to_response('user.html', {
        'user': user,
        'users': User.objects.exclude(pk=user.pk),
        'following': following,
        'followers': followers,
        'msgs': Paginator(msgs, 10).page(request.GET.get('page', 1)),
    }, context_instance=RequestContext(request))

def logged_in(request, username):
    user = get_object_or_404(User, username=username)

    if request.method == 'POST':
        form = MessageForm(request.POST)

        if form.is_valid():
            Message.objects.create(
                user=user,
                msg=form.cleaned_data['msg'],
            )
            return HttpResponseRedirect('?')
    else:
        form = MessageForm()

    # Use MySQL to get the Message objects out via JOIN and DISTINCT
    msgs = Message.objects.filter(
        Q(user=user) | Q(user__following__dst=user)
    ).distinct()

    # Alternatively, use .as_model() to get the User objects out
    msgs = Tree('messages-for-%s' % user).values().as_model(Message)

    return render_to_response('logged_in.html', {
        'user': user,
        'form': form,
        'msgs': Paginator(msgs, 10).page(request.GET.get('page', 1)),
    }, context_instance=RequestContext(request))

def intersection(request):
    a = get_object_or_404(User, username=request.GET.get('a', ''))
    b = get_object_or_404(User, username=request.GET.get('b', ''))

    # MySQL intersection (double-JOIN)
    users = User.objects.filter(followers__src=a) & \
        User.objects.filter(followers__src=b)

    # Alternatively, use Swordfish
    users = TreeIntersection('followed-by-%s' % a, 'followed-by-%s' % b)

    return render_to_response('intersection.html', {
        'a': a,
        'b': b,
        'users': Paginator(users, 10).page(request.GET.get('page', 1)),
    }, context_instance=RequestContext(request))

def difference(request):
    a = get_object_or_404(User, username=request.GET.get('a', ''))
    b = get_object_or_404(User, username=request.GET.get('b', ''))

    # MySQL/Python difference (memory-bound, requires in-Python sorting)
    users = list(set(User.objects.filter(followers__src=a)) - \
        set(User.objects.filter(followers__src=b)))
    users.sort()

    # Alternatively, use Swordfish
    users = TreeDifference('followed-by-%s' % a, 'followed-by-%s' % b)

    return render_to_response('difference.html', {
        'a': a,
        'b': b,
        'users': Paginator(users, 10).page(request.GET.get('page', 1)),
    }, context_instance=RequestContext(request))
