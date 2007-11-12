from django.db import models
from django.conf import settings

class Wikipage(models.Model):
    """Wiki page storage"""
    title = models.CharField(maxlength=30)
    content = models.TextField()

    def editurl(self):
        return settings.WIKI_SITEBASE + "edit/" + self.title + "/"

    def __repr__(self):
        return self.title
    class Admin:
        list_display=('title',)
