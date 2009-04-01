from django.db.models import Q
from django.template import RequestContext
from django.core.paginator import Paginator
from django.contrib.auth.models import User
from django.http import HttpResponseRedirect
from django.shortcuts import render_to_response, get_object_or_404

from django_swordfish.db import Tree, TreeIntersection

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
