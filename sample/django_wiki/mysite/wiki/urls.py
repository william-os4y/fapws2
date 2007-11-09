from django.conf.urls.defaults import patterns

urlpatterns = patterns('mysite.wiki.views',
    (r'^((?:[A-Z]+[a-z]+))/$', 'page'),
    (r'^edit/((?:[A-Z]+[a-z]+))/$', 'edit'),
    (r'^delete/$', 'delete'),
    (r'^$', 'index'),
)
 
