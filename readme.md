# baghand
A simple command-line tool for converting between zip format and 
tarballs of gzipped files, accomplished without any compressing or 
decompressing.

# naming
Baghand is stupidly named, this is intentional. The name comes from the 
fact that "zip" is (in my opinion) a terrible name. Zip and compression 
have become almost synonymous, but zip was named after the "zipper", 
which is a device for keeping a opening closed, it does not perform any 
compression, and if it is used for compression, is likely burst, making 
it a poor choice for such a task, and a silly name. If you think about 
how one might pack luggage for a long trip, the contents of the bag are 
compressed to fit, but the zipper does perform the compression, instead 
you would use your hand to compress the items into the bag before using 
the zipper to close the bag.
Baghand is named after the tool that actually does the compression, 
instead of the closure mechanism.
That being said, however, baghand does not actually do compression or 
decompression of files. Instead it changes the packaging around the 
compressed data from zip to tar. Thus rendering the name and analogy 
incorrect, however, the name baghand was chosen primarily because it 
serves to poke fun at the naming of zip (and derivatives).
However, it does not serve this purpose well either. A solid argument 
could be made in favor of zip (but not gzip), since the actual 
compression algorithm is actually called "deflate", and the zip file is 
arguably just a container for deflated data. If one were to accept this 
argument, zip actually becomes an appropriate name for the format.
That being said, I like reject all arguments. Baghand is the proper 
name for this tool, and zip is a stupid name.
