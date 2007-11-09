from django.http import HttpResponse, HttpResponseRedirect
from django.shortcuts import render_to_response
from mysite.wiki.models import Wikipage

def index(request):
    """Return simple list of wiki pages"""
    pages = Wikipage.objects.all().order_by('title')
    return render_to_response('home.html', locals())

def page(request, title):
    """Display page, or redirect to root if page doesn't exist yet"""
    try:
        page = Wikipage.objects.get(title__exact=title)
        return render_to_response('page.html', locals())
    except Wikipage.DoesNotExist:
        return HttpResponseRedirect("/wiki/edit/%s/" % title)

def edit(request, title):
    """Process submitted page edits (POST) or display editing form (GET)"""
    if request.POST:
        try:
            page = Wikipage.objects.get(title__exact=title)
        except Wikipage.DoesNotExist:
            # Must be a new one; let's create it
            page = Wikipage(title=title)
        page.content = request.POST['content']
        page.title = request.POST['title']
        page.save()
        return HttpResponseRedirect("/wiki/" + page.title + "/")
    else:
        try:
            page = Wikipage.objects.get(title__exact=title)
        except Wikipage.DoesNotExist:
            # create a dummy page object -- note that it is not saved!
            page = Wikipage(title=title)
            page.body = "<!-- Enter content here -->"
        return render_to_response('edit.html', locals())

def delete(request):
    """Delete a page"""
    if request.POST:
        title = request.POST['title']
        try:
            page = Wikipage.objects.get(title__exact=title)
        except Wikipage.DoesNotExist:
            return HttpResponseRedirect("/wiki/DeleteFailed/")
        page.delete()
    return HttpResponseRedirect("/wiki/")
