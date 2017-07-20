# PDFEnabler
Integrate powerful PDF manipulation tools into just about anything

First, see the comments on the Common repository, to recap:
    1. Clone them all
    2. ... in a place where they are siblings
    
PDFEnabler is a COM object. One significant aspect of this project is that it demonstrates the power of RAW COM, this time without 
ALL the CRUD associated with MFC or ATL implementations of the same.

To see COM in it's utter simplicity, look here. Otherwise, you can rely on MFC or ATL - and be forever stuck in a) a restrictive 
"one size fits all" strategy that forces your hand in every thing you do (and which is poorly written, as you can see if you debug 
into it's code), or b) swim in a sea of inane macro definitions that you have to somehow get exactly right. God forbid, you need 
to extend you're capabilities, IF you can find the right macro, then MAYBE you can see what you need to do.

Have confidence in your understanding of how C++ works - you are <i>just</i> as good at what you do as those that put these tools 
right smack in your way. Just because MS or DB isn't your employer, does NOT mean that you can't do better than them. And, from what
I've seen, <i>anybody</i> can do better than that.

So, the PdfEnabler project:

The first step is to (of course), instantiate the COM object, CoCreateInstance or your method of choice.

Then, you want to get an overall IPdfDocument interface, which needs the name of the pdf file.

From here, you can do a lot of stuff from the Document AND the page level.

Highlights:

    Document
       Get Pages
       Add Streams
       Remove streams
       Parse the text
       Extract the fonts
       Add pages
       
    Page
       Rotate
       Add Image
       Parset Text
       Add binary objects
       
Best approach is too look at the interfaces in the '.odl file. 

This particular project has proven very useful in the CursiVision system. In general, it is a powerful connection between your software,
and the manipulation of PDF files. There is nothing you can't do in terms of managing PDF with this framework. Granted, it may not currently
offer exactly the particular thing you want, but you should be able to clearly see the patterns with which this component was developed,
and through that - and with the supporting functionality already there - add PDF functionality with a minimum of effort.


