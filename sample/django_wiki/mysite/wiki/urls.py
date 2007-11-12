from django.conf.urls.defaults import patterns

urlpatterns = patterns('mysite.wiki.views',
    (r'^((?:\w+))/$', 'page'),
    (r'^edit/((?:\w+))/$', 'edit'),
    (r'^delete/$', 'delete'),
    (r'^$', 'index'),
)
 
