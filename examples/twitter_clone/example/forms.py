from django import forms

import models

class MessageForm(forms.Form):
    msg = forms.CharField(widget=forms.widgets.Textarea())

    class Meta:
        model = models.Message
