from mysite.wiki.models import Wikipage

data=Wikipage(title="HelloWorld", content="This is the famous Hello World!!!")
data.save()
