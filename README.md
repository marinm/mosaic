# mosaic
Playing with minimum resolution images.

Server and browser/client code.


## What does it do?
Mosaic is a tool for pixelating images. It can be run as a web app for users to upload images. It can also be run as a REST API to recieve encoded PNG images.

Send a request string to https://marinm.net/cgi/mosaic.cgi

#### REQUEST STRING
```
{"file":"..."}
```
Where `...` is a base64-encoded JPEG or PNG file.

#### RESPONSE STRING
```
{"palette":"..."}
```

Where `...` is a base64-encoded PNG file.

The "palette" file is a small 16x16 thumbnail where each pixel is an entry in the quantized colour space for the request image.
