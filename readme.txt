libcurl shipped with matlab does NOT support SSL and hence not https.

To SOLVE the issue:

Redirect MATLAB/R2013a/bin/glnxa64/libcurl.so.4
to /usr/lib/x86_64-linux-gnu/libcurl.so.4
