# mosaic
Exploring minimum resolution images.

## What does it do?
Mosaic is a tool for quantizing RBG images to 16x16 resolution. It can be run as a web app for users to upload images. It can also be run as a REST API to recieved encoded JPEG/PNG images.

mosaic.c :

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
