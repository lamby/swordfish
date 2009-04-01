from django.conf.urls.defaults import *

urlpatterns = patterns('example.views',
    url(r'^$', 'index', name='index'),
    url(r'intersection$', 'intersection', name='intersection'),
    url(r'(?P<username>[^\/]+)/home$', 'logged_in', name='logged-in'),
    url(r'(?P<username>[^\/]+)$', 'user_page', name='user-page'),
)
