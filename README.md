# PDFEnabler
Integrate powerful PDF manipulation tools into just about anything

First, see the comments on the Common repository, to recap:
    1. Clone Common
    2. In a place where they are siblings
    3. Pay attention to the use of the GSYSTEM_HOME environment variable described in the Common repository
    
PDFEnabler is a COM object. One significant aspect of this project is that it demonstrates the power of RAW COM, this time without 
ALL the CRUD associated with MFC or ATL implementations of the same.

The first step is to (of course), instantiate the COM object, using CoCreateInstance or your method of choice.

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
and through that - and with the supporting functionality already there - add the desired PDF functionality with a minimum of effort.


